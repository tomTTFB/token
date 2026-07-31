#pragma once

#include <TFT_eSPI.h>

// The "Connecting to WiFi... / Syncing time... / Success/Fail" screen
// shown while WifiManager runs its blocking connect + NTP fetch.
namespace SyncStatus {
    // Runs the whole connect-then-sync flow against the given network,
    // updating the screen as it progresses, and leaves the final result
    // on screen until the user backs out.
    void run(TFT_eSPI &tft, const String &ssid, const String &password);
}
