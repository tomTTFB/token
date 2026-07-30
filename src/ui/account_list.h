#pragma once

#include <TFT_eSPI.h>

namespace AccountList {
    // Draws a static placeholder account list. Not wired to real accounts
    // or encoder input yet -- just the layout.
    void draw(TFT_eSPI &tft);

    // Redraws just the bottom hint/countdown line, so the power-off hold
    // countdown can update every tick without a full-screen redraw.
    void drawFooter(TFT_eSPI &tft, const String &text, uint16_t color);
}
