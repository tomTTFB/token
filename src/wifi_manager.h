#pragma once

#include <Arduino.h>

namespace WifiManager {
    struct Network {
        String ssid;
        int32_t rssi;
        bool open;
    };

    constexpr int MAX_NETWORKS = 20;

    // Blocking scan (typically a second or two). Populates the internal
    // network list, strongest signal first. Returns the number found.
    int scan();

    int networkCount();
    const Network &networkAt(int index);

    // Blocking connect with a timeout; returns true once an IP is
    // assigned. Pass an empty password for open networks.
    bool connect(const String &ssid, const String &password, uint32_t timeoutMs = 15000);

    // Blocking NTP fetch -- WiFi must already be connected. On success,
    // stores the result in TimeSync and returns true.
    bool syncTime(uint32_t timeoutMs = 10000);

    void disconnect();
}
