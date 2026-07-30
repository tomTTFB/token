#include "boot_screen.h"

#include "colors.h"
#include "matrix_rain.h"

namespace {
    constexpr uint32_t RAIN_ONLY_MS = 900;  // rain fills the screen alone first
    constexpr uint32_t LOGO_HOLD_MS = 1800; // then the logo sits on top of the rain
    constexpr uint8_t FRAME_DELAY_MS = 15;

    const char *KEY_ART[] = {
        "   ___",
        "  /   \\",
        " |  o  |===]",
        "  \\___/",
    };
    constexpr int KEY_ART_LINES = 4;

    void drawLogo(TFT_eSPI &tft) {
        const int cx = tft.width() / 2;
        int y = tft.height() / 2 - 52;

        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.setTextFont(1);
        tft.setTextSize(2);

        // Measure the widest line and left-align every line to that single
        // block. Centering each line on its own (TC_DATUM per line) shifts
        // a different amount per line since they're different lengths,
        // which throws the vertical strokes out of alignment between rows
        // and breaks the key silhouette.
        int blockWidth = 0;
        for (int i = 0; i < KEY_ART_LINES; i++) {
            blockWidth = max(blockWidth, (int)tft.textWidth(KEY_ART[i]));
        }
        const int blockX = cx - blockWidth / 2;

        tft.setTextDatum(TL_DATUM);
        for (int i = 0; i < KEY_ART_LINES; i++) {
            tft.drawString(KEY_ART[i], blockX, y);
            y += 16;
        }

        tft.setTextDatum(TC_DATUM);
        tft.setTextSize(1);
        tft.drawString("TOKEN", cx, y + 10, 4);
    }
}

void BootScreen::play(TFT_eSPI &tft) {
    MatrixRain::begin(tft);

    uint32_t start = millis();
    while (millis() - start < RAIN_ONLY_MS) {
        MatrixRain::update(tft);
        delay(FRAME_DELAY_MS);
    }

    uint32_t logoStart = millis();
    while (millis() - logoStart < LOGO_HOLD_MS) {
        MatrixRain::update(tft);
        drawLogo(tft);
        delay(FRAME_DELAY_MS);
    }
}
