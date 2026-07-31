#pragma once

#include <cstdint>

namespace Backlight {
    // Attaches LEDC PWM to TFT_BL and applies the default level. Must be
    // called after tft.begin() so it wins over any pin state TFT_eSPI sets.
    void init();

    // Sets brightness as a percentage, clamped to [MIN_LEVEL, 100].
    void setLevel(uint8_t percent);

    uint8_t getLevel();
}
