#include "matrix_rain.h"

#include "colors.h"

namespace {
    // drawChar(x, y, c, color, bg, size) always uses the built-in 5x7 GLCD
    // glyph regardless of which fonts are loaded, and its box is 6x8. Space
    // columns/rows on a wider pitch than that box so a gap shows between
    // neighboring characters instead of them touching edge to edge.
    constexpr int CELL_W = 10;
    constexpr int CELL_H = 13;

    constexpr uint8_t TRAIL_LEN = 5;     // dimming steps behind the head
    constexpr uint16_t STEP_MIN_MS = 45; // fastest column fall speed
    constexpr uint16_t STEP_MAX_MS = 140; // slowest column fall speed

    const char CHARSET[] = "01ABCDEFGHIJKLMNOPQRSTUVWXYZ$%#*+=<>/\\|";
    constexpr int CHARSET_LEN = sizeof(CHARSET) - 1;

    struct Column {
        int16_t headRow;
        uint16_t stepMs;
        uint32_t lastStepAt;
    };

    Column *columns = nullptr;
    int colCount = 0;
    int rowCount = 0;
    uint16_t shadeColors[TRAIL_LEN + 1]; // [0] head (brightest) .. [TRAIL_LEN] darkest

    char randChar() {
        return CHARSET[random(CHARSET_LEN)];
    }

    void resetColumn(Column &c) {
        c.headRow = -random(0, rowCount > 0 ? rowCount : 1);
        c.stepMs = random(STEP_MIN_MS, STEP_MAX_MS);
        c.lastStepAt = millis();
    }

    void drawCell(TFT_eSPI &tft, int col, int row, char ch, uint16_t color) {
        if (row < 0 || row >= rowCount) return;
        tft.drawChar(col * CELL_W, row * CELL_H, ch, color, TFT_BLACK, 1);
    }
}

void MatrixRain::begin(TFT_eSPI &tft) {
    colCount = tft.width() / CELL_W;
    rowCount = tft.height() / CELL_H;

    delete[] columns;
    columns = new Column[colCount];

    for (int i = 0; i <= TRAIL_LEN; i++) {
        float t = (float)i / TRAIL_LEN;
        shadeColors[i] = rgb565(
            (uint8_t)(TOKEN_BLUE_R * (1.0f - t)),
            (uint8_t)(TOKEN_BLUE_G * (1.0f - t)),
            (uint8_t)(TOKEN_BLUE_B * (1.0f - t)));
    }

    tft.fillScreen(TFT_BLACK);
    for (int i = 0; i < colCount; i++) resetColumn(columns[i]);
}

void MatrixRain::update(TFT_eSPI &tft) {
    uint32_t now = millis();
    for (int i = 0; i < colCount; i++) {
        Column &c = columns[i];
        if (now - c.lastStepAt < c.stepMs) continue;
        c.lastStepAt = now;
        c.headRow++;

        drawCell(tft, i, c.headRow, randChar(), shadeColors[0]);
        for (int s = 1; s <= TRAIL_LEN; s++) {
            drawCell(tft, i, c.headRow - s, randChar(), shadeColors[s]);
        }
        drawCell(tft, i, c.headRow - TRAIL_LEN - 1, ' ', TFT_BLACK);

        if (c.headRow - TRAIL_LEN > rowCount) resetColumn(c);
    }
}
