#include "power.h"

#include <esp_sleep.h>

#include "board_pins.h"
#include "ui/colors.h"

namespace {
    constexpr uint32_t HOLD_MS = 5000;

    uint32_t pressStart = 0;
    bool wasPressed = false;

    void shutdown(TFT_eSPI &tft, Adafruit_NeoPixel &pixels) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString("Shutting down...", tft.width() / 2, tft.height() / 2, 2);
        delay(500);

        pixels.clear();
        pixels.show();

        // Cuts power to the display, SD slot and radio; the ESP32 itself
        // keeps just enough power to watch BOARD_USER_KEY for wake-up.
        digitalWrite(BOARD_PWR_EN, LOW);

        esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_USER_KEY, 0);
        esp_deep_sleep_start();
    }
}

void Power::pollShutdownButton(bool pressed, TFT_eSPI &tft, Adafruit_NeoPixel &pixels) {
    if (pressed && !wasPressed) {
        pressStart = millis();
    }
    if (pressed && millis() - pressStart >= HOLD_MS) {
        shutdown(tft, pixels);
    }
    wasPressed = pressed;
}
