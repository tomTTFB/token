#include "keyboard.h"

#include <cstring>

#include "account_list.h"
#include "colors.h"

namespace {
    const char *ROWS[] = {
        "abcdefghijklm",
        "nopqrstuvwxyz",
        "ABCDEFGHIJKLM",
        "NOPQRSTUVWXYZ",
        "0123456789",
        "!\"#$%&*()-_=+",
    };
    constexpr int CHAR_ROWS = 6;

    // DEL and DONE live on their own row, past the character rows.
    constexpr int ACTION_ROW = CHAR_ROWS;
    constexpr int TOTAL_ROWS = CHAR_ROWS + 1;

    constexpr int MAX_BUFFER = 63;
    constexpr int HEADER_H = 34;
    constexpr int FOOTER_BAND_H = 14;
    constexpr int KEY_FONT = 2;

    char buf[MAX_BUFFER + 1] = "";
    int bufLen = 0;

    int cursorRow = 0;
    int cursorCol = 0;

    const char *promptText = "";

    int rowLength(int row) {
        return row == ACTION_ROW ? 2 : (int)strlen(ROWS[row]);
    }

    int totalCells() {
        int total = 0;
        for (int r = 0; r < TOTAL_ROWS; r++) total += rowLength(r);
        return total;
    }

    int flatten(int row, int col) {
        int index = 0;
        for (int r = 0; r < row; r++) index += rowLength(r);
        return index + col;
    }

    void unflatten(int index) {
        for (int r = 0; r < TOTAL_ROWS; r++) {
            int len = rowLength(r);
            if (index < len) {
                cursorRow = r;
                cursorCol = index;
                return;
            }
            index -= len;
        }
    }

    int keyboardTop() { return HEADER_H; }
    int keyboardHeight(TFT_eSPI &tft) { return tft.height() - FOOTER_BAND_H - HEADER_H; }
    int rowHeight(TFT_eSPI &tft) { return keyboardHeight(tft) / TOTAL_ROWS; }

    void drawHeader(TFT_eSPI &tft) {
        tft.fillRect(0, 0, tft.width(), HEADER_H - 2, TFT_BLACK);

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString(promptText, 10, 4, 2);

        String shown = String(buf) + "_";
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(shown, 10, 20, 2);

        tft.drawFastHLine(0, HEADER_H - 2, tft.width(), TOKEN_BLUE);
    }

    void drawCharRow(TFT_eSPI &tft, int row, int y, int h) {
        const char *chars = ROWS[row];
        int len = strlen(chars);
        int cellW = tft.width() / len;

        for (int c = 0; c < len; c++) {
            bool sel = row == cursorRow && c == cursorCol;
            int x = c * cellW;

            if (sel) {
                tft.fillRoundRect(x + 1, y + 1, cellW - 2, h - 2, 3, TOKEN_BLUE_DIM);
                tft.drawRoundRect(x + 1, y + 1, cellW - 2, h - 2, 3, TOKEN_BLUE);
            }

            char label[2] = {chars[c], '\0'};
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
            tft.drawString(label, x + cellW / 2, y + h / 2, KEY_FONT);
        }
    }

    void drawActionRow(TFT_eSPI &tft, int y, int h) {
        int cellW = tft.width() / 2;
        const char *labels[2] = {"DEL", "DONE"};

        for (int c = 0; c < 2; c++) {
            bool sel = cursorRow == ACTION_ROW && c == cursorCol;
            int x = c * cellW;

            tft.fillRoundRect(x + 4, y + 2, cellW - 8, h - 4, 4, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
            tft.drawRoundRect(x + 4, y + 2, cellW - 8, h - 4, 4, sel ? TOKEN_BLUE : TFT_DARKGREY);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
            tft.drawString(labels[c], x + cellW / 2, y + h / 2, KEY_FONT);
        }
    }

    void drawGrid(TFT_eSPI &tft) {
        int h = rowHeight(tft);
        int y = keyboardTop();

        tft.fillRect(0, y, tft.width(), keyboardHeight(tft), TFT_BLACK);

        for (int row = 0; row < CHAR_ROWS; row++) {
            drawCharRow(tft, row, y, h);
            y += h;
        }
        drawActionRow(tft, y, h);
    }
}

void Keyboard::enter(TFT_eSPI &tft, const char *prompt) {
    promptText = prompt;
    buf[0] = '\0';
    bufLen = 0;
    cursorRow = 0;
    cursorCol = 0;

    tft.fillScreen(TFT_BLACK);
    drawHeader(tft);
    drawGrid(tft);
    AccountList::drawIdleFooter(tft);
}

void Keyboard::scroll(TFT_eSPI &tft, int delta) {
    int total = totalCells();
    int index = flatten(cursorRow, cursorCol);
    index = ((index + delta) % total + total) % total;
    unflatten(index);
    drawGrid(tft);
}

bool Keyboard::press(TFT_eSPI &tft) {
    if (cursorRow == ACTION_ROW) {
        if (cursorCol == 0) {
            backspace(tft);
        } else {
            return true; // DONE
        }
    } else if (bufLen < MAX_BUFFER) {
        buf[bufLen++] = ROWS[cursorRow][cursorCol];
        buf[bufLen] = '\0';
        drawHeader(tft);
    }
    return false;
}

bool Keyboard::backspace(TFT_eSPI &tft) {
    if (bufLen == 0) return false;
    buf[--bufLen] = '\0';
    drawHeader(tft);
    return true;
}

const char *Keyboard::buffer() {
    return buf;
}
