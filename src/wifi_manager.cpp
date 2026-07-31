#include "wifi_manager.h"

#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

#include "time_sync.h"

namespace {
    // Any year at or past this means the clock has been set by something
    // real, not left at the zeroed epoch beginNtp() starts from.
    constexpr int MIN_PLAUSIBLE_YEAR = 2020;

    WifiManager::Network networks[WifiManager::MAX_NETWORKS];
    int count = 0;

    void sortByStrongestSignal() {
        // Small N (<= MAX_NETWORKS) and run once per scan -- a plain
        // insertion sort is plenty.
        for (int i = 1; i < count; i++) {
            WifiManager::Network key = networks[i];
            int j = i - 1;
            while (j >= 0 && networks[j].rssi < key.rssi) {
                networks[j + 1] = networks[j];
                j--;
            }
            networks[j + 1] = key;
        }
    }
}

int WifiManager::scan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int found = WiFi.scanNetworks();
    count = 0;
    for (int i = 0; i < found && count < MAX_NETWORKS; i++) {
        networks[count].ssid = WiFi.SSID(i);
        networks[count].rssi = WiFi.RSSI(i);
        networks[count].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
        count++;
    }
    WiFi.scanDelete();

    sortByStrongestSignal();
    return count;
}

int WifiManager::networkCount() {
    return count;
}

const WifiManager::Network &WifiManager::networkAt(int index) {
    return networks[index];
}

bool WifiManager::connect(const String &ssid, const String &password, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    if (password.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
    } else {
        WiFi.begin(ssid.c_str());
    }

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(100);
    }

    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::beginNtp() {
    // The ESP32's RTC-backed system clock survives deep sleep (deliberately
    // -- it's what lets the side button wake the device), so a stale value
    // from a previous sync can still be sitting there. A plausible-looking
    // year doesn't prove a fresh NTP reply arrived, so the check in
    // ntpArrived() would happily pass against leftover stale time. Zero the
    // clock first so that check can only pass once a real reply sets it
    // forward.
    struct timeval zero = {0, 0};
    settimeofday(&zero, nullptr);

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

bool WifiManager::ntpArrived() {
    time_t now;
    time(&now);

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    if (timeinfo.tm_year + 1900 < MIN_PLAUSIBLE_YEAR) return false;

    TimeSync::setUnixTime(now);

    // Logged as UTC so a wrong reading here (vs. a wrong reading only in
    // the UI) points straight at the NTP response itself, not our display
    // code.
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("NTP synced: %s UTC (epoch %ld)\n", buf, (long)now);

    return true;
}

bool WifiManager::syncTime(uint32_t timeoutMs) {
    beginNtp();

    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (ntpArrived()) return true;
        delay(100);
    }

    Serial.println("NTP sync timed out");
    return false;
}

void WifiManager::disconnect() {
    WiFi.disconnect(true);
    // Powering the radio down rather than just dropping the association --
    // otherwise a failed WiFi.begin() keeps retrying in the background for
    // the rest of the session, and the station stays up burning battery
    // long after the sync it was needed for. scan()/connect() both set the
    // mode back to WIFI_STA, so bringing it up again costs nothing.
    WiFi.mode(WIFI_OFF);
}
