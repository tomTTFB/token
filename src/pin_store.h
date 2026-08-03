#pragma once

#include <Arduino.h>

// PIN-derived key management. The 4-digit PIN itself is never stored --
// only a random salt and a small "canary" blob encrypted with the key
// PBKDF2 derives from the PIN. Checking a PIN attempt means re-deriving
// the key from the salt and the guess, then trying to authenticate-decrypt
// the canary with it: a wrong PIN produces a wrong key, and AES-GCM's
// built-in tag check makes that decryption fail outright rather than
// silently produce garbage. AccountStore's account secrets are encrypted
// under this same derived key.
namespace PinStore {
    constexpr size_t KEY_LEN = 32; // AES-256
    constexpr int PIN_LEN = 4;
    constexpr int MAX_ATTEMPTS = 5; // matches plan/ui.md's PIN entry screen

    // Opens the NVS namespace and loads the fail counter. Call once during
    // setup(), before any other function here.
    void begin();

    // True once a PIN has been configured (a salt + canary exist in NVS).
    // False on a factory-fresh device or right after wipe() -- the caller
    // should run the "set a PIN" flow instead of "enter your PIN" in that
    // case.
    bool isSet();

    // Generates a fresh salt, derives a key from `pin`, and stores a new
    // canary encrypted under it. Resets the fail counter. Used both for
    // first-time setup and for changing an existing PIN -- the caller is
    // responsible for re-encrypting AccountStore's entries under the
    // written key afterward (see AccountStore::reencrypt).
    void setPin(const char *pin, uint8_t key[KEY_LEN]);

    // Derives a key from `pin` against the stored salt and authenticates
    // it against the canary. On success, writes the key to `key`, resets
    // the fail counter, and returns true. On failure, increments and
    // persists the fail counter and returns false.
    bool verify(const char *pin, uint8_t key[KEY_LEN]);

    // Number of consecutive failed verify() calls since the last success
    // (or since setPin()/wipe()).
    int failCount();

    // Backs the fail counter off by one, so the next wrong attempt is the
    // last before a wipe rather than an immediate one. Used when the user
    // cancels out of the wipe-warning screen instead of confirming it --
    // plan/ui.md's "gives one final chance".
    void grantFinalAttempt();

    // Clears all PIN state (salt, canary, fail count) -- isSet() becomes
    // false again, as if the device were factory-fresh. Does not touch
    // AccountStore; callers wipe that separately.
    void wipe();
}
