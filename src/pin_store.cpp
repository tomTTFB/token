#include "pin_store.h"

#include <Preferences.h>
#include <cstring>
#include <esp_random.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

namespace {
    // NVS namespace and key names. Keys stay short deliberately -- NVS caps
    // them at 15 characters.
    constexpr const char *NVS_NAMESPACE = "pin";
    constexpr const char *KEY_SALT = "salt";
    constexpr const char *KEY_CANARY_IV = "civ";
    constexpr const char *KEY_CANARY_TAG = "ctag";
    constexpr const char *KEY_CANARY_CT = "cct";
    constexpr const char *KEY_FAILS = "fails";

    constexpr size_t SALT_LEN = 16;
    constexpr size_t IV_LEN = 12;
    constexpr size_t TAG_LEN = 16;
    constexpr unsigned int PBKDF2_ITERATIONS = 10000;

    // Fixed marker plaintext -- never shown to the user, just something to
    // GCM-authenticate against a candidate key. Any fixed value works; the
    // content itself carries no meaning.
    constexpr size_t CANARY_LEN = 16;
    constexpr uint8_t CANARY_PLAINTEXT[CANARY_LEN] = {'T', 'o', 'k', 'e', 'n', 'P', 'i', 'n',
                                                        'C', 'a', 'n', 'a', 'r', 'y', '!', '\0'};

    Preferences prefs;
    bool opened = false;
    int fails = 0;

    void deriveKey(const char *pin, const uint8_t *salt, uint8_t key[PinStore::KEY_LEN]) {
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
        mbedtls_pkcs5_pbkdf2_hmac(&ctx, (const unsigned char *)pin, strlen(pin), salt, SALT_LEN, PBKDF2_ITERATIONS,
                                   (uint32_t)PinStore::KEY_LEN, key);
        mbedtls_md_free(&ctx);
    }

    void encryptCanary(const uint8_t key[PinStore::KEY_LEN], uint8_t iv[IV_LEN], uint8_t tag[TAG_LEN],
                        uint8_t ct[CANARY_LEN]) {
        esp_fill_random(iv, IV_LEN);

        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
        mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, CANARY_LEN, iv, IV_LEN, nullptr, 0, CANARY_PLAINTEXT, ct,
                                   TAG_LEN, tag);
        mbedtls_gcm_free(&gcm);
    }

    bool canaryAuthenticates(const uint8_t key[PinStore::KEY_LEN], const uint8_t iv[IV_LEN],
                              const uint8_t tag[TAG_LEN], const uint8_t ct[CANARY_LEN]) {
        uint8_t out[CANARY_LEN];

        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
        int rc = mbedtls_gcm_auth_decrypt(&gcm, CANARY_LEN, iv, IV_LEN, nullptr, 0, tag, TAG_LEN, ct, out);
        mbedtls_gcm_free(&gcm);

        return rc == 0;
    }

    void persistFails() {
        prefs.putUChar(KEY_FAILS, (uint8_t)fails);
    }
}

void PinStore::begin() {
    opened = prefs.begin(NVS_NAMESPACE, false);
    if (!opened) {
        Serial.println("PinStore: failed to open NVS namespace");
        return;
    }

    fails = prefs.getUChar(KEY_FAILS, 0);
}

bool PinStore::isSet() {
    return opened && prefs.getBytesLength(KEY_SALT) == SALT_LEN;
}

void PinStore::setPin(const char *pin, uint8_t key[KEY_LEN]) {
    if (!opened) return;

    uint8_t salt[SALT_LEN];
    esp_fill_random(salt, SALT_LEN);
    deriveKey(pin, salt, key);

    uint8_t iv[IV_LEN], tag[TAG_LEN], ct[CANARY_LEN];
    encryptCanary(key, iv, tag, ct);

    prefs.putBytes(KEY_SALT, salt, SALT_LEN);
    prefs.putBytes(KEY_CANARY_IV, iv, IV_LEN);
    prefs.putBytes(KEY_CANARY_TAG, tag, TAG_LEN);
    prefs.putBytes(KEY_CANARY_CT, ct, CANARY_LEN);

    fails = 0;
    persistFails();
}

bool PinStore::verify(const char *pin, uint8_t key[KEY_LEN]) {
    if (!isSet()) return false;

    uint8_t salt[SALT_LEN];
    prefs.getBytes(KEY_SALT, salt, SALT_LEN);
    deriveKey(pin, salt, key);

    uint8_t iv[IV_LEN], tag[TAG_LEN], ct[CANARY_LEN];
    prefs.getBytes(KEY_CANARY_IV, iv, IV_LEN);
    prefs.getBytes(KEY_CANARY_TAG, tag, TAG_LEN);
    prefs.getBytes(KEY_CANARY_CT, ct, CANARY_LEN);

    if (!canaryAuthenticates(key, iv, tag, ct)) {
        fails++;
        persistFails();
        return false;
    }

    fails = 0;
    persistFails();
    return true;
}

int PinStore::failCount() {
    return fails;
}

void PinStore::grantFinalAttempt() {
    if (!opened) return;
    fails = MAX_ATTEMPTS > 0 ? MAX_ATTEMPTS - 1 : 0;
    persistFails();
}

void PinStore::wipe() {
    if (!opened) opened = prefs.begin(NVS_NAMESPACE, false);
    if (opened) prefs.clear();
    fails = 0;
}
