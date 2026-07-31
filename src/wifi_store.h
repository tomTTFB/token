#pragma once

#include <Arduino.h>

// Persistent list of WiFi networks that have connected successfully at
// least once, so Token can reconnect on its own at boot without the user
// walking the scan/keyboard flow again.
//
// Ordering is most-recently-connected first -- WifiAuto uses that as its
// tie-breaker when several known networks are in range.
//
// Credentials are stored unencrypted in NVS. That matches the threat model
// in the architecture notes (an attacker who can read flash directly is out
// of scope), but it does mean a WiFi password is recoverable from a
// dumped flash image, unlike the account secrets.
namespace WifiStore {
    constexpr int MAX_ENTRIES = 8;

    // Opens the NVS namespace and loads the saved list into RAM. Call once
    // during setup(), before any other function here.
    void begin();

    // Records a working network, moving it to the front. Updates the stored
    // password if it changed, and evicts the least recently used entry once
    // MAX_ENTRIES is reached. No-op for an empty SSID.
    bool remember(const String &ssid, const String &password);

    bool isKnown(const String &ssid);

    // Looks up a saved password. Returns false if the SSID isn't known;
    // an empty string on success means a known open network.
    bool passwordFor(const String &ssid, String &out);

    void forget(const String &ssid);

    int count();
    const String &ssidAt(int index);
}
