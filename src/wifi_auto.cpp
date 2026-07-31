#include "wifi_auto.h"

#include <WiFi.h>

#include "wifi_manager.h"
#include "wifi_store.h"

namespace {
    constexpr uint32_t SCAN_TIMEOUT_MS = 12000;
    constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    constexpr uint32_t SYNC_TIMEOUT_MS = 10000;

    WifiAuto::State currentState = WifiAuto::State::Idle;
    uint32_t stateEnteredMs = 0;

    void setState(WifiAuto::State s) {
        currentState = s;
        stateEnteredMs = millis();
    }

    void finish(WifiAuto::State result) {
        // Drops the association and powers the radio down -- the manual
        // Sync Time flow brings WiFi back up itself, so nothing is lost by
        // going dark for the rest of the session.
        WifiManager::disconnect();
        setState(result);
    }

    bool timedOut(uint32_t limitMs) {
        return millis() - stateEnteredMs > limitMs;
    }

    // Strongest in-range network that's also in the store. Ties go to the
    // stronger signal rather than the store's recency order -- a saved
    // network that's barely reachable is a worse bet than a solid one.
    int strongestKnownNetwork(int found) {
        int best = -1;
        int32_t bestRssi = INT32_MIN;

        for (int i = 0; i < found; i++) {
            if (!WifiStore::isKnown(WiFi.SSID(i))) continue;
            if (WiFi.RSSI(i) <= bestRssi) continue;
            bestRssi = WiFi.RSSI(i);
            best = i;
        }
        return best;
    }

    void handleScanning() {
        int found = WiFi.scanComplete();

        if (found == WIFI_SCAN_RUNNING) {
            if (timedOut(SCAN_TIMEOUT_MS)) {
                Serial.println("WifiAuto: scan timed out");
                WiFi.scanDelete();
                finish(WifiAuto::State::Failed);
            }
            return;
        }

        if (found < 0) {
            Serial.println("WifiAuto: scan failed");
            finish(WifiAuto::State::Failed);
            return;
        }

        int best = strongestKnownNetwork(found);
        if (best < 0) {
            Serial.println("WifiAuto: no saved network in range");
            WiFi.scanDelete();
            finish(WifiAuto::State::Failed);
            return;
        }

        String ssid = WiFi.SSID(best);
        String password;
        WifiStore::passwordFor(ssid, password);
        WiFi.scanDelete();

        Serial.printf("WifiAuto: connecting to %s\n", ssid.c_str());
        if (password.isEmpty()) {
            WiFi.begin(ssid.c_str());
        } else {
            WiFi.begin(ssid.c_str(), password.c_str());
        }
        setState(WifiAuto::State::Connecting);
    }

    void handleConnecting() {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WifiAuto: connected, starting NTP");
            WifiManager::beginNtp();
            setState(WifiAuto::State::Syncing);
            return;
        }

        if (timedOut(CONNECT_TIMEOUT_MS)) {
            // Most likely a stale saved password. The entry is deliberately
            // left in the store -- the user can overwrite it by connecting
            // manually, and dropping it here would silently lose a network
            // that failed for an unrelated reason (AP down, out of range).
            Serial.println("WifiAuto: connect timed out");
            finish(WifiAuto::State::Failed);
        }
    }

    void handleSyncing() {
        if (WifiManager::ntpArrived()) {
            finish(WifiAuto::State::Done);
            return;
        }

        if (timedOut(SYNC_TIMEOUT_MS)) {
            Serial.println("WifiAuto: NTP timed out");
            finish(WifiAuto::State::Failed);
        }
    }
}

void WifiAuto::begin() {
    if (WifiStore::count() == 0) {
        // Nothing to reconnect to -- don't power the radio up at all.
        setState(State::Idle);
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(/*async=*/true);
    setState(State::Scanning);
}

void WifiAuto::poll() {
    switch (currentState) {
        case State::Scanning: handleScanning(); break;
        case State::Connecting: handleConnecting(); break;
        case State::Syncing: handleSyncing(); break;

        case State::Idle:
        case State::Done:
        case State::Failed:
            break; // nothing in flight
    }
}

void WifiAuto::cancel() {
    bool active = currentState == State::Scanning || currentState == State::Connecting ||
                  currentState == State::Syncing;
    if (!active) return;

    if (currentState == State::Scanning) WiFi.scanDelete();
    WifiManager::disconnect();
    setState(State::Idle);
}

WifiAuto::State WifiAuto::state() {
    return currentState;
}
