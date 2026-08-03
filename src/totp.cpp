#include "totp.h"

#include <mbedtls/md.h>

namespace {
    // Secrets this long or longer aren't real-world TOTP keys (issuers
    // hand out 10-20 raw bytes, i.e. 16-32 base32 characters) -- capping
    // here keeps base32Decode's caller-supplied buffer small and fixed.
    constexpr size_t MAX_KEY_BYTES = 64;

    int charValue(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= '2' && c <= '7') return c - '2' + 26;
        return -1;
    }

    uint32_t pow10(uint8_t n) {
        uint32_t v = 1;
        while (n--) v *= 10;
        return v;
    }
}

int Totp::base32Decode(const String &encoded, uint8_t *out, size_t outCapacity) {
    uint32_t bitBuffer = 0;
    int bitsLeft = 0;
    size_t outLen = 0;

    for (size_t i = 0; i < encoded.length(); i++) {
        char c = encoded[i];
        if (c == '=' || c == ' ' || c == '-') continue;

        int val = charValue(toupper(c));
        if (val < 0) return -1;

        bitBuffer = (bitBuffer << 5) | (uint32_t)val;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            if (outLen >= outCapacity) return -1;
            out[outLen++] = (uint8_t)((bitBuffer >> (bitsLeft - 8)) & 0xFF);
            bitsLeft -= 8;
        }
    }

    return (int)outLen;
}

bool Totp::isValidSecret(const String &secret) {
    uint8_t scratch[MAX_KEY_BYTES];
    return base32Decode(secret, scratch, sizeof(scratch)) > 0;
}

bool Totp::generate(const String &secret, uint8_t digits, uint32_t period, time_t unixTime, char *out, size_t outCapacity) {
    if (digits < 6 || digits > 8 || period == 0) return false;
    if (outCapacity < (size_t)digits + 1) return false;

    uint8_t key[MAX_KEY_BYTES];
    int keyLen = base32Decode(secret, key, sizeof(key));
    if (keyLen <= 0) return false;

    uint64_t counter = (uint64_t)unixTime / period;
    uint8_t counterBytes[8];
    for (int i = 7; i >= 0; i--) {
        counterBytes[i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }

    uint8_t hmac[20]; // SHA1 digest size
    const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (mbedtls_md_hmac(mdInfo, key, (size_t)keyLen, counterBytes, sizeof(counterBytes), hmac) != 0) {
        return false;
    }

    // RFC 4226 dynamic truncation.
    int offset = hmac[19] & 0x0F;
    uint32_t binCode = ((uint32_t)(hmac[offset] & 0x7F) << 24) | ((uint32_t)(hmac[offset + 1] & 0xFF) << 16) |
                        ((uint32_t)(hmac[offset + 2] & 0xFF) << 8) | (uint32_t)(hmac[offset + 3] & 0xFF);

    uint32_t code = binCode % pow10(digits);
    snprintf(out, outCapacity, "%0*lu", digits, (unsigned long)code);
    return true;
}
