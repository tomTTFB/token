#include "backlight.h"

#include <Arduino.h>

#include "board_pins.h"

namespace {
    constexpr uint32_t PWM_FREQ_HZ = 5000;
    constexpr uint8_t PWM_RESOLUTION_BITS = 8;
    constexpr uint8_t DEFAULT_LEVEL = 100;
    // Below this the ST7789 backlight is effectively unreadable, so there's
    // no point letting the slider go all the way to 0.
    constexpr uint8_t MIN_LEVEL = 10;

    uint8_t currentLevel = DEFAULT_LEVEL;

    void apply(uint8_t percent) {
        uint32_t duty = ((uint32_t)percent * 255) / 100;
        ledcWrite(TFT_BACKLIGHT_PWM_CHANNEL, duty);
    }
}

void Backlight::init() {
    ledcSetup(TFT_BACKLIGHT_PWM_CHANNEL, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcAttachPin(TFT_BL, TFT_BACKLIGHT_PWM_CHANNEL);
    apply(currentLevel);
}

void Backlight::setLevel(uint8_t percent) {
    if (percent < MIN_LEVEL) percent = MIN_LEVEL;
    if (percent > 100) percent = 100;
    currentLevel = percent;
    apply(currentLevel);
}

uint8_t Backlight::getLevel() {
    return currentLevel;
}
