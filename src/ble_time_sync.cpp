#include "ble_time_sync.h"

#include <NimBLEDevice.h>
// The short "host/ble_gatt.h" form only resolves under CONFIG_NIMBLE_CPP_IDF
// (ESP-IDF component builds) -- see NimBLEUtils.h. Plain Arduino builds only
// get the library's src/ root on the include path, so these need the full
// relative path from there.
#include "nimble/nimble/host/include/host/ble_gatt.h"
#include "nimble/nimble/host/include/host/ble_hs_mbuf.h"

#include "time_sync.h"

namespace {
    constexpr uint16_t CTS_SERVICE_UUID = 0x1805;
    constexpr uint16_t CTS_CURRENT_TIME_CHAR_UUID = 0x2A2B;
    constexpr uint16_t CTS_LOCAL_TIME_CHAR_UUID = 0x2A0F;
    constexpr uint32_t BOND_TIMEOUT_MS = 15000; // covers Bonding + Reading

    NimBLEServer *server = nullptr;
    volatile BleTimeSync::State currentState = BleTimeSync::State::Idle;
    uint32_t stateEnteredMs = 0;

    // Written from the NimBLE host task (onConnect/onDisconnect), read from
    // the Arduino task -- including stop()'s wait loop, which would spin the
    // full timeout if the compiler hoisted a non-volatile read out of it.
    volatile uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;

    // Set by the discovery/read chain (NimBLE host task); consumed once by
    // poll() (Arduino loop task) so TimeSync is only ever touched from one
    // thread.
    volatile time_t pendingUnixTime = 0;
    volatile bool timeApplied = true;

    uint16_t ctsStartHandle = 0;
    uint16_t ctsEndHandle = 0;
    uint16_t ctsValHandle = 0;
    uint16_t ctsLocalTimeValHandle = 0; // 0 if the phone doesn't expose it

    // The phone's reported wall-clock time, held between the Current Time
    // read and the (possibly separate) Local Time Information read so both
    // can be combined once the offset is known.
    struct tm pendingLocalTm = {};

    BleTimeSync::OffsetSource offsetSourceUsed = BleTimeSync::OffsetSource::Unknown;

    void setState(BleTimeSync::State s) {
        currentState = s;
        stateEnteredMs = millis();
    }

    // BLE_UUID16_DECLARE takes the address of an unnamed temporary, which
    // is a GNU C extension -- errors out under this toolchain's C++ mode
    // ([-fpermissive]). A named local sidesteps it.
    ble_uuid_t *uuid16(uint16_t v, ble_uuid16_t &storage) {
        storage.u.type = BLE_UUID_TYPE_16;
        storage.value = v;
        return &storage.u;
    }

