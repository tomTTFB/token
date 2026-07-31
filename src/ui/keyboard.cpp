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
    constexpr int PROMPT_Y = 4;
    constexpr int BUFFER_Y = 20;
    constexpr int HEADER_LINE_Y = 38; // below the buffer text's ~16px height, so it doesn't clip it
    constexpr int HEADER_H = 40;
    constexpr int FOOTER_BAND_H = 14;
    constexpr int KEY_FONT = 2;
    constexpr int LEFT_MARGIN = 13;

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

    // The prompt label and divider line are static for the life of the
    // screen -- drawn once on entry, never touched again.
    void drawPromptAndLine(TFT_eSPI &tft) {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString(promptText, LEFT_MARGIN, PROMPT_Y, 2);
        tft.drawFastHLine(0, HEADER_LINE_Y, tft.width(), TOKEN_BLUE);
    }

    // The typed-so-far buffer, redrawn on every keypress/backspace. Clears
    // a fixed-size band first (rather than relying on drawString's
    // background fill) so a shrinking buffer doesn't leave stale
    // characters behind from a longer previous string.
    void drawBuffer(TFT_eSPI &tft) {
        tft.fillRect(0, BUFFER_Y - 2, tft.width(), 20, TFT_BLACK);

        String shown = String(buf) + "_";
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(shown, LEFT_MARGIN, BUFFER_Y, 2);
    }

    void charCellRect(TFT_eSPI &tft, int row, int col, int &x, int &y, int &w, int &h) {
        h = rowHeight(tft);
        y = keyboardTop() + row * h;
        int len = rowLength(row);
        w = tft.width() / len;
        x = col * w;
    }

    void actionCellRect(TFT_eSPI &tft, int col, int &x, int &y, int &w, int &h) {
        h = rowHeight(tft);
        y = keyboardTop() + ACTION_ROW * h;
        w = tft.width() / 2;
        x = col * w;
    }

    void drawCell(TFT_eSPI &tft, int row, int col, bool sel) {
        int x, y, w, h;

        if (row == ACTION_ROW) {
            actionCellRect(tft, col, x, y, w, h);
            const char *labels[2] = {"DEL", "DONE"};

            tft.fillRect(x, y, w, h, TFT_BLACK);
            tft.fillRoundRect(x + 6, y + 2, w - 12, h - 4, 4, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
            tft.drawRoundRect(x + 6, y + 2, w - 12, h - 4, 4, sel ? TOKEN_BLUE : TFT_DARKGREY);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
            tft.drawString(labels[col], x + w / 2, y + h / 2, KEY_FONT);
            return;
        }

        charCellRect(tft, row, col, x, y, w, h);

        // Clear the cell's own rectangle first: redrawing just this one
        // cell (instead of the whole grid) means an unselected cell has to
        // erase its own leftover highlight, since nothing else repaints it.
        tft.fillRect(x, y, w, h, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 3, TOKEN_BLUE_DIM);
            tft.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 3, TOKEN_BLUE);
        }

        char label[2] = {ROWS[row][col], '\0'};
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
        tft.drawString(label, x + w / 2, y + h / 2, KEY_FONT);
    }

    void drawGrid(TFT_eSPI &tft) {
        for (int row = 0; row < TOTAL_ROWS; row++) {
            int len = rowLength(row);
            for (int col = 0; col < len; col++) {
                drawCell(tft, row, col, row == cursorRow && col == cursorCol);
            }
        }
    }
}

void Keyboard::enter(TFT_eSPI &tft, const char *prompt) {
    promptText = prompt;
    buf[0] = '\0';
    bufLen = 0;
    cursorRow = 0;
    cursorCol = 0;

    tft.fillScreen(TFT_BLACK);
    drawPromptAndLine(tft);
    drawBuffer(tft);
    drawGrid(tft);
    AccountList::drawIdleFooter(tft);
}

void Keyboard::scroll(TFT_eSPI &tft, int delta) {
    int total = totalCells();
    int index = flatten(cursorRow, cursorCol);
    index = ((index + delta) % total + total) % total;

    int oldRow = cursorRow, oldCol = cursorCol;
    unflatten(index);

    // Only the two cells whose selection state actually changed need to be
    // touched -- not the whole grid.
    drawCell(tft, oldRow, oldCol, false);
    drawCell(tft, cursorRow, cursorCol, true);
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
        drawBuffer(tft);
    }
    return false;
}

bool Keyboard::backspace(TFT_eSPI &tft) {
    if (bufLen == 0) return false;
    buf[--bufLen] = '\0';
    drawBuffer(tft);
    return true;
}

const char *Keyboard::buffer() {
    return buf;
}
