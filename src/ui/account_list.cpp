#include "account_list.h"

#include "colors.h"

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
}

void AccountList::draw(TFT_eSPI &tft) {
    clampScrollOffset();

    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
    tft.drawString("TOKEN", 10, 6, 4);
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