    // Manual proleptic-Gregorian day count (no timegm/mktime -- both are
    // either non-portable or timezone-dependent, and this is small enough
    // to just get right directly). Verified against known reference dates
    // including the 1900/2000/2100 leap-year edge cases.
    time_t tmToEpochUtc(const struct tm &t) {
        static const int cumDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int year = t.tm_year + 1900;
        int month = t.tm_mon; // 0-11

        int64_t days = 365LL * (year - 1970) + (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
        days += cumDays[month];
        bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        if (month > 1 && leap) days += 1;
        days += t.tm_mday - 1;

        return (time_t)(days * 86400LL + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec);
    }

    // Combines the local time already parsed into pendingLocalTm with an
    // offset and publishes the result -- the shared tail end of both the
    // detected-offset and manual-offset paths below.
    void applyLocalTime(int offsetMinutes, BleTimeSync::OffsetSource source) {
        pendingUnixTime = tmToEpochUtc(pendingLocalTm) - (time_t)offsetMinutes * 60;
        offsetSourceUsed = source;
        timeApplied = false;
        Serial.printf("BLE: phone reported local time %04d-%02d-%02d %02d:%02d:%02d, using %s offset %+d min\n",
                      pendingLocalTm.tm_year + 1900, pendingLocalTm.tm_mon + 1, pendingLocalTm.tm_mday,
                      pendingLocalTm.tm_hour, pendingLocalTm.tm_min, pendingLocalTm.tm_sec,
                      source == BleTimeSync::OffsetSource::Detected ? "detected" : "manual", offsetMinutes);
        setState(BleTimeSync::State::Success);
    }

    void applyManualOffset() {
        applyLocalTime(TimeSync::utcOffsetMinutes(), BleTimeSync::OffsetSource::Manual);
    }

    // CTS Local Time Information (0x2A0F): sint8 time zone in 15-minute
    // units (-48..56, 0x80 = "not known"), followed by a uint8 DST offset
    // enum whose values (0, 2, 4, 8) happen to equal that same field's
    // added minutes / 15 (0, 30, 60, 120 -- i.e. 0/0.5/1/2 hours; 0xFF =
    // "not known"). Optional -- not every phone exposes it, which is why
    // this is only ever attempted as a improvement over the manual
    // Settings > Time Zone offset, never a hard requirement.
    int readLocalTimeCb(uint16_t, const ble_gatt_error *error, ble_gatt_attr *attr, void *) {
        uint8_t buf[2];
        uint16_t len = sizeof(buf);
        bool gotValue = error->status == 0 && attr && attr->om && OS_MBUF_PKTLEN(attr->om) >= 2 &&
                        ble_hs_mbuf_to_flat(attr->om, buf, len, &len) == 0 && len >= 2;

        int8_t tz = gotValue ? (int8_t)buf[0] : (int8_t)0x80;
        uint8_t dst = gotValue ? buf[1] : 0xFF;

        if (!gotValue || tz == (int8_t)0x80) {
            Serial.println("BLE: phone's local time zone not known, falling back to Settings > Time Zone");
            applyManualOffset();
            return 0;
        }

        int offsetMinutes = (int)tz * 15;
        if (dst != 0xFF) offsetMinutes += (int)dst * 15;
        applyLocalTime(offsetMinutes, BleTimeSync::OffsetSource::Detected);
        return 0;
    }

    int readCb(uint16_t connHandleArg, const ble_gatt_error *error, ble_gatt_attr *attr, void *) {
        if (error->status != 0 || !attr || !attr->om) {
            Serial.printf("BLE: CTS read failed, status=%d\n", error->status);
            setState(BleTimeSync::State::Failed);
            return 0;
        }

        // CTS Current Time (0x2A2B): year u16 LE, month, day, hours,
        // minutes, seconds, day_of_week, [fractions256, adjust_reason] --
        // only the first 7 bytes matter here.
        uint8_t buf[10] = {0};
        uint16_t len = sizeof(buf);
        if (OS_MBUF_PKTLEN(attr->om) < 7 || ble_hs_mbuf_to_flat(attr->om, buf, len, &len) != 0) {
            Serial.println("BLE: CTS value too short or malformed");
            setState(BleTimeSync::State::Failed);
            return 0;
        }

        pendingLocalTm = {};
        pendingLocalTm.tm_year = (buf[0] | (buf[1] << 8)) - 1900;
        pendingLocalTm.tm_mon = buf[2] - 1;
        pendingLocalTm.tm_mday = buf[3];
        pendingLocalTm.tm_hour = buf[4];
        pendingLocalTm.tm_min = buf[5];
        pendingLocalTm.tm_sec = buf[6];

        // CTS reports the phone's local wall-clock time, not UTC. If the
        // phone also exposes Local Time Information, use its own reported
        // offset instead of the one guessed in Settings > Time Zone --
        // that guess is only a fallback for phones that don't.
        if (ctsLocalTimeValHandle != 0) {
            int rc = ble_gattc_read(connHandleArg, ctsLocalTimeValHandle, readLocalTimeCb, nullptr);
            if (rc != 0) {
                Serial.printf("BLE: failed to start local time zone read, rc=%d\n", rc);
                applyManualOffset();
            }
            return 0;
        }

        applyManualOffset();
        return 0;
    }

    int chrDiscCb(uint16_t ch, const ble_gatt_error *error, const ble_gatt_chr *chr, void *) {
        if (error->status == 0) {
            if (chr) {
                uint16_t uuid16Val = ble_uuid_u16(&chr->uuid.u);
                if (uuid16Val == CTS_CURRENT_TIME_CHAR_UUID) ctsValHandle = chr->val_handle;
                else if (uuid16Val == CTS_LOCAL_TIME_CHAR_UUID) ctsLocalTimeValHandle = chr->val_handle;
            }
            return 0;
        }
        if (error->status == BLE_HS_EDONE) {
            if (ctsValHandle == 0) {
                Serial.println("BLE: phone's CTS service has no Current Time characteristic");
                setState(BleTimeSync::State::Failed);
                return 0;
            }
            int rc = ble_gattc_read(ch, ctsValHandle, readCb, nullptr);
            if (rc != 0) {
                Serial.printf("BLE: failed to start CTS read, rc=%d\n", rc);
                setState(BleTimeSync::State::Failed);
            }
            return 0;
        }
        Serial.printf("BLE: characteristic discovery error, status=%d\n", error->status);
        setState(BleTimeSync::State::Failed);
        return 0;
    }

    int svcDiscCb(uint16_t ch, const ble_gatt_error *error, const ble_gatt_svc *service, void *) {
        if (error->status == 0) {
            if (service) {
                ctsStartHandle = service->start_handle;
                ctsEndHandle = service->end_handle;
                Serial.println("BLE: found Current Time Service on phone");
            }
            return 0;
        }
        if (error->status == BLE_HS_EDONE) {
            // The phone doesn't expose CTS at all -- a known gap on some
            // Android builds, see ble_time_sync.h.
            if (ctsStartHandle == 0) {
                Serial.println("BLE: phone does not expose Current Time Service");
                setState(BleTimeSync::State::Failed);
                return 0;
            }
            // Discovers every characteristic in the service (rather than
            // by-UUID) so a single round trip picks up both Current Time
            // and, if present, Local Time Information.
            int rc = ble_gattc_disc_all_chrs(ch, ctsStartHandle, ctsEndHandle, chrDiscCb, nullptr);
            if (rc != 0) {
                Serial.printf("BLE: failed to start characteristic discovery, rc=%d\n", rc);
                setState(BleTimeSync::State::Failed);
            }
            return 0;
        }
        Serial.printf("BLE: service discovery error, status=%d\n", error->status);
        setState(BleTimeSync::State::Failed);
        return 0;
    }

    class ServerCallbacks : public NimBLEServerCallbacks {
        void onConnect(NimBLEServer *, ble_gap_conn_desc *desc) override {
            connHandle = desc->conn_handle;
            Serial.printf("BLE: phone connected (%s)\n",
                          NimBLEAddress(desc->peer_id_addr).toString().c_str());
            setState(BleTimeSync::State::Bonding);

            int rc = NimBLEDevice::startSecurity(desc->conn_handle);
            if (rc != 0) {
                Serial.printf("BLE: startSecurity failed to start, rc=%d\n", rc);
                setState(BleTimeSync::State::Failed);
            }
        }

        void onDisconnect(NimBLEServer *, ble_gap_conn_desc *) override {
            Serial.println("BLE: phone disconnected");
            if (currentState != BleTimeSync::State::Success) {
                setState(BleTimeSync::State::Failed);
            }
            connHandle = BLE_HS_CONN_HANDLE_NONE;
        }

        void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
            Serial.printf("BLE: authentication complete -- encrypted=%d authenticated=%d bonded=%d\n",
                          desc->sec_state.encrypted, desc->sec_state.authenticated, desc->sec_state.bonded);
            if (!desc->sec_state.encrypted) {
                setState(BleTimeSync::State::Failed);
                return;
            }

            ctsStartHandle = ctsEndHandle = ctsValHandle = ctsLocalTimeValHandle = 0;
            setState(BleTimeSync::State::Reading);
            ble_uuid16_t svcUuid;
            int rc = ble_gattc_disc_svc_by_uuid(desc->conn_handle, uuid16(CTS_SERVICE_UUID, svcUuid), svcDiscCb,
                                                nullptr);
            if (rc != 0) {
                Serial.printf("BLE: failed to start CTS service discovery, rc=%d\n", rc);
                setState(BleTimeSync::State::Failed);
            }
        }
    };

