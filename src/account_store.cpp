#include "account_store.h"

#include <Preferences.h>
#include <cstring>
#include <esp_random.h>
#include <mbedtls/gcm.h>

#include "totp.h"

namespace {
    // NVS namespace and key names. Keys stay short deliberately -- NVS caps
    // them at 15 characters.
    constexpr const char *NVS_NAMESPACE = "accounts";
    constexpr const char *KEY_COUNT = "n";

    constexpr size_t IV_LEN = 12;
    constexpr size_t TAG_LEN = 16;
    // name + issuer + secret + 2 unit-separator bytes -- real-world values
    // are a fraction of this; generous headroom costs nothing but stack.
    constexpr size_t MAX_PLAINTEXT = 200;
    constexpr size_t MAX_BLOB = IV_LEN + TAG_LEN + MAX_PLAINTEXT;

    Preferences prefs;
    bool opened = false;

    AccountStore::Account entries[AccountStore::MAX_ENTRIES];
    int entryCount = 0;
    uint8_t sessionKey[PinStore::KEY_LEN];

    const AccountStore::Account EMPTY_ACCOUNT = {"", "", "", AccountStore::DEFAULT_DIGITS, AccountStore::DEFAULT_PERIOD};

    String blobKey(int i) { return "bl" + String(i); }
    String digitsKey(int i) { return "dg" + String(i); }
    String periodKey(int i) { return "pd" + String(i); }

    // Joins name/issuer/secret with a unit-separator byte (0x1F, not valid
    // in any of the three) so all three encrypt as one GCM operation under
    // one nonce, rather than three operations each needing its own.
    int serialize(const AccountStore::Account &a, uint8_t *out, size_t outCapacity) {
        String joined = a.name + "\x1F" + a.issuer + "\x1F" + a.secret;
        if (joined.length() > outCapacity) return -1;
        memcpy(out, joined.c_str(), joined.length());
        return (int)joined.length();
    }

    bool deserialize(const uint8_t *buf, size_t len, AccountStore::Account &a) {
        String joined;
        joined.reserve(len);
        for (size_t i = 0; i < len; i++) joined += (char)buf[i];

        int sep1 = joined.indexOf('\x1F');
        if (sep1 < 0) return false;
        int sep2 = joined.indexOf('\x1F', sep1 + 1);
        if (sep2 < 0) return false;

        a.name = joined.substring(0, sep1);
        a.issuer = joined.substring(sep1 + 1, sep2);
        a.secret = joined.substring(sep2 + 1);
        return true;
    }

    // Layout: [iv (IV_LEN)][tag (TAG_LEN)][ciphertext (variable)].
    bool encryptEntry(const AccountStore::Account &a, uint8_t *blob, size_t &blobLen) {
        uint8_t plaintext[MAX_PLAINTEXT];
        int ptLen = serialize(a, plaintext, sizeof(plaintext));
        if (ptLen < 0) return false;

        uint8_t *iv = blob;
        uint8_t *tag = blob + IV_LEN;
        uint8_t *ct = blob + IV_LEN + TAG_LEN;
        esp_fill_random(iv, IV_LEN);

        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, sessionKey, 256);
        int rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, (size_t)ptLen, iv, IV_LEN, nullptr, 0, plaintext,
                                            ct, TAG_LEN, tag);
        mbedtls_gcm_free(&gcm);
        if (rc != 0) return false;

        blobLen = IV_LEN + TAG_LEN + (size_t)ptLen;
        return true;
    }

    bool decryptEntry(const uint8_t *blob, size_t blobLen, const uint8_t *key, AccountStore::Account &a) {
        if (blobLen < IV_LEN + TAG_LEN) return false;

        const uint8_t *iv = blob;
        const uint8_t *tag = blob + IV_LEN;
        const uint8_t *ct = blob + IV_LEN + TAG_LEN;
        size_t ctLen = blobLen - IV_LEN - TAG_LEN;
        if (ctLen > MAX_PLAINTEXT) return false;

        uint8_t plaintext[MAX_PLAINTEXT];
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
        int rc = mbedtls_gcm_auth_decrypt(&gcm, ctLen, iv, IV_LEN, nullptr, 0, tag, TAG_LEN, ct, plaintext);
        mbedtls_gcm_free(&gcm);
        if (rc != 0) return false;

        return deserialize(plaintext, ctLen, a);
    }

    void persistAt(int i) {
        uint8_t blob[MAX_BLOB];
        size_t blobLen = 0;
        if (!encryptEntry(entries[i], blob, blobLen)) {
            Serial.println("AccountStore: failed to encrypt entry, not persisted");
            return;
        }

        prefs.putBytes(blobKey(i).c_str(), blob, blobLen);
        prefs.putUChar(digitsKey(i).c_str(), entries[i].digits);
        prefs.putUInt(periodKey(i).c_str(), entries[i].period);
    }
}

void AccountStore::begin(const uint8_t key[PinStore::KEY_LEN]) {
    memcpy(sessionKey, key, PinStore::KEY_LEN);

    opened = prefs.begin(NVS_NAMESPACE, false);
    if (!opened) {
        Serial.println("AccountStore: failed to open NVS namespace");
        return;
    }

    int savedCount = constrain(prefs.getInt(KEY_COUNT, 0), 0, MAX_ENTRIES);
    entryCount = 0;
    for (int i = 0; i < savedCount; i++) {
        size_t blobLen = prefs.getBytesLength(blobKey(i).c_str());
        if (blobLen == 0 || blobLen > MAX_BLOB) continue;

        uint8_t blob[MAX_BLOB];
        prefs.getBytes(blobKey(i).c_str(), blob, blobLen);

        AccountStore::Account a;
        if (!decryptEntry(blob, blobLen, sessionKey, a)) {
            Serial.printf("AccountStore: entry %d failed to authenticate, dropped\n", i);
            continue;
        }
        a.digits = prefs.getUChar(digitsKey(i).c_str(), DEFAULT_DIGITS);
        a.period = prefs.getUInt(periodKey(i).c_str(), DEFAULT_PERIOD);

        entries[entryCount++] = a;
    }

    Serial.printf("AccountStore: %d saved account(s)\n", entryCount);
}

bool AccountStore::add(const String &name, const String &issuer, const String &secret, uint8_t digits, uint32_t period) {
    if (!opened || name.isEmpty() || entryCount >= MAX_ENTRIES) return false;
    if (!Totp::isValidSecret(secret)) return false;

    int i = entryCount;
    entries[i] = {name, issuer, secret, digits, period};
    entryCount++;

    persistAt(i);
    prefs.putInt(KEY_COUNT, entryCount);
    return true;
}

void AccountStore::reencrypt(const uint8_t newKey[PinStore::KEY_LEN]) {
    if (!opened) return;
    memcpy(sessionKey, newKey, PinStore::KEY_LEN);
    for (int i = 0; i < entryCount; i++) persistAt(i);
}

void AccountStore::wipeAll() {
    if (!opened) opened = prefs.begin(NVS_NAMESPACE, false);
    if (opened) prefs.clear();
    entryCount = 0;
}

int AccountStore::count() {
    return entryCount;
}

const AccountStore::Account &AccountStore::at(int index) {
    if (index < 0 || index >= entryCount) return EMPTY_ACCOUNT;
    return entries[index];
}
