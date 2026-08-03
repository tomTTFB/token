#pragma once

#include <Arduino.h>

#include "pin_store.h"

// Persistent list of TOTP accounts. Each entry's name/issuer/secret are
// serialized and encrypted together with AES-256-GCM under the key
// PinStore derives from the user's PIN (see pin_store.h) -- a wrong key
// fails GCM's tag check outright rather than producing garbage.
// digits/period aren't sensitive and stay plaintext.
namespace AccountStore {
    constexpr int MAX_ENTRIES = 16;
    constexpr uint8_t DEFAULT_DIGITS = 6;
    constexpr uint32_t DEFAULT_PERIOD = 30;

    struct Account {
        String name;
        String issuer;
        String secret; // base32
        uint8_t digits;
        uint32_t period;
    };

    // Opens the NVS namespace and decrypts the saved list into RAM using
    // `key` (from PinStore::verify() or PinStore::setPin()). Call once,
    // right after a successful PIN unlock. An entry that fails to
    // authenticate under `key` -- corrupt, or left over from a previous
    // PIN generation -- is dropped rather than surfaced as garbage.
    void begin(const uint8_t key[PinStore::KEY_LEN]);

    // Validates `secret` decodes as base32 and appends a new account,
    // evicting nothing -- returns false once MAX_ENTRIES is reached, or if
    // the secret is invalid.
    bool add(const String &name, const String &issuer, const String &secret, uint8_t digits = DEFAULT_DIGITS,
             uint32_t period = DEFAULT_PERIOD);

    // Re-encrypts every entry currently held in RAM under `newKey` and
    // persists it. Called after PinStore::setPin() during a PIN change,
    // once the account list has already been decrypted under the old key.
    void reencrypt(const uint8_t newKey[PinStore::KEY_LEN]);

    // Clears every stored account, in RAM and in NVS. Safe to call even
    // before begin() (e.g. wiping from the locked wipe-warning screen).
    void wipeAll();

    int count();
    const Account &at(int index);
}