    ServerCallbacks serverCallbacks;

    // True between a successful begin() and the teardown below, so the
    // stack is only ever deinit'd once no matter how many callers ask --
    // the sync completing releases the radio on its own, and backing out
    // of the screen afterwards then has nothing left to do.
    bool radioUp = false;

    // Drops the link and frees the NimBLE stack, leaving currentState
    // alone so the screen can keep showing the result that triggered this.
    void releaseRadio() {
        if (!radioUp) return;
        radioUp = false;

        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        if (advertising) advertising->stop();

        if (server && connHandle != BLE_HS_CONN_HANDLE_NONE) {
            server->disconnect(connHandle);

            // disconnect() only requests the disconnect -- it completes
            // asynchronously via onDisconnect, which is what actually
            // clears connHandle. Calling deinit() while that's still in
            // flight, or while the link is still up, tears down the
            // NimBLE stack out from under an in-progress GAP procedure
            // and crashes the device. Give it a moment to finish first.
            uint32_t start = millis();
            while (connHandle != BLE_HS_CONN_HANDLE_NONE && millis() - start < 2000) {
                delay(10);
            }
        }

        // Bonds are intentionally left in place -- deleting them here
        // forgets the phone on Token's side only, not the phone's own
        // cached keys. On the next sync attempt the phone tries to resume
        // the old bond with keys Token no longer has, encryption fails
        // instantly, and the two sides end up in a connect/disconnect loop
        // until the phone itself notices and purges its copy. Leaving
        // bonds alone means a phone that's paired once just reconnects
        // silently after that, like any other BLE accessory.
        NimBLEDevice::deinit(true);

        server = nullptr;
        connHandle = BLE_HS_CONN_HANDLE_NONE;
        Serial.println("BLE: radio released");
    }
}

