#include "time_sync.h"

#include <Arduino.h>

namespace {
    time_t baseUnixTime = 0;
    uint32_t baseMillis = 0;
    bool synced = false;
}

void TimeSync::setUnixTime(time_t unixTime) {
    baseUnixTime = unixTime;
    baseMillis = millis();
    synced = true;
}

time_t TimeSync::now() {
    if (!synced) return 0;
    return baseUnixTime + (time_t)((millis() - baseMillis) / 1000);
}

bool TimeSync::isSynced() {
    return synced;
}
