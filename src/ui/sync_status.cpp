#include "sync_status.h"

#include <time.h>

#include "account_list.h"
#include "colors.h"
#include "time_sync.h"
#include "wifi_manager.h"
#include "wifi_store.h"

namespace {
    void drawStatus(TFT_eSPI &tft, const char *ssid, const char *line1, const char *line2, uint16_t color2) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString("Sync Time", 10, 6, 4);
        tft.drawFastHLine(0, 26, tft.width(), TOKEN_BLUE);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(line1, tft.width() / 2, tft.height() / 2 - 18, 2);

        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(ssid, tft.width() / 2, tft.height() / 2, 1);

        tft.setTextColor(color2, TFT_BLACK);
        tft.drawString(line2, tft.width() / 2, tft.height() / 2 + 20, 2);

        AccountList::drawIdleFooter(tft);
    }
}

void SyncStatus::run(TFT_eSPI &tft, const String &ssid, const String &password) {
    drawStatus(tft, ssid.c_str(), "Connecting to WiFi...", "", TFT_WHITE);

    if (!WifiManager::connect(ssid, password)) {
        drawStatus(tft, ssid.c_str(), "Connection failed", "back: return", TFT_RED);
        // WiFi.begin() keeps retrying on its own after the timeout above,
        // so this path has to tear the radio down like the others do.
        WifiManager::disconnect();
        return;
    }

    // Saved here rather than after the NTP step: associating is what proves
    // the password is right, and a network worth reconnecting to at boot
    // shouldn't be forgotten just because this particular sync timed out.
    WifiStore::remember(ssid, password);

    drawStatus(tft, ssid.c_str(), "Syncing time...", "pool.ntp.org", TFT_DARKGREY);

    if (!WifiManager::syncTime()) {
        drawStatus(tft, ssid.c_str(), "Time sync failed", "back: return", TFT_RED);
        WifiManager::disconnect();
        return;
    }

    time_t now = TimeSync::now();
    struct tm tmVal;
    gmtime_r(&now, &tmVal);
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d:%02d UTC", tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);

    drawStatus(tft, ssid.c_str(), "Success!", timeStr, TOKEN_BLUE);
    WifiManager::disconnect();
}
