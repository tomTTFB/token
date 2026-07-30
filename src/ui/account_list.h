#pragma once

#include <TFT_eSPI.h>

namespace AccountList {
    // Draws a static placeholder account list. Not wired to real accounts
    // or encoder input yet -- just the layout.
    void draw(TFT_eSPI &tft);
}
