#include "account_list.h"

#include <ctime>

#include "battery.h"
#include "colors.h"
#include "time_sync.h"

namespace {
    struct Account {
        const char *name;
        const char *issuer;
        const char *code;
    };

    const Account ACCOUNTS[] = {
        {"GitHub", "you@example.com", "000000"},
        {"Google", "you@gmail.com", "000000"},
        {"AWS", "root", "000000"},
        {"Proton Mail", "you@proton.me", "000000"},
    };
    constexpr int ACCOUNT_COUNT = 4;

    // One extra row for Settings, always last, reached by scrolling past
    // the final account.
    constexpr int ROW_COUNT = ACCOUNT_COUNT + 1;
    constexpr int SETTINGS_ROW = ACCOUNT_COUNT;

    constexpr int FOOTER_BAND_H = 14;
    constexpr int TOP = 32;
    constexpr int ROW_H = 34;
    constexpr int VISIBLE_ROWS = 3;
    constexpr const char *IDLE_HINT = "hold side button 5s to power off";

    int selected = 0;
    int scrollOffset = 0;

    void clampScrollOffset() {
        if (selected < scrollOffset) scrollOffset = selected;
        if (selected > scrollOffset + VISIBLE_ROWS - 1) scrollOffset = selected - VISIBLE_ROWS + 1;
    }

    void drawAccountRow(TFT_eSPI &tft, int y, const Account &account, bool sel) {
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        if (sel) {
            tft.fillRoundRect(4, y, tft.width() - 8, ROW_H - 4, 6, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, ROW_H - 4, 6, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString(account.name, 16, y + 5, 2);

        tft.setTextColor(TFT_DARKGREY, rowBg);
        tft.drawString(account.issuer, 16, y + 20, 1);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TOKEN_BLUE_LIGHT, rowBg);
        tft.drawString(account.code, tft.width() - 16, y + 3, 4);
    }

    void drawSettingsRow(TFT_eSPI &tft, int y, bool sel) {
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        if (sel) {
            tft.fillRoundRect(4, y, tft.width() - 8, ROW_H - 4, 6, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, ROW_H - 4, 6, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString("Settings", 16, y + 5, 2);

        tft.setTextColor(TFT_DARKGREY, rowBg);
        tft.drawString("sync time, system, about", 16, y + 20, 1);
    }

    // Small chevrons in the header/footer margin hinting that the list
    // continues past the visible window.
    void drawScrollHints(TFT_eSPI &tft) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(scrollOffset > 0 ? TOKEN_BLUE : TFT_BLACK, TFT_BLACK);
        tft.drawString("^", tft.width() - 6, TOP - 16, 2);

        bool moreBelow = scrollOffset + VISIBLE_ROWS < ROW_COUNT;
        tft.setTextColor(moreBelow ? TOKEN_BLUE : TFT_BLACK, TFT_BLACK);
        tft.drawString("v", tft.width() - 6, TOP + VISIBLE_ROWS * ROW_H + 2, 2);
    }

    constexpr int HEADER_FONT = 1;
    constexpr int BATT_TOP = 6;
    constexpr int BATT_BODY_W = 18;
    constexpr int BATT_BODY_H = 9;
    constexpr int BATT_NUB_W = 2;

    // Battery percentage + glyph and the synced clock, right-aligned above
    // the header divider. Kept separate from draw() so it can be refreshed
    // on its own every second without redrawing the whole list.
    void drawHeaderWidgets(TFT_eSPI &tft) {
        int right = tft.width() - 6;

        tft.fillRect(right - 96, 0, 96, 26, TFT_BLACK);

        int bodyLeft = right - BATT_NUB_W - BATT_BODY_W;
        int bodyRight = right - BATT_NUB_W;
        uint16_t battOutline = Battery::isCharging() ? TOKEN_BLUE : TFT_DARKGREY;

        tft.drawRect(bodyLeft, BATT_TOP, BATT_BODY_W, BATT_BODY_H, battOutline);
        tft.fillRect(bodyRight, BATT_TOP + (BATT_BODY_H - 4) / 2, BATT_NUB_W, 4, battOutline);

        int fillMaxW = BATT_BODY_W - 2;
        int fillW = Battery::isAvailable() ? (fillMaxW * Battery::percent() / 100) : 0;
        uint16_t fillColor = (Battery::isAvailable() && Battery::percent() <= 15) ? TFT_RED : TOKEN_BLUE;
        tft.fillRect(bodyLeft + 1, BATT_TOP + 1, fillW, BATT_BODY_H - 2, fillColor);
        tft.fillRect(bodyLeft + 1 + fillW, BATT_TOP + 1, fillMaxW - fillW, BATT_BODY_H - 2, TFT_BLACK);

        char pct[6];
        if (Battery::isAvailable()) {
            snprintf(pct, sizeof(pct), "%d%%", Battery::percent());
        } else {
            snprintf(pct, sizeof(pct), "--");
        }

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        int pctRight = bodyLeft - 4;
        tft.drawString(pct, pctRight, BATT_TOP, HEADER_FONT);

        char timeStr[6] = "--:--";
        if (TimeSync::isSynced()) {
            time_t t = TimeSync::localNow();
            struct tm tmVal;
            gmtime_r(&t, &tmVal);
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tmVal.tm_hour, tmVal.tm_min);
        }

        int timeRight = pctRight - tft.textWidth(pct, HEADER_FONT) - 8;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(timeStr, timeRight, BATT_TOP, HEADER_FONT);
    }
}

void AccountList::draw(TFT_eSPI &tft) {
    clampScrollOffset();

    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
    tft.drawString("TOKEN", 10, 6, 4);
    drawHeaderWidgets(tft);
    tft.drawFastHLine(0, 26, tft.width(), TOKEN_BLUE);

    for (int slot = 0; slot < VISIBLE_ROWS; slot++) {
        int row = scrollOffset + slot;
        if (row >= ROW_COUNT) break;

        int y = TOP + slot * ROW_H;
        bool sel = row == selected;

        if (row == SETTINGS_ROW) {
            drawSettingsRow(tft, y, sel);
        } else {
            drawAccountRow(tft, y, ACCOUNTS[row], sel);
        }
    }

    drawScrollHints(tft);
    drawIdleFooter(tft);
}

void AccountList::scroll(TFT_eSPI &tft, int delta) {
    selected = ((selected + delta) % ROW_COUNT + ROW_COUNT) % ROW_COUNT;
    draw(tft);
}

bool AccountList::isSettingsSelected() {
    return selected == SETTINGS_ROW;
}

void AccountList::drawFooter(TFT_eSPI &tft, const String &text, uint16_t color) {
    int y0 = tft.height() - FOOTER_BAND_H;
    tft.fillRect(0, y0, tft.width(), FOOTER_BAND_H, TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text, tft.width() / 2, tft.height() - 8, 1);
}

void AccountList::drawIdleFooter(TFT_eSPI &tft) {
    drawFooter(tft, IDLE_HINT, TFT_DARKGREY);
}

void AccountList::refreshHeaderWidgets(TFT_eSPI &tft) {
    drawHeaderWidgets(tft);
}
