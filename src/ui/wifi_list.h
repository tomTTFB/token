#pragma once

#include <TFT_eSPI.h>

// Lists nearby WiFi networks (scanned on entry) for the sync-time flow.
// An open network can connect immediately; a secured one needs a password
// via Keyboard first.
namespace WifiList {
    // Runs a blocking scan and draws the resulting list.
    void enter(TFT_eSPI &tft);

    // Redraws the list as it was left, without rescanning -- used when
    // control returns from the keyboard screen (cancelled with an empty
    // password buffer).
    void redraw(TFT_eSPI &tft);

    // Moves the selection by delta rows (wrapping), then redraws.
    void scroll(TFT_eSPI &tft, int delta);

    // True if the highlighted network has no password.
    bool selectedIsOpen();

    const String &selectedSsid();
}
