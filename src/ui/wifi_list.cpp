#include "wifi_list.h"

#include "account_list.h"
#include "colors.h"
#include "wifi_manager.h"

namespace {
    constexpr int TOP = 34;
    constexpr int ROW_H = 24;
    constexpr int FOOTER_BAND_H = 14;

    int selected = 0;
    int scrollOffset = 0;

    int visibleRows(TFT_eSPI &tft) {
        return (tft.height() - FOOTER_BAND_H - TOP) / ROW_H;
    }

    void clampScrollOffset(int visible) {
        if (selected < scrollOffset) scrollOffset = selected;
        if (selected > scrollOffset + visible - 1) scrollOffset = selected - visible + 1;
    }

    void drawRow(TFT_eSPI &tft, int y, int h, const WifiManager::Network &net, bool sel) {
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        // Clear the row's own rectangle first so an unselected row erases
        // its own leftover highlight when only this row is being redrawn.
        tft.fillRect(4, y, tft.width() - 8, h - 4, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(4, y, tft.width() - 8, h - 4, 4, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, h - 4, 4, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString(net.ssid, 14, y + h / 2 - 8, 2);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_DARKGREY, rowBg);
        tft.drawString(net.open ? "open" : "locked", tft.width() - 14, y + h / 2 - 8, 1);
    }

    // Redraws every row in the visible window (not the title/divider/
    // footer, which don't change here) -- used both for the initial draw
    // and whenever the window shifts.
    void drawRows(TFT_eSPI &tft) {
        int visible = visibleRows(tft);
        int count = WifiManager::networkCount();

        if (count == 0) {
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawString("No networks found", tft.width() / 2, tft.height() / 2, 2);
            return;
        }

        for (int slot = 0; slot < visible; slot++) {
            int row = scrollOffset + slot;
            if (row >= count) break;
            drawRow(tft, TOP + slot * ROW_H, ROW_H, WifiManager::networkAt(row), row == selected);
        }
    }

    // Redraws a single row in place, if it's currently within the visible
    // window.
    void redrawRowAt(TFT_eSPI &tft, int row) {
        int slot = row - scrollOffset;
        if (slot < 0 || slot >= visibleRows(tft)) return;
        drawRow(tft, TOP + slot * ROW_H, ROW_H, WifiManager::networkAt(row), row == selected);
    }

    void draw(TFT_eSPI &tft) {
        int visible = visibleRows(tft);
        clampScrollOffset(visible);

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString("WiFi Networks", 10, 6, 4);
        tft.drawFastHLine(0, 26, tft.width(), TOKEN_BLUE);

        drawRows(tft);
        AccountList::drawIdleFooter(tft);
    }
}

void WifiList::enter(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Scanning...", tft.width() / 2, tft.height() / 2, 2);

    WifiManager::scan();

    selected = 0;
    scrollOffset = 0;
    draw(tft);
}

void WifiList::redraw(TFT_eSPI &tft) {
    draw(tft);
}

void WifiList::scroll(TFT_eSPI &tft, int delta) {
    int count = WifiManager::networkCount();
    if (count == 0) return;

    int oldSelected = selected;
    selected = ((selected + delta) % count + count) % count;

    int oldScrollOffset = scrollOffset;
    clampScrollOffset(visibleRows(tft));

    if (scrollOffset != oldScrollOffset) {
        drawRows(tft);
        return;
    }

    redrawRowAt(tft, oldSelected);
    redrawRowAt(tft, selected);
}

bool WifiList::selectedIsOpen() {
    if (WifiManager::networkCount() == 0) return false;
    return WifiManager::networkAt(selected).open;
}

const String &WifiList::selectedSsid() {
    static String empty = "";
    if (WifiManager::networkCount() == 0) return empty;
    return WifiManager::networkAt(selected).ssid;
}
