#include "time_sync.h"

#include <Arduino.h>

namespace {
    time_t baseUnixTime = 0;
    uint32_t baseMillis = 0;
    bool synced = false;
    int offsetMinutes = 0;
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

time_t TimeSync::localNow() {
    return now() + (time_t)offsetMinutes * 60;
}

bool TimeSync::isSynced() {
    return synced;
}

void TimeSync::setUtcOffsetMinutes(int minutes) {
    offsetMinutes = minutes;
}

int TimeSync::utcOffsetMinutes() {
    return offsetMinutes;
}
