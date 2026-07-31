#pragma once

#include <ctime>

// Tracks wall-clock time as a base Unix timestamp plus the millis() value
// at the moment it was set, per the hybrid time model in the architecture
// notes. There's no NVS persistence layer in this project yet, so a sync
// only lasts until the next reboot/deep-sleep cycle.
namespace TimeSync {
    // Called once a sync (NTP, or any future source) succeeds.
    void setUnixTime(time_t unixTime);

    // Current wall-clock time, extrapolated from the last sync. Only
    // meaningful once isSynced() is true.
    time_t now();

    bool isSynced();
}
