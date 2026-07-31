#pragma once

#include <Arduino.h>

// Boot-time reconnect to any saved network in range (see WifiStore),
// followed by an NTP sync -- so a device that's been powered off comes back
// with a valid clock without the user doing anything.
//
// Every step is non-blocking and driven from loop(), because the whole
// sequence (scan, associate, SNTP round trip) can take tens of seconds and
// the account list has to stay responsive throughout. The radio is switched
// off again as soon as the attempt finishes either way, so this doesn't
// leave WiFi associated for the rest of the session.
namespace WifiAuto {
    enum class State { Idle, Scanning, Connecting, Syncing, Done, Failed };

    // Kicks off an async scan, unless there are no saved networks at all --
    // in which case the radio is never powered up. Call once from setup(),
    // after WifiStore::begin().
    void begin();

    // Advances the state machine; call every loop() iteration. No-op once
    // the attempt has finished.
    void poll();

    // Abandons an in-flight attempt and releases the radio. Must be called
    // before any manual WiFi use (the Sync Time flow), which would
    // otherwise be fighting this for the same hardware.
    void cancel();

    State state();
}
