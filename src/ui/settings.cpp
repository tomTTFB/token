#include "settings.h"

#include <Arduino.h>

#include "account_link.h"
#include "account_list.h"
#include "account_store.h"
#include "backlight.h"
#include "ble_time_sync.h"
#include "colors.h"
#include "pin_entry.h"
#include "pin_store.h"
#include "time_sync.h"
#include "wifi_auto.h"

namespace {
    enum class Page { Menu, SyncTime, BluetoothSync, AddAccount, ChangePin, Typing, System, TimeZone, WipeConfirm,
                       About };

    const char *MENU_ITEMS[] = {"Sync Time", "Add Account", "Change PIN", "Typing", "System", "Time Zone",
                                 "Wipe Device", "About"};
    constexpr int MENU_COUNT = 8;
    constexpr int MENU_TOP = 34;
    constexpr int MENU_ROW_H = 26;
    constexpr int MENU_VISIBLE_ROWS = 4;
    constexpr uint8_t BRIGHTNESS_STEP = 5;
    constexpr int TIMEZONE_STEP_MINUTES = 30;
    constexpr int TIMEZONE_MIN_MINUTES = -12 * 60;
    constexpr int TIMEZONE_MAX_MINUTES = 14 * 60;

    constexpr int SYNC_BTN_TOP = 30;
    constexpr int SYNC_BTN_H = 122;
    constexpr int SYNC_BTN_GAP = 8;
    constexpr int SYNC_BTN_COUNT = 2;

    Page page = Page::Menu;
    int menuSelected = 0;
    int menuScrollOffset = 0;
    int syncMenuSelected = 0; // 0 = WiFi, 1 = Bluetooth

    // Whether a click-to-type on the account list sends Enter after the
    // code. Off by default -- typing a code into the wrong field shouldn't
    // also submit it.
    bool enterAfterCode = false;

    // What drawBleSyncBody() last actually painted -- compared against the
    // live BleTimeSync state on every poll(). Comparing against the
    // previous poll's *reading* instead of this would miss a transition
    // that completes entirely between two polls (the whole connect ->
    // bond -> discover -> read chain can finish in well under one loop()
    // tick), leaving the screen stuck on a stale status forever.
    BleTimeSync::State lastDrawnBleState = BleTimeSync::State::Idle;

    // Same idea as lastDrawnBleState, but for the Add Account page --
    // compared against AccountLink's live state on every poll() so the
    // Listening -> Success/Failed transition (which can complete between
    // two poll() calls) always gets picked up.
    AccountLink::State lastDrawnLinkState = AccountLink::State::Idle;

    // Whether Change PIN's verify-old-PIN step has already succeeded --
    // distinguishes what an Unlocked/PinSet result from PinEntry::press()
    // means, since Change PIN drives PinEntry through two back-to-back
    // sub-flows (enterVerify then enterSetNew) that share the same Action
    // values.
    bool changePinVerified = false;

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

    void clampMenuScrollOffset() {
        if (menuSelected < menuScrollOffset) menuScrollOffset = menuSelected;
        if (menuSelected > menuScrollOffset + MENU_VISIBLE_ROWS - 1) {
            menuScrollOffset = menuSelected - MENU_VISIBLE_ROWS + 1;
        }
    }

    // row is the item's index into MENU_ITEMS; slot is its position within
    // the visible window, which is what actually determines its y.
    void drawMenuRow(TFT_eSPI &tft, int row, int slot) {
        int y = MENU_TOP + slot * MENU_ROW_H;
        bool sel = row == menuSelected;
        uint16_t rowBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;

        // Clear the row's own rectangle first so an unselected row erases
        // its own leftover highlight when only this row is redrawn.
        tft.fillRect(4, y, tft.width() - 8, MENU_ROW_H - 4, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(4, y, tft.width() - 8, MENU_ROW_H - 4, 6, TOKEN_BLUE_DIM);
            tft.drawRoundRect(4, y, tft.width() - 8, MENU_ROW_H - 4, 6, TOKEN_BLUE);
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, rowBg);
        tft.drawString(MENU_ITEMS[row], 16, y + 5, 2);
    }

