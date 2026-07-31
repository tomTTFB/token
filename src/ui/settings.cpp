#include "settings.h"

#include <Arduino.h>

#include "account_list.h"
#include "backlight.h"
#include "colors.h"
#include "time_sync.h"

namespace {
    enum class Page { Menu, SyncTime, BluetoothSync, System, About, TimeZone };

    const char *MENU_ITEMS[] = {"Sync Time", "System", "Time Zone", "About"};
    constexpr int MENU_COUNT = 4;
    constexpr int MENU_TOP = 34;
    constexpr int MENU_ROW_H = 26;
    constexpr uint8_t BRIGHTNESS_STEP = 5;
    constexpr int TIMEZONE_STEP_MINUTES = 30;
    constexpr int TIMEZONE_MIN_MINUTES = -12 * 60;
    constexpr int TIMEZONE_MAX_MINUTES = 14 * 60;

    constexpr int SYNC_BTN_TOP = 30;
    constexpr int SYNC_BTN_H = 122;
    constexpr int SYNC_BTN_GAP = 8;
    constexpr int SYNC_BTN_COUNT = 2;

    Page page = Page::Menu;
    int menuSelected = 0;
    int syncMenuSelected = 0; // 0 = WiFi, 1 = Bluetooth

    void drawHeader(TFT_eSPI &tft, const char *title) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString(title, 10, 6, 4);
        tft.drawFastHLine(0, 26, tft.width(), TOKEN_BLUE);
    }

    void drawInfoRow(TFT_eSPI &tft, int y, const char *label, const String &value) {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(label, 16, y, 1);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(value, tft.width() - 16, y, 1);
    }

    void drawMenuRow(TFT_eSPI &tft, int i) {
        int y = MENU_TOP + i * MENU_ROW_H;
        bool sel = i == menuSelected;
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        // Clear the row's own rectangle first so an unselected row erases
        // its own leftover highlight when only this row is redrawn.
        tft.fillRect(4, y, tft.width() - 8, MENU_ROW_H - 4, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(4, y, tft.width() - 8, MENU_ROW_H - 4, 6, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, MENU_ROW_H - 4, 6, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString(MENU_ITEMS[i], 16, y + 5, 2);
    }

    void drawMenu(TFT_eSPI &tft) {
        drawHeader(tft, "Settings");
        for (int i = 0; i < MENU_COUNT; i++) drawMenuRow(tft, i);
        AccountList::drawIdleFooter(tft);
    }

    void drawWifiIcon(TFT_eSPI &tft, int cx, int cy, uint16_t color) {
        // TFT_eSPI's drawArc has 0 degrees at 6 o'clock, sweeping clockwise
        // -- so straight up is 180, and 45 degrees either side of that
        // (135/225) gives a narrow upward fan instead of a full dome
        // reaching all the way down to the horizontal (90/270).
        tft.fillCircle(cx, cy, 3, color);
        tft.drawArc(cx, cy, 12, 9, 135, 225, color, TFT_BLACK, true);
        tft.drawArc(cx, cy, 21, 18, 135, 225, color, TFT_BLACK, true);
        tft.drawArc(cx, cy, 30, 27, 135, 225, color, TFT_BLACK, true);
    }

    // A simplified version of the Bluetooth rune-bind glyph: a vertical
    // stroke with a bowtie crossing it on the right.
    void drawBluetoothIcon(TFT_eSPI &tft, int cx, int cy, uint16_t color) {
        int h = 44, w = 26;
        int top = cy - h / 2;
        int bottom = cy + h / 2;
        int mid = cy;
        int rUpper = cy - h / 4;
        int rLower = cy + h / 4;
        int xr = cx + w / 2;

        // Two triangles sharing the center point on the vertical line --
        // top->upper-right->center forms one, center->lower-right->bottom
        // the other. Crossing upper and lower here draws a star instead
        // of the bowtie.
        tft.drawLine(cx, top, cx, bottom, color);
        tft.drawLine(cx, top, xr, rUpper, color);
        tft.drawLine(xr, rUpper, cx, mid, color);
        tft.drawLine(cx, mid, xr, rLower, color);
        tft.drawLine(xr, rLower, cx, bottom, color);
    }

    void drawSyncButton(TFT_eSPI &tft, int i) {
        int btnW = (tft.width() - SYNC_BTN_GAP * (SYNC_BTN_COUNT + 1)) / SYNC_BTN_COUNT;
        const char *labels[SYNC_BTN_COUNT] = {"WiFi", "Bluetooth"};

        int x = SYNC_BTN_GAP + i * (btnW + SYNC_BTN_GAP);
        bool sel = i == syncMenuSelected;
        uint16_t accent = sel ? TOKEN_BLUE : TFT_DARKGREY;

        tft.fillRect(x, SYNC_BTN_TOP, btnW, SYNC_BTN_H, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(x, SYNC_BTN_TOP, btnW, SYNC_BTN_H, 8, TOKEN_BLUE_DIM);
        }
        tft.drawRoundRect(x, SYNC_BTN_TOP, btnW, SYNC_BTN_H, 8, accent);

        int cx = x + btnW / 2;
        int iconCy = SYNC_BTN_TOP + SYNC_BTN_H / 2 - 14;

        if (i == 0) {
            drawWifiIcon(tft, cx, iconCy, accent);
        } else {
            drawBluetoothIcon(tft, cx, iconCy, accent);
        }

        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
        tft.drawString(labels[i], cx, SYNC_BTN_TOP + SYNC_BTN_H - 8, 2);
    }

    void drawSyncTime(TFT_eSPI &tft) {
        drawHeader(tft, "Sync Time");
        for (int i = 0; i < SYNC_BTN_COUNT; i++) drawSyncButton(tft, i);
        AccountList::drawIdleFooter(tft);
    }

    void drawBluetoothSync(TFT_eSPI &tft) {
        drawHeader(tft, "Bluetooth Sync");

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Coming soon", tft.width() / 2, tft.height() / 2, 2);

        AccountList::drawIdleFooter(tft);
    }

    // Just the pct readout + bar -- the part that changes as the encoder
    // adjusts brightness. The pct text is cleared to a fixed-size band
    // first since its width varies ("5%" vs "100%"), which drawString's
    // own background-fill wouldn't fully cover on a shrink.
    void drawBrightnessValue(TFT_eSPI &tft) {
        uint8_t level = Backlight::getLevel();

        tft.fillRect(tft.width() - 60, 36, 44, 18, TFT_BLACK);
        char pct[8];
        snprintf(pct, sizeof(pct), "%d%%", level);
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TOKEN_BLUE_LIGHT, TFT_BLACK);
        tft.drawString(pct, tft.width() - 16, 38, 2);

        int barX = 16, barY = 64, barW = tft.width() - 32, barH = 14;
        tft.drawRect(barX, barY, barW, barH, TOKEN_BLUE);
        int fillW = (barW - 2) * level / 100;
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, TOKEN_BLUE);
        tft.fillRect(barX + 1 + fillW, barY + 1, barW - 2 - fillW, barH - 2, TFT_BLACK);
    }

    void drawSystem(TFT_eSPI &tft) {
        drawHeader(tft, "System");

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Brightness", 16, 38, 2);

        drawBrightnessValue(tft);

        drawInfoRow(tft, 100, "Free heap", String(ESP.getFreeHeap() / 1024) + " KB");
        drawInfoRow(tft, 116, "Uptime", String(millis() / 1000) + " s");

        AccountList::drawIdleFooter(tft);
    }

    // The offset readout + local-time preview -- the part that changes as
    // the encoder adjusts the offset. The offset string is a fixed 6
    // characters ("+01:00"), so drawString's own background fill is
    // enough; no explicit clear needed the way the brightness pct does.
    void drawTimeZoneValue(TFT_eSPI &tft) {
        int offset = TimeSync::utcOffsetMinutes();
        char offsetStr[8];
        snprintf(offsetStr, sizeof(offsetStr), "%c%02d:%02d", offset < 0 ? '-' : '+',
                 abs(offset) / 60, abs(offset) % 60);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TOKEN_BLUE_LIGHT, TFT_BLACK);
        tft.drawString(offsetStr, tft.width() - 16, 38, 2);

        char localStr[6] = "--:--";
        if (TimeSync::isSynced()) {
            time_t t = TimeSync::localNow();
            struct tm tmVal;
            gmtime_r(&t, &tmVal);
            snprintf(localStr, sizeof(localStr), "%02d:%02d", tmVal.tm_hour, tmVal.tm_min);
        }
        drawInfoRow(tft, 74, "Local time now", localStr);
    }

    void drawTimeZone(TFT_eSPI &tft) {
        drawHeader(tft, "Time Zone");

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("UTC offset", 16, 38, 2);

        drawTimeZoneValue(tft);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("scroll to adjust, 30 min steps", tft.width() / 2, tft.height() - 32, 1);

        AccountList::drawIdleFooter(tft);
    }

    void drawAbout(TFT_eSPI &tft) {
        drawHeader(tft, "About");

        drawInfoRow(tft, 38, "Firmware built", String(__DATE__) + " " + String(__TIME__));
        drawInfoRow(tft, 58, "Chip", String(ESP.getChipModel()) + " rev" + String(ESP.getChipRevision()));
        drawInfoRow(tft, 78, "CPU freq", String(ESP.getCpuFreqMHz()) + " MHz");
        drawInfoRow(tft, 98, "Flash", String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB");
        drawInfoRow(tft, 118, "Free heap", String(ESP.getFreeHeap() / 1024) + " KB");

        AccountList::drawIdleFooter(tft);
    }

    void drawCurrentPage(TFT_eSPI &tft) {
        switch (page) {
            case Page::Menu: drawMenu(tft); break;
            case Page::SyncTime: drawSyncTime(tft); break;
            case Page::BluetoothSync: drawBluetoothSync(tft); break;
            case Page::System: drawSystem(tft); break;
            case Page::TimeZone: drawTimeZone(tft); break;
            case Page::About: drawAbout(tft); break;
        }
    }
}

