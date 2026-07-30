#pragma once

#include <TFT_eSPI.h>

namespace BootScreen {
    // Plays the matrix-rain intro and Token logo reveal. Blocks for the
    // duration of the animation, then leaves the rain frozen on screen.
    void play(TFT_eSPI &tft);
}
