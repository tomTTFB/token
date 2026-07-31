#pragma once

#include <TFT_eSPI.h>

namespace Settings {
    // Draws the top-level settings menu with the first item selected.
    // Called when the encoder button is pressed while the account list's
    // trailing Settings row is selected.
    void enter(TFT_eSPI &tft);

    // Moves the encoder-driven selection by delta rows: menu item on the
    // top-level menu, brightness step on the System page. No-op on
    // read-only pages (Sync Time, About).
    void scroll(TFT_eSPI &tft, int delta);

    // Handles an encoder button press. Enters the selected submenu item
    // from the top-level menu; no-op on submenu pages.
    void press(TFT_eSPI &tft);

    // Steps back one level on a side-button click. Returns true if the
    // press was handled within settings (submenu -> menu); false if the
    // caller should leave settings entirely (already at the top menu).
    bool back(TFT_eSPI &tft);
}
