#include "account_list.h"

#include <ctime>

#include "account_store.h"
#include "battery.h"
#include "colors.h"
#include "time_sync.h"
#include "totp.h"

namespace {
    constexpr int FOOTER_BAND_H = 14;
    constexpr int TOP = 32;
    constexpr int ROW_H = 38; // 34 plus room for the countdown bar at the bottom of each row
    constexpr int VISIBLE_ROWS = 3;
    constexpr const char *IDLE_HINT = "hold side button 5s to power off";

    int selected = 0;
    int scrollOffset = 0;

    // One extra row for Settings, always last, reached by scrolling past
    // the final account. Computed rather than cached -- the account count
    // can grow at runtime (AccountLink adding one over serial).
    int rowCount() { return AccountStore::count() + 1; }
    int settingsRow() { return AccountStore::count(); }

    void clampScrollOffset() {
        if (selected < scrollOffset) scrollOffset = selected;
        if (selected > scrollOffset + VISIBLE_ROWS - 1) scrollOffset = selected - VISIBLE_ROWS + 1;
    }

    // Just the code text -- factored out of drawAccountRow so the header
    // refresh tick can update it on its own every second (codes roll over
    // every period) without touching the name/issuer/highlight, which
    // don't change nearly as often.
    void drawAccountCode(TFT_eSPI &tft, int y, const AccountStore::Account &account, bool sel) {
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        // Without a synced clock, a "code" would just be wrong rather than
        // absent -- show dashes instead of something that looks valid.
        char code[10] = "------";
        if (TimeSync::isSynced()) {
            Totp::generate(account.secret, account.digits, account.period, TimeSync::now(), code, sizeof(code));
        }

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TOKEN_BLUE_LIGHT, rowBg);
        tft.drawString(code, tft.width() - 16, y + 3, 4);
    }

    // A thin bar draining left-to-right over the account's period as the
    // current code counts down to its next rotation. Redrawn on the same
    // per-second cadence as the code text (see refreshCodes) -- the track
    // is always repainted full-width first so a shrinking fill doesn't
    // leave a stale sliver of the previous, longer bar behind.
    void drawAccountTimer(TFT_eSPI &tft, int y, const AccountStore::Account &account, bool sel) {
        int barX = 16, barY = y + ROW_H - 10, barW = tft.width() - 32, barH = 2;

        tft.fillRect(barX, barY, barW, barH, TFT_DARKGREY);
        if (!TimeSync::isSynced()) return; // no meaningful countdown without a synced clock

        uint32_t period = account.period > 0 ? account.period : AccountStore::DEFAULT_PERIOD;
        uint32_t elapsed = (uint32_t)(TimeSync::now() % period);
        int fillW = barW - (int)((uint64_t)barW * elapsed / period);

        tft.fillRect(barX, barY, fillW, barH, sel ? TOKEN_BLUE_LIGHT : TOKEN_BLUE);
    }

    void drawAccountRow(TFT_eSPI &tft, int y, const AccountStore::Account &account, bool sel) {
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        // Always clear the row's own rectangle first -- redrawing a single
        // row in place (instead of the whole screen) means a row going
        // from selected to unselected has to erase its own highlight
        // outline, since nothing else is about to paint over it.
        tft.fillRect(4, y, tft.width() - 8, ROW_H - 4, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(4, y, tft.width() - 8, ROW_H - 4, 6, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, ROW_H - 4, 6, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString(account.name, 16, y + 5, 2);

        tft.setTextColor(TFT_DARKGREY, rowBg);
        tft.drawString(account.issuer, 16, y + 20, 1);

        drawAccountCode(tft, y, account, sel);
        drawAccountTimer(tft, y, account, sel);
    }

    void drawSettingsRow(TFT_eSPI &tft, int y, bool sel) {
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        tft.fillRect(4, y, tft.width() - 8, ROW_H - 4, TFT_BLACK);
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

    void drawRowByIndex(TFT_eSPI &tft, int row, int y, bool sel) {
        if (row == settingsRow()) {
            drawSettingsRow(tft, y, sel);
        } else {
            drawAccountRow(tft, y, AccountStore::at(row), sel);
        }
    }

    // Redraws every row currently in the visible window. Used when the
    // window itself shifts (scrolling past its top/bottom edge) -- every
    // visible row changes, but the header/footer don't, so this alone is
    // enough without a full-screen clear.
    void drawRows(TFT_eSPI &tft) {
        for (int slot = 0; slot < VISIBLE_ROWS; slot++) {
            int row = scrollOffset + slot;
            if (row >= rowCount()) break;
            drawRowByIndex(tft, row, TOP + slot * ROW_H, row == selected);
        }
    }

    // Redraws a single row in place, if it's currently within the visible
    // window (a no-op otherwise -- nothing on screen to touch).
    void redrawRowAt(TFT_eSPI &tft, int row) {
        int slot = row - scrollOffset;
        if (slot < 0 || slot >= VISIBLE_ROWS) return;
        drawRowByIndex(tft, row, TOP + slot * ROW_H, row == selected);
    }

    // Small chevrons in the header/footer margin hinting that the list
    // continues past the visible window.
    void drawScrollHints(TFT_eSPI &tft) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(scrollOffset > 0 ? TOKEN_BLUE : TFT_BLACK, TFT_BLACK);
        tft.drawString("^", tft.width() - 6, TOP - 16, 2);

        bool moreBelow = scrollOffset + VISIBLE_ROWS < rowCount();
        tft.setTextColor(moreBelow ? TOKEN_BLUE : TFT_BLACK, TFT_BLACK);
        tft.drawString("v", tft.width() - 6, TOP + VISIBLE_ROWS * ROW_H + 2, 2);
    }

    // Shown in the empty row slots below Settings when there are no
    // accounts yet -- otherwise the screen is just a lone "Settings" row
    // with no clue what to do next.
    void drawEmptyHint(TFT_eSPI &tft) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("no accounts yet", tft.width() / 2, TOP + ROW_H + 12, 1);
        tft.drawString("Settings > Add Account to add one", tft.width() / 2, TOP + ROW_H + 26, 1);
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

    drawRows(tft);
    if (AccountStore::count() == 0) drawEmptyHint(tft);
    drawScrollHints(tft);
    drawIdleFooter(tft);
}

void AccountList::scroll(TFT_eSPI &tft, int delta) {
    int oldSelected = selected;
    int rows = rowCount();
    selected = ((selected + delta) % rows + rows) % rows;

    int oldScrollOffset = scrollOffset;
    clampScrollOffset();

    if (scrollOffset != oldScrollOffset) {
        // The window shifted -- every visible row changed, but the
        // header/footer/title didn't, so just repaint the rows and the
        // scroll chevrons rather than the whole screen.
        drawRows(tft);
        drawScrollHints(tft);
        return;
    }

    redrawRowAt(tft, oldSelected);
    redrawRowAt(tft, selected);
}

bool AccountList::isSettingsSelected() {
    return selected == settingsRow();
}

int AccountList::selectedAccountIndex() {
    return isSettingsSelected() ? -1 : selected;
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

void AccountList::refreshCodes(TFT_eSPI &tft) {
    for (int slot = 0; slot < VISIBLE_ROWS; slot++) {
        int row = scrollOffset + slot;
        if (row >= rowCount() || row == settingsRow()) continue;
        int y = TOP + slot * ROW_H;
        const AccountStore::Account &account = AccountStore::at(row);
        bool sel = row == selected;
        drawAccountCode(tft, y, account, sel);
        drawAccountTimer(tft, y, account, sel);
    }
}