void Settings::enter(TFT_eSPI &tft) {
    page = Page::Menu;
    menuSelected = 0;
    drawCurrentPage(tft);
}

void Settings::redraw(TFT_eSPI &tft) {
    drawCurrentPage(tft);
}

void Settings::scroll(TFT_eSPI &tft, int delta) {
    switch (page) {
        case Page::Menu: {
            int oldSelected = menuSelected;
            menuSelected = ((menuSelected + delta) % MENU_COUNT + MENU_COUNT) % MENU_COUNT;
            drawMenuRow(tft, oldSelected);
            drawMenuRow(tft, menuSelected);
            break;
        }

        case Page::SyncTime:
            syncMenuSelected = ((syncMenuSelected + delta) % SYNC_BTN_COUNT + SYNC_BTN_COUNT) % SYNC_BTN_COUNT;
            // Only two buttons total -- redrawing both is cheap and avoids
            // tracking which one changed.
            drawSyncButton(tft, 0);
            drawSyncButton(tft, 1);
            break;

        case Page::System: {
            int level = (int)Backlight::getLevel() + delta * BRIGHTNESS_STEP;
            Backlight::setLevel((uint8_t)constrain(level, 0, 100));
            drawBrightnessValue(tft);
            break;
        }

        case Page::TimeZone: {
            int offset = TimeSync::utcOffsetMinutes() + delta * TIMEZONE_STEP_MINUTES;
            TimeSync::setUtcOffsetMinutes(constrain(offset, TIMEZONE_MIN_MINUTES, TIMEZONE_MAX_MINUTES));
            drawTimeZoneValue(tft);
            break;
        }

        case Page::BluetoothSync:
        case Page::About:
            break; // read-only pages
    }
}

Settings::Action Settings::press(TFT_eSPI &tft) {
    if (page == Page::Menu) {
        switch (menuSelected) {
            case 0: page = Page::SyncTime; break;
            case 1: page = Page::System; break;
            case 2: page = Page::TimeZone; break;
            case 3: page = Page::About; break;
        }
        drawCurrentPage(tft);
        return Action::None;
    }

    if (page == Page::SyncTime) {
        if (syncMenuSelected == 0) {
            return Action::OpenWifiList;
        }
        page = Page::BluetoothSync;
        drawCurrentPage(tft);
    }

    return Action::None;
}

bool Settings::back(TFT_eSPI &tft) {
    switch (page) {
        case Page::Menu:
            return false;
        case Page::BluetoothSync:
            page = Page::SyncTime;
            break;
        default:
            page = Page::Menu;
            break;
    }
    drawCurrentPage(tft);
    return true;
}