    // Redraws every row currently in the visible window -- used when the
    // window itself shifts, since every visible row's content changes.
    void drawMenuRows(TFT_eSPI &tft) {
        for (int slot = 0; slot < MENU_VISIBLE_ROWS; slot++) {
            int row = menuScrollOffset + slot;
            if (row >= MENU_COUNT) break;
            drawMenuRow(tft, row, slot);
        }
    }

    // Redraws a single row in place, if it's currently within the visible
    // window (a no-op otherwise -- nothing on screen to touch).
    void redrawMenuRowAt(TFT_eSPI &tft, int row) {
        int slot = row - menuScrollOffset;
        if (slot < 0 || slot >= MENU_VISIBLE_ROWS) return;
        drawMenuRow(tft, row, slot);
    }

    // Small chevrons hinting the menu continues past the visible window,
    // same convention as AccountList::drawScrollHints.
    void drawMenuScrollHints(TFT_eSPI &tft) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(menuScrollOffset > 0 ? TOKEN_BLUE : TFT_BLACK, TFT_BLACK);
        tft.drawString("^", tft.width() - 6, MENU_TOP - 16, 2);

        bool moreBelow = menuScrollOffset + MENU_VISIBLE_ROWS < MENU_COUNT;
        tft.setTextColor(moreBelow ? TOKEN_BLUE : TFT_BLACK, TFT_BLACK);
        tft.drawString("v", tft.width() - 6, MENU_TOP + MENU_VISIBLE_ROWS * MENU_ROW_H + 2, 2);
    }

    void drawMenu(TFT_eSPI &tft) {
        clampMenuScrollOffset();
        drawHeader(tft, "Settings");
        drawMenuRows(tft);
        drawMenuScrollHints(tft);
        AccountList::drawIdleFooter(tft);
    }

    // bg must match whatever's actually behind the icon (the button fill,
    // which differs selected vs. not) -- drawArc's anti-aliased edges
    // blend toward it, so a mismatched bg shows up as a visible halo
    // around the arcs.
    void drawWifiIcon(TFT_eSPI &tft, int cx, int cy, uint16_t color, uint16_t bg) {
        // TFT_eSPI's drawArc has 0 degrees at 6 o'clock, sweeping clockwise
        // -- so straight up is 180, and 45 degrees either side of that
        // (135/225) gives a narrow upward fan instead of a full dome
        // reaching all the way down to the horizontal (90/270).
        //
        // The icon's visual mass is the arcs above the dot, so its true
        // center sits well above the dot itself -- shift the dot down from
        // the passed-in cy to land the icon's visual center on cy, the
        // same cy-is-center convention drawBluetoothIcon uses, so the two
        // icons line up when drawn at the same height.
        int dotY = cy + 14;
        tft.fillCircle(cx, dotY, 3, color);
        tft.drawArc(cx, dotY, 12, 9, 135, 225, color, bg, true);
        tft.drawArc(cx, dotY, 21, 18, 135, 225, color, bg, true);
        tft.drawArc(cx, dotY, 30, 27, 135, 225, color, bg, true);
    }

    // The Bluetooth rune-bind glyph: a vertical spine, with two vertices
    // on the right (at the quarter heights) that each fan out both to the
    // spine's own top/bottom and further out to tips on the left, past
    // the spine itself -- those extending tips are what actually make it
    // read as the Bluetooth logo rather than a plain bowtie converging on
    // the spine's center.
    void drawBluetoothIcon(TFT_eSPI &tft, int cx, int cy, uint16_t color, uint16_t bg) {
        constexpr int ICON_W = 36; // spine to each side's vertices
        constexpr int ICON_H = 60;
        constexpr float LINE_W = 4.0f;

        int right = cx + ICON_W / 2;
        int left = cx - ICON_W / 2;
        int top = cy - ICON_H / 2;
        int bottom = cy + ICON_H / 2;
        int upperQ = cy - ICON_H / 4;
        int lowerQ = cy + ICON_H / 4;

        // Pass bg explicitly rather than relying on drawWideLine's default
        // (which reads the pixel back over SPI to blend against) -- that
        // read-back isn't reliable on this hardware, and left a grayish
        // fringe around the icon regardless of the button's actual
        // background.
        tft.drawWideLine(cx, top, cx, bottom, LINE_W, color, bg);          // spine
        tft.drawWideLine(right, lowerQ, left, upperQ, LINE_W, color, bg);  // crossing diagonal
        tft.drawWideLine(right, upperQ, left, lowerQ, LINE_W, color, bg);  // crossing diagonal
        tft.drawWideLine(right, lowerQ, cx, bottom, LINE_W, color, bg);    // right-lower to spine bottom
        tft.drawWideLine(right, upperQ, cx, top, LINE_W, color, bg);       // right-upper to spine top
    }

    void drawSyncButton(TFT_eSPI &tft, int i) {
        int btnW = (tft.width() - SYNC_BTN_GAP * (SYNC_BTN_COUNT + 1)) / SYNC_BTN_COUNT;
        const char *labels[SYNC_BTN_COUNT] = {"WiFi", "Bluetooth"};

        int x = SYNC_BTN_GAP + i * (btnW + SYNC_BTN_GAP);
        bool sel = i == syncMenuSelected;
        uint16_t accent = sel ? TOKEN_BLUE : TFT_DARKGREY;

        tft.fillRect(x, SYNC_BTN_TOP, btnW, SYNC_BTN_H, TFT_BLACK);
        if (sel) {
            tft.fillRoundRect(x, SYNC_BTN_TOP, btnW, SYNC_BTN_H, 8, TOKEN_BLUE_DIM);
        }
        tft.drawRoundRect(x, SYNC_BTN_TOP, btnW, SYNC_BTN_H, 8, accent);

        int cx = x + btnW / 2;
        int iconCy = SYNC_BTN_TOP + SYNC_BTN_H / 2 - 14;

        uint16_t iconBg = sel ? TOKEN_BLUE_DIM : TFT_BLACK;
        if (i == 0) {
            drawWifiIcon(tft, cx, iconCy, accent, iconBg);
        } else {
            drawBluetoothIcon(tft, cx, iconCy, accent, iconBg);
        }

        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(sel ? TOKEN_BLUE : TFT_WHITE, sel ? TOKEN_BLUE_DIM : TFT_BLACK);
        tft.drawString(labels[i], cx, SYNC_BTN_TOP + SYNC_BTN_H - 8, 2);
    }

    void drawSyncTime(TFT_eSPI &tft) {
        drawHeader(tft, "Sync Time");
        for (int i = 0; i < SYNC_BTN_COUNT; i++) drawSyncButton(tft, i);
        AccountList::drawIdleFooter(tft);
    }

    // Just the status text -- the part that changes as BleTimeSync's state
    // machine advances. Cleared to the band between the header rule and
    // the footer band (see FOOTER_BAND_H in account_list.cpp) so repeated
    // polling redraws don't touch either of those.
    void drawBleSyncBody(TFT_eSPI &tft) {
        const char *line1 = "";
        const char *line2 = "";
        uint16_t color2 = TFT_DARKGREY;
        char timeStr[32];

        switch (BleTimeSync::state()) {
            case BleTimeSync::State::Idle:
            case BleTimeSync::State::Advertising:
                line1 = "Advertising as \"Token\"";
                line2 = "Pair from your phone's Bluetooth settings";
                break;
            case BleTimeSync::State::Bonding:
                line1 = "Connected";
                line2 = "Confirm pairing on your phone...";
                break;
            case BleTimeSync::State::Reading:
                line1 = "Paired";
                line2 = "Reading time...";
                break;
            case BleTimeSync::State::Success: {
                time_t now = TimeSync::now();
                struct tm tmVal;
                gmtime_r(&now, &tmVal);
                snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d:%02d UTC", tmVal.tm_hour, tmVal.tm_min,
                         tmVal.tm_sec);
                line1 = "Success!";
                line2 = timeStr;
                color2 = TOKEN_BLUE;
                break;
            }
            case BleTimeSync::State::Failed:
                line1 = "Sync failed";
                line2 = "back: retry / return";
                color2 = TFT_RED;
                break;
        }

        tft.fillRect(0, 27, tft.width(), tft.height() - 27 - 14, TFT_BLACK);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(line1, tft.width() / 2, tft.height() / 2 - 12, 2);

        tft.setTextColor(color2, TFT_BLACK);
        tft.drawString(line2, tft.width() / 2, tft.height() / 2 + 14, 1);

        // Which offset actually produced that time -- lets a wrong reading
        // be caught here instead of only showing up later as a wrong TOTP
        // code, and tells you which fix applies (nothing to do, vs. go fix
        // Settings > Time Zone).
        if (BleTimeSync::state() == BleTimeSync::State::Success) {
            const char *zoneLabel = BleTimeSync::lastOffsetSource() == BleTimeSync::OffsetSource::Detected
                                         ? "zone: from phone"
                                         : "zone: Settings > Time Zone";
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawString(zoneLabel, tft.width() / 2, tft.height() / 2 + 34, 1);
        }
    }

    void drawBluetoothSync(TFT_eSPI &tft) {
        drawHeader(tft, "Bluetooth Sync");
        drawBleSyncBody(tft);
        lastDrawnBleState = BleTimeSync::state();
        AccountList::drawIdleFooter(tft);
    }

    // Just the status text -- the part that changes as AccountLink's
    // state machine advances. Same band as drawBleSyncBody, see there for
    // why it's cleared rather than redrawn wholesale.
    //
    // On Success this previews name + issuer, the same two fields the web
    // setup tool's Review card shows before it ever sends the account --
    // so confirmation here isn't just "an account arrived", it's "this is
    // the account that arrived".
    void drawAddAccountBody(TFT_eSPI &tft) {
        int midY = tft.height() / 2;
        tft.fillRect(0, 27, tft.width(), tft.height() - 27 - 14, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);

        switch (AccountLink::state()) {
            case AccountLink::State::Idle:
            case AccountLink::State::Listening:
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.drawString("Waiting for account...", tft.width() / 2, midY - 10, 2);
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.drawString("from the Token Setup web app", tft.width() / 2, midY + 14, 1);
                break;

            case AccountLink::State::Success: {
                tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
                tft.drawString("Added!", tft.width() / 2, midY - 28, 2);

                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.drawString(AccountLink::lastMessage(), tft.width() / 2, midY - 4, 1);

                const String &issuer = AccountLink::lastIssuer();
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.drawString(issuer.isEmpty() ? "(no issuer)" : issuer, tft.width() / 2, midY + 12, 1);

                tft.drawString("press to add another", tft.width() / 2, midY + 32, 1);
                break;
            }

            case AccountLink::State::Failed:
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.drawString("Failed", tft.width() / 2, midY - 20, 2);
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.drawString(AccountLink::lastMessage(), tft.width() / 2, midY + 2, 1);
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.drawString("press to add another", tft.width() / 2, midY + 22, 1);
                break;
        }
    }

    void drawAddAccount(TFT_eSPI &tft) {
        drawHeader(tft, "Add Account");
        drawAddAccountBody(tft);
        lastDrawnLinkState = AccountLink::state();
        AccountList::drawIdleFooter(tft);
    }

    // Just the ON/OFF readout -- the part that changes on press().
    void drawTypingValue(TFT_eSPI &tft) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(enterAfterCode ? TOKEN_BLUE_LIGHT : TFT_DARKGREY, TFT_BLACK);
        tft.drawString(enterAfterCode ? "ON" : "OFF", tft.width() - 16, 38, 2);
    }

    void drawTyping(TFT_eSPI &tft) {
        drawHeader(tft, "Typing");

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Press Enter after code", 16, 38, 2);

        drawTypingValue(tft);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("press to toggle", tft.width() / 2, tft.height() - 32, 1);

        AccountList::drawIdleFooter(tft);
    }

    // Just the pct readout + bar -- the part that changes as the encoder
    // adjusts brightness. The pct text is cleared to a fixed-size band
    // first since its width varies ("5%" vs "100%"), which drawString's
    // own background-fill wouldn't fully cover on a shrink.
    void drawBrightnessValue(TFT_eSPI &tft) {
        uint8_t level = Backlight::getLevel();

        tft.fillRect(tft.width() - 60, 36, 44, 18, TFT_BLACK);
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
    }

    void drawSystem(TFT_eSPI &tft) {
        drawHeader(tft, "System");

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Brightness", 16, 38, 2);

        drawBrightnessValue(tft);

        drawInfoRow(tft, 100, "Free heap", String(ESP.getFreeHeap() / 1024) + " KB");
        drawInfoRow(tft, 116, "Uptime", String(millis() / 1000) + " s");

        AccountList::drawIdleFooter(tft);
    }

    // The offset readout + local-time preview -- the part that changes as
    // the encoder adjusts the offset. The offset string is a fixed 6
    // characters ("+01:00"), so drawString's own background fill is
    // enough; no explicit clear needed the way the brightness pct does.
    void drawTimeZoneValue(TFT_eSPI &tft) {
        int offset = TimeSync::utcOffsetMinutes();
        char offsetStr[8];
        snprintf(offsetStr, sizeof(offsetStr), "%c%02d:%02d", offset < 0 ? '-' : '+',
                 abs(offset) / 60, abs(offset) % 60);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TOKEN_BLUE_LIGHT, TFT_BLACK);
        tft.drawString(offsetStr, tft.width() - 16, 38, 2);

        char localStr[6] = "--:--";
        if (TimeSync::isSynced()) {
            time_t t = TimeSync::localNow();
            struct tm tmVal;
            gmtime_r(&t, &tmVal);
            snprintf(localStr, sizeof(localStr), "%02d:%02d", tmVal.tm_hour, tmVal.tm_min);
        }
        drawInfoRow(tft, 74, "Local time now", localStr);
    }

    void drawTimeZone(TFT_eSPI &tft) {
        drawHeader(tft, "Time Zone");

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("UTC offset", 16, 38, 2);

        drawTimeZoneValue(tft);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("scroll to adjust, 30 min steps", tft.width() / 2, tft.height() - 32, 1);

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

    void drawWipeConfirm(TFT_eSPI &tft) {
        drawHeader(tft, "Wipe Device");

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("This deletes all accounts", tft.width() / 2, tft.height() / 2 - 24, 2);
        tft.drawString("and resets your PIN.", tft.width() / 2, tft.height() / 2 - 4, 2);

        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("This cannot be undone.", tft.width() / 2, tft.height() / 2 + 20, 1);

        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("press: wipe   back: cancel", tft.width() / 2, tft.height() / 2 + 36, 1);

        AccountList::drawIdleFooter(tft);
    }

    void drawCurrentPage(TFT_eSPI &tft) {
        switch (page) {
            case Page::Menu: drawMenu(tft); break;
            case Page::SyncTime: drawSyncTime(tft); break;
            case Page::BluetoothSync: drawBluetoothSync(tft); break;
            case Page::AddAccount: drawAddAccount(tft); break;
            case Page::ChangePin: break; // PinEntry owns its own drawing
            case Page::Typing: drawTyping(tft); break;
            case Page::System: drawSystem(tft); break;
            case Page::TimeZone: drawTimeZone(tft); break;
            case Page::WipeConfirm: drawWipeConfirm(tft); break;
            case Page::About: drawAbout(tft); break;
        }
    }
}

