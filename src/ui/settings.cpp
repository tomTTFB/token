#include "settings.h"

#include <Arduino.h>

#include "account_list.h"
#include "backlight.h"
#include "colors.h"

namespace {
    enum class Page { Menu, SyncTime, BluetoothSync, System, About };

    const char *MENU_ITEMS[] = {"Sync Time", "System", "About"};
    constexpr int MENU_COUNT = 3;
    constexpr int MENU_TOP = 34;
    constexpr int MENU_ROW_H = 26;
    constexpr uint8_t BRIGHTNESS_STEP = 5;

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

    void drawMenu(TFT_eSPI &tft) {
        drawHeader(tft, "Settings");

        for (int i = 0; i < MENU_COUNT; i++) {
            int y = MENU_TOP + i * MENU_ROW_H;
            bool sel = i == menuSelected;
            uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

            if (sel) {
                tft.fillRoundRect(4, y, tft.width() - 8, MENU_ROW_H - 4, 6, TOKEN_BLUE_DIM);
                tft.drawRoundRect(4, y, tft.width() - 8, MENU_ROW_H - 4, 6, TOKEN_BLUE);
            }

            tft.setTextDatum(TL_DATUM);
            tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
            tft.drawString(MENU_ITEMS[i], 16, y + 5, 2);
        }

        AccountList::drawIdleFooter(tft);
    }

    void drawWifiIcon(TFT_eSPI &tft, int cx, int cy, uint16_t color) {
        tft.fillCircle(cx, cy, 3, color);
        tft.drawArc(cx, cy, 12, 9, 270, 90, color, TFT_BLACK, true);
        tft.drawArc(cx, cy, 21, 18, 270, 90, color, TFT_BLACK, true);
        tft.drawArc(cx, cy, 30, 27, 270, 90, color, TFT_BLACK, true);
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

        tft.drawLine(cx, top, cx, bottom, color);
        tft.drawLine(cx, top, xr, rLower, color);
        tft.drawLine(xr, rLower, cx, mid, color);
        tft.drawLine(cx, mid, xr, rUpper, color);
        tft.drawLine(xr, rUpper, cx, bottom, color);
    }

    void drawSyncTime(TFT_eSPI &tft) {
        drawHeader(tft, "Sync Time");

        int btnW = (tft.width() - SYNC_BTN_GAP * (SYNC_BTN_COUNT + 1)) / SYNC_BTN_COUNT;
        const char *labels[SYNC_BTN_COUNT] = {"WiFi", "Bluetooth"};

        for (int i = 0; i < SYNC_BTN_COUNT; i++) {
            int x = SYNC_BTN_GAP + i * (btnW + SYNC_BTN_GAP);
            bool sel = i == syncMenuSelected;
            uint16_t accent = sel ? TOKEN_BLUE : TFT_DARKGREY;

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

        AccountList::drawIdleFooter(tft);
    }

    void drawBluetoothSync(TFT_eSPI &tft) {
        drawHeader(tft, "Bluetooth Sync");

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Coming soon", tft.width() / 2, tft.height() / 2, 2);

        AccountList::drawIdleFooter(tft);
    }

    void drawSystem(TFT_eSPI &tft) {
        drawHeader(tft, "System");

        uint8_t level = Backlight::getLevel();

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Brightness", 16, 38, 2);

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

        drawInfoRow(tft, 100, "Free heap", String(ESP.getFreeHeap() / 1024) + " KB");
        drawInfoRow(tft, 116, "Uptime", String(millis() / 1000) + " s");

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
        case Page::Menu:
            menuSelected = ((menuSelected + delta) % MENU_COUNT + MENU_COUNT) % MENU_COUNT;
            drawCurrentPage(tft);
            break;

        case Page::SyncTime:
            syncMenuSelected = ((syncMenuSelected + delta) % SYNC_BTN_COUNT + SYNC_BTN_COUNT) % SYNC_BTN_COUNT;
            drawCurrentPage(tft);
            break;

        case Page::System: {
            int level = (int)Backlight::getLevel() + delta * BRIGHTNESS_STEP;
            Backlight::setLevel((uint8_t)constrain(level, 0, 100));
            drawCurrentPage(tft);
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
            case 2: page = Page::About; break;
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
