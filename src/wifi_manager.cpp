#include "wifi_manager.h"

#include <WiFi.h>
#include <time.h>

#include "time_sync.h"

namespace {
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

bool WifiManager::syncTime(uint32_t timeoutMs) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, timeoutMs)) {
        return false;
    }

    time_t now;
    time(&now);
    TimeSync::setUnixTime(now);
    return true;
}

void WifiManager::disconnect() {
    WiFi.disconnect(true);
}
