#pragma once

#include <TFT_eSPI.h>

namespace AccountList {
    // Draws the account list (plus the trailing Settings row) around the
    // current selection, windowed to fit the screen.
    void draw(TFT_eSPI &tft);

    // Moves the selection by delta rows (wrapping past either end) across
    // accounts and the trailing Settings row, then redraws.
    void scroll(TFT_eSPI &tft, int delta);

    // True when the trailing Settings row is the current selection.
    bool isSettingsSelected();

    // Redraws just the bottom hint/countdown line, so the power-off hold
    // countdown can update every tick without a full-screen redraw.
    void drawFooter(TFT_eSPI &tft, const String &text, uint16_t color);

    // Standard idle footer ("hold side button 5s to power off"), shared by
    // every screen so the power-off hint always reads the same.
    void drawIdleFooter(TFT_eSPI &tft);
}
