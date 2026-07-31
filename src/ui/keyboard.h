#pragma once

#include <TFT_eSPI.h>

// Full-screen text entry: a QWERTY + numbers + symbols grid navigated by
// rotating through cells in row-major order, wrapping at either end.
namespace Keyboard {
    // Resets the buffer and draws the keyboard under the given prompt
    // (e.g. "Password:").
    void enter(TFT_eSPI &tft, const char *prompt);

    // Moves the highlighted key by delta cells (wrapping), then redraws.
    void scroll(TFT_eSPI &tft, int delta);

    // "Presses" the highlighted key: appends a character, or triggers the
    // DEL/DONE action. Returns true once DONE is pressed -- the caller
    // should read buffer() and move on.
    bool press(TFT_eSPI &tft);

    // Removes the last character (this is also the side button's role on
    // this screen instead of "back"). Returns false if the buffer was
    // already empty, so the caller can treat that as "cancel" instead.
    bool backspace(TFT_eSPI &tft);

    const char *buffer();
}
