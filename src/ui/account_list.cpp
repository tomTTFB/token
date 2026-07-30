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
    constexpr int SELECTED = 0;
}

void AccountList::draw(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
    tft.drawString("TOKEN", 10, 6, 4);
    tft.drawFastHLine(0, 26, tft.width(), TOKEN_BLUE);

    const int rowH = 34;
    const int top = 32;

    for (int i = 0; i < ACCOUNT_COUNT; i++) {
        int y = top + i * rowH;
        if (y + rowH > tft.height() - 12) break;

        uint16_t rowBg = i == SELECTED ? TOKEN_BLUE_DIM : TFT_BLACK;

        if (i == SELECTED) {
            tft.fillRoundRect(4, y, tft.width() - 8, rowH - 4, 6, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, rowH - 4, 6, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(i == SELECTED ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString(ACCOUNTS[i].name, 16, y + 5, 2);

        tft.setTextColor(TFT_DARKGREY, rowBg);
        tft.drawString(ACCOUNTS[i].issuer, 16, y + 20, 1);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TOKEN_BLUE, rowBg);
        tft.drawString(ACCOUNTS[i].code, tft.width() - 16, y + 8, 2);
    }

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("hold side button 5s to power off", tft.width() / 2, tft.height() - 8, 1);
}
