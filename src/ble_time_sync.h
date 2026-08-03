#pragma once

#include <Arduino.h>

// Time sync over BLE using the phone's built-in Current Time Service (CTS,
// GATT UUID 0x1805) -- the same trick cheap "dumb" Bluetooth watches use to
// get the time without any companion app. iOS, and Android on a
// best-effort basis, expose CTS to any peripheral once it's bonded via the
// phone's own Bluetooth settings screen -- no pairing code, nothing to
// install.
//
// The roles invert from what you'd expect: the phone (BLE central)
// connects to Token (the peripheral) the normal way, but once bonded,
// Token turns around and acts as the GATT *client*, reading the phone's
// own CTS characteristic. NimBLEClient refuses to attach to a connection
// it didn't dial itself, so this reads the phone directly via the
// low-level ble_gattc_* host calls against the connection handle handed
// to the server's onConnect callback -- see ble_time_sync.cpp.
namespace BleTimeSync {
    enum class State { Idle, Advertising, Bonding, Reading, Success, Failed };

    // Which offset the most recent successful sync actually used to
    // convert the phone's reported local time to UTC. Detected means the
    // phone exposed CTS's optional Local Time Information characteristic
    // (0x2A0F) and it had a known time zone; Manual means it fell back to
    // Settings > Time Zone, either because the phone didn't expose that
    // characteristic or reported its zone as unknown. Meaningless before
    // the first Success.
    enum class OffsetSource { Unknown, Manual, Detected };

    // Initializes the BLE stack and starts advertising as "Token" with
    // bonding required. Safe to call again (e.g. to retry after Failed);
    // stop() should be called first if a previous attempt is still live.
    void begin();

    // Must be polled every loop() iteration while a sync attempt is in
    // flight. The GATT discovery/read chain runs on NimBLE's own host
    // task; poll() is where the result gets handed to TimeSync, keeping
    // that (and the display) single-threaded.
    void poll();

    State state();

    // See OffsetSource above.
    OffsetSource lastOffsetSource();

    // Disconnects and frees the NimBLE stack. Call this whenever the
    // Bluetooth Sync screen is left, success or not. Bonds are left intact
    // -- a phone that's paired once will reconnect silently on later
    // visits instead of re-pairing, and deleting only Token's half of a
    // bond while the phone keeps its own causes a connect/disconnect loop
    // on the next attempt (see ble_time_sync.cpp).
    void stop();
}
