#pragma once

#include <TFT_eSPI.h>

namespace Settings {
    // Returned by press() when it wants the caller to hand off to a screen
    // that lives outside the Settings module entirely.
    enum class Action { None, OpenWifiList };

    // Draws the top-level settings menu with the first item selected.
    // Called when the encoder button is pressed while the account list's
    // trailing Settings row is selected.
    void enter(TFT_eSPI &tft);

    // Redraws whatever page is currently active, without changing it --
    // used when control returns to Settings from an external screen (the
    // WiFi list, keyboard, or sync status) that was reached via
    // Action::OpenWifiList.
    void redraw(TFT_eSPI &tft);

    // Moves the encoder-driven selection by delta rows: menu item on the
    // top-level menu, brightness step on the System page, WiFi/Bluetooth
    // choice on Sync Time. No-op on read-only pages (About, Bluetooth
    // Sync).
    void scroll(TFT_eSPI &tft, int delta);

    // Handles an encoder button press. Enters the selected submenu item
    // from the top-level menu; on Sync Time, either opens the WiFi flow
    // (see Action) or the Bluetooth placeholder. No-op elsewhere.
    Action press(TFT_eSPI &tft);

    // Steps back one level on a side-button click. Returns true if the
    // press was handled within settings; false if the caller should leave
    // settings entirely (already at the top menu).
    bool back(TFT_eSPI &tft);
}