void Settings::enter(TFT_eSPI &tft) {
    page = Page::Menu;
    menuSelected = 0;
    menuScrollOffset = 0;
    drawCurrentPage(tft);
}

void Settings::redraw(TFT_eSPI &tft) {
    drawCurrentPage(tft);
}

void Settings::scroll(TFT_eSPI &tft, int delta) {
    switch (page) {
        case Page::Menu: {
            int oldSelected = menuSelected;
            menuSelected = ((menuSelected + delta) % MENU_COUNT + MENU_COUNT) % MENU_COUNT;

            int oldScrollOffset = menuScrollOffset;
            clampMenuScrollOffset();
            if (menuScrollOffset != oldScrollOffset) {
                drawMenuRows(tft);
                drawMenuScrollHints(tft);
                break;
            }

            redrawMenuRowAt(tft, oldSelected);
            redrawMenuRowAt(tft, menuSelected);
            break;
        }

        case Page::SyncTime:
            syncMenuSelected = ((syncMenuSelected + delta) % SYNC_BTN_COUNT + SYNC_BTN_COUNT) % SYNC_BTN_COUNT;
            // Only two buttons total -- redrawing both is cheap and avoids
            // tracking which one changed.
            drawSyncButton(tft, 0);
            drawSyncButton(tft, 1);
            break;

        case Page::System: {
            int level = (int)Backlight::getLevel() + delta * BRIGHTNESS_STEP;
            Backlight::setLevel((uint8_t)constrain(level, 0, 100));
            drawBrightnessValue(tft);
            break;
        }

        case Page::TimeZone: {
            int offset = TimeSync::utcOffsetMinutes() + delta * TIMEZONE_STEP_MINUTES;
            TimeSync::setUtcOffsetMinutes(constrain(offset, TIMEZONE_MIN_MINUTES, TIMEZONE_MAX_MINUTES));
            drawTimeZoneValue(tft);
            break;
        }

        case Page::ChangePin:
            PinEntry::scroll(tft, delta);
            break;

        case Page::BluetoothSync:
        case Page::AddAccount:
        case Page::Typing:
        case Page::WipeConfirm:
        case Page::About:
            break; // read-only pages / toggled by press() rather than scroll()
    }
}

