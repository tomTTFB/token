#include "power.h"

#include <esp_sleep.h>

#include "board_pins.h"
#include "ui/account_list.h"
#include "ui/colors.h"

namespace {
    constexpr uint32_t HOLD_MS = 5000;
    // Only the final stretch of the hold gets a visible countdown; a hint
    // that starts blinking the instant you touch the button is more
    // alarming than useful.
    constexpr int COUNTDOWN_START_SECONDS = 3;

    uint32_t pressStart = 0;
    bool wasPressed = false;
    int lastShownSeconds = -1;

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

bool Power::pollShutdownButton(bool pressed, TFT_eSPI &tft, Adafruit_NeoPixel &pixels) {
    uint32_t now = millis();
    bool shortClick = false;

    if (pressed && !wasPressed) {
        pressStart = now;
        lastShownSeconds = -1;
    }

    if (pressed) {
        uint32_t elapsed = now - pressStart;
        if (elapsed >= HOLD_MS) {
            shutdown(tft, pixels);
        }

        int secondsLeft = (HOLD_MS - elapsed + 999) / 1000; // ceil to whole seconds
        if (secondsLeft <= COUNTDOWN_START_SECONDS && secondsLeft != lastShownSeconds) {
            char countdown[24];
            snprintf(countdown, sizeof(countdown), "powering off in %ds", secondsLeft);
            AccountList::drawFooter(tft, countdown, TOKEN_BLUE);
            lastShownSeconds = secondsLeft;
        }
    } else if (wasPressed) {
        shortClick = true;
        AccountList::drawIdleFooter(tft);
        lastShownSeconds = -1;
    }

    wasPressed = pressed;
    return shortClick;
}
