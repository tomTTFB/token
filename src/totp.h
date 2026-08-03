#pragma once

#include <Arduino.h>

// RFC 6238 TOTP codes over RFC 4226 dynamic truncation, keyed by an
// RFC 4648 base32-encoded shared secret (the format every 2FA issuer hands
// out). HMAC-SHA1 only -- SHA1 is what virtually every otpauth:// secret in
// the wild is generated for, regardless of the algorithm's own reputation
// elsewhere.
namespace Totp {
    // Decodes a base32 secret (case-insensitive, '=' padding and spaces
    // tolerated/ignored) into raw bytes. Returns the decoded length, or -1
    // if the string contains a character outside the base32 alphabet.
    int base32Decode(const String &encoded, uint8_t *out, size_t outCapacity);

    // True if `secret` decodes cleanly under base32Decode -- used to
    // validate an incoming account before it's stored.
    bool isValidSecret(const String &secret);

    // Computes the current TOTP code for a base32 `secret`, `digits` long
    // (6-8), stepping every `period` seconds, at Unix time `unixTime`.
    // Writes a NUL-terminated, zero-padded string to `out` (must be at
    // least digits+1 bytes) and returns true, or returns false if the
    // secret doesn't decode or digits is out of range.
    bool generate(const String &secret, uint8_t digits, uint32_t period, time_t unixTime, char *out, size_t outCapacity);
}