void BleTimeSync::begin() {
    connHandle = BLE_HS_CONN_HANDLE_NONE;
    timeApplied = true;
    ctsStartHandle = ctsEndHandle = ctsValHandle = ctsLocalTimeValHandle = 0;
    offsetSourceUsed = OffsetSource::Unknown;

    NimBLEDevice::init("Token");
    radioUp = true; // from here on the stack needs deinit(), even if setup below fails
    NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/false, /*sc=*/true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT); // Just Works -- physical possession already gates the device

    server = NimBLEDevice::createServer();
    // serverCallbacks is statically allocated, so the server must not take
    // ownership of it -- setCallbacks defaults to deleteCallbacks=true, and
    // the destructor deinit() runs would then call delete on a non-heap
    // pointer, corrupting the heap and rebooting the device.
    server->setCallbacks(&serverCallbacks, /*deleteCallbacks=*/false);
    server->start();

    // The library's default advertising payload varies by version and
    // isn't guaranteed to include a Complete Local Name or the
    // general-discoverable flag -- iOS's Bluetooth Settings list won't
    // show a peripheral whose primary advertising packet lacks either, so
    // build that payload explicitly rather than relying on the default.
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN);
    advData.setName("Token");

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->setAdvertisementData(advData);
    // Keeps the name+flags in the primary advertising packet rather than
    // the scan response -- the primary packet is all iOS's Settings list
    // reads.
    advertising->setScanResponse(false);

    if (!advertising->start()) {
        Serial.println("BLE: advertising->start() failed");
        releaseRadio();
        setState(State::Failed);
        return;
    }
    Serial.println("BLE: advertising as \"Token\"");
    setState(State::Advertising);
}

void BleTimeSync::poll() {
    if (currentState == State::Success && !timeApplied) {
        TimeSync::setUnixTime(pendingUnixTime);
        timeApplied = true;
        // Deliberately not tearing down in the same pass: the caller
        // redraws after this returns, and releaseRadio() can spend up to
        // two seconds waiting on the disconnect. Leaving it to the next
        // poll lets "Success!" reach the screen first.
        return;
    }

    // The phone has what it came for, so drop the link rather than staying
    // paired (and re-advertising on disconnect) until the user backs out.
    if (currentState == State::Success) {
        releaseRadio();
        return;
    }

    bool awaitingPhone = currentState == State::Bonding || currentState == State::Reading;
    if (awaitingPhone && millis() - stateEnteredMs > BOND_TIMEOUT_MS) {
        setState(State::Failed);
        releaseRadio();
    }
}

BleTimeSync::State BleTimeSync::state() {
    return currentState;
}

BleTimeSync::OffsetSource BleTimeSync::lastOffsetSource() {
    return offsetSourceUsed;
}

void BleTimeSync::stop() {
    // Usually already released by poll() on a successful sync -- this is
    // the path for backing out mid-attempt, and a no-op after that.
    releaseRadio();
    setState(State::Idle);
}