Settings::Action Settings::press(TFT_eSPI &tft) {
    if (page == Page::Menu) {
        switch (menuSelected) {
            case 0: page = Page::SyncTime; break;
            case 1: page = Page::AddAccount; break;
            case 2: page = Page::ChangePin; break;
            case 3: page = Page::Typing; break;
            case 4: page = Page::System; break;
            case 5: page = Page::TimeZone; break;
            case 6: page = Page::WipeConfirm; break;
            case 7: page = Page::About; break;
        }
        if (page == Page::AddAccount) AccountLink::begin();
        if (page == Page::ChangePin) {
            changePinVerified = false;
            PinEntry::enterVerify(tft, "Current PIN");
        } else {
            drawCurrentPage(tft);
        }
        return Action::None;
    }

    if (page == Page::SyncTime) {
        if (syncMenuSelected == 0) {
            return Action::OpenWifiList;
        }
        page = Page::BluetoothSync;
        // BLE and WiFi share the 2.4GHz radio -- don't leave a background
        // auto-connect running underneath a pairing attempt.
        WifiAuto::cancel();
        BleTimeSync::begin();
        drawCurrentPage(tft);
    }

    if (page == Page::Typing) {
        enterAfterCode = !enterAfterCode;
        drawTypingValue(tft);
    }

    if (page == Page::AddAccount && AccountLink::state() != AccountLink::State::Listening) {
        // Not listening means the last attempt already resolved
        // (Success/Failed) -- start listening again for another account
        // rather than making the user back out and back in each time.
        AccountLink::begin();
        drawCurrentPage(tft);
    }

    if (page == Page::ChangePin) {
        PinEntry::Action action = PinEntry::press(tft);
        if (!changePinVerified && action == PinEntry::Action::Unlocked) {
            // The current PIN checked out -- move on to picking a new one.
            changePinVerified = true;
            PinEntry::enterSetNew(tft);
        } else if (changePinVerified && action == PinEntry::Action::PinSet) {
            AccountStore::reencrypt(PinEntry::sessionKey());
            page = Page::Menu;
            drawCurrentPage(tft);
        }
    }

    if (page == Page::WipeConfirm) {
        AccountStore::wipeAll();
        PinStore::wipe();
        ESP.restart();
    }

    return Action::None;
}

bool Settings::back(TFT_eSPI &tft) {
    switch (page) {
        case Page::Menu:
            return false;
        case Page::BluetoothSync:
            BleTimeSync::stop();
            page = Page::SyncTime;
            break;
        case Page::ChangePin:
            // PinEntry::back() steps back a digit box on its own and
            // redraws; only pop out of the whole flow once it reports
            // nothing left to back into.
            if (PinEntry::back(tft)) return true;
            changePinVerified = false;
            page = Page::Menu;
            break;
        default:
            page = Page::Menu;
            break;
    }
    drawCurrentPage(tft);
    return true;
}

void Settings::poll(TFT_eSPI &tft) {
    if (page == Page::AddAccount) {
        AccountLink::poll();
        if (AccountLink::state() != lastDrawnLinkState) {
            drawAddAccountBody(tft);
            lastDrawnLinkState = AccountLink::state();
        }
        return;
    }

    if (page != Page::BluetoothSync) return;

    BleTimeSync::poll();
    if (BleTimeSync::state() != lastDrawnBleState) {
        drawBleSyncBody(tft);
        lastDrawnBleState = BleTimeSync::state();
    }
}

bool Settings::typeEnterAfterCode() {
    return enterAfterCode;
}
