#pragma once

#include <cstdint>

// Signature Token blue: a clean azure blue, distinct from cyan (too green)
// and navy (too dark). Used for the boot logo, rain highlights and UI accents.
constexpr uint8_t TOKEN_BLUE_R = 59;
constexpr uint8_t TOKEN_BLUE_G = 130;
constexpr uint8_t TOKEN_BLUE_B = 246;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t TOKEN_BLUE = rgb565(TOKEN_BLUE_R, TOKEN_BLUE_G, TOKEN_BLUE_B);
constexpr uint16_t TOKEN_BLUE_DIM = rgb565(TOKEN_BLUE_R / 6, TOKEN_BLUE_G / 6, TOKEN_BLUE_B / 6);
