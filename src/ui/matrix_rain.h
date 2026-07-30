#pragma once

#include <TFT_eSPI.h>

// A cmatrix-style digital rain effect, rendered in Token blue: the leading
// character of each column is drawn at full brightness and the characters
// behind it fade step by step down to black.
namespace MatrixRain {
    // Lays out one column per character cell across the full screen and
    // resets every column to a random falling position.
    void begin(TFT_eSPI &tft);

    // Advances any column whose turn it is to fall a row, rate-limited
    // internally by millis(). Safe to call every loop iteration.
    void update(TFT_eSPI &tft);
}
