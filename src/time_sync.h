#pragma once

#include <ctime>

// Tracks wall-clock time as a base Unix timestamp plus the millis() value
// at the moment it was set, per the hybrid time model in the architecture
// notes. There's no NVS persistence layer in this project yet, so a sync
// only lasts until the next reboot/deep-sleep cycle.
namespace TimeSync {
    // Called once a sync (NTP, or any future source) succeeds. Always UTC --
    // local-time display is a separate concern, see utcOffsetMinutes below.
    void setUnixTime(time_t unixTime);

    // Current UTC time, extrapolated from the last sync. Only meaningful
    // once isSynced() is true.
    time_t now();

    // now() shifted by the configured UTC offset, for display purposes
    // (there's no automatic timezone/DST lookup here -- the offset is set
    // by hand in Settings).
    time_t localNow();

    bool isSynced();

    // Manual UTC offset in minutes (e.g. +60 for BST), applied by
    // localNow(). Not persisted across reboots, same as the sync itself.
    void setUtcOffsetMinutes(int minutes);
    int utcOffsetMinutes();
}
