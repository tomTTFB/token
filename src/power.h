#pragma once

#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>

namespace Power {
    // Call once per loop with the current side-button state (true ==
    // pressed). Once the button has been held continuously for the hold
    // threshold, cuts peripheral power and deep-sleeps the MCU; a later
    // press of the same button wakes it back up into a fresh boot.
    //
    // Returns true once, on the loop where the button is released after a
    // press shorter than the hold threshold -- callers use this as the
    // "back" gesture for screen navigation.
    bool pollShutdownButton(bool pressed, TFT_eSPI &tft, Adafruit_NeoPixel &pixels);
}
