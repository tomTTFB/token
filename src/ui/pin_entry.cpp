#include "pin_entry.h"

#include <cstring>

#include "account_list.h"
#include "colors.h"

namespace {
    enum class Mode { BootSetupEnter, BootSetupConfirm, BootUnlock, BootWipeWarning, VerifyOnly, SetNewEnter,
                       SetNewConfirm };

    constexpr int BOX_W = 36;
    constexpr int BOX_H = 44;
    constexpr int BOX_GAP = 12;
    constexpr int BOX_Y = 40;
    constexpr int TITLE_Y = 10;
    constexpr int HINT_Y = 98;
    constexpr int STATUS_Y = 116;
    constexpr int ATTEMPTS_Y = 134;

    Mode mode = Mode::BootUnlock;
    int cursor = 0;
    int digitVal[PinStore::PIN_LEN];
    char firstPin[PinStore::PIN_LEN + 1];
    uint8_t sessionKeyBuf[PinStore::KEY_LEN];
    String verifyPrompt;
    String statusMsg;

    void resetDigits() {
        cursor = 0;
        for (int i = 0; i < PinStore::PIN_LEN; i++) digitVal[i] = 0;
    }

    void finalizePin(char out[PinStore::PIN_LEN + 1]) {
        for (int i = 0; i < PinStore::PIN_LEN; i++) out[i] = (char)('0' + digitVal[i]);
        out[PinStore::PIN_LEN] = '\0';
    }

    const char *titleFor() {
        switch (mode) {
            case Mode::BootSetupEnter: return "Set PIN";
            case Mode::BootSetupConfirm: return "Confirm PIN";
            case Mode::BootUnlock: return "Enter PIN";
            case Mode::VerifyOnly: return verifyPrompt.c_str();
            case Mode::SetNewEnter: return "New PIN";
            case Mode::SetNewConfirm: return "Confirm New PIN";
            case Mode::BootWipeWarning: return "";
        }
        return "";
    }

    int boxesStartX(TFT_eSPI &tft) {
        int totalW = PinStore::PIN_LEN * BOX_W + (PinStore::PIN_LEN - 1) * BOX_GAP;
        return (tft.width() - totalW) / 2;
    }

    // Digits up to and including the active box are shown -- the active
    // one reflects whatever scroll() last landed on, which reads as "live"
    // rather than masked. Digits are shown as plain numbers (not masked)
    // since this screen is only visible to whoever is holding the device,
    // per plan/ui.md.
    void drawBox(TFT_eSPI &tft, int i) {
        int x = boxesStartX(tft) + i * (BOX_W + BOX_GAP);
        bool active = i == cursor;
        uint16_t border = active ? TOKEN_BLUE : TFT_DARKGREY;

        tft.fillRect(x, BOX_Y, BOX_W, BOX_H, TFT_BLACK);
        tft.drawRoundRect(x, BOX_Y, BOX_W, BOX_H, 4, border);

        if (i <= cursor) {
            char label[2] = {(char)('0' + digitVal[i]), '\0'};
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(label, x + BOX_W / 2, BOX_Y + BOX_H / 2, 4);
        }
    }

    void drawBoxes(TFT_eSPI &tft) {
        for (int i = 0; i < PinStore::PIN_LEN; i++) drawBox(tft, i);
    }

    // Amber at 3 or fewer remaining, red at 1 -- per plan/ui.md.
    void drawAttempts(TFT_eSPI &tft) {
        if (mode != Mode::BootUnlock) return;

        int remaining = PinStore::MAX_ATTEMPTS - PinStore::failCount();
        uint16_t color = remaining <= 1 ? TFT_RED : (remaining <= 3 ? TFT_ORANGE : TFT_DARKGREY);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, TFT_BLACK);
        tft.drawString("Attempts remaining: " + String(remaining), tft.width() / 2, ATTEMPTS_Y, 1);
    }

    void drawStatus(TFT_eSPI &tft) {
        if (statusMsg.isEmpty()) return;
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString(statusMsg, tft.width() / 2, STATUS_Y, 1);
    }

    void drawWipeWarning(TFT_eSPI &tft) {
        tft.fillScreen(TFT_BLACK);

        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("WARNING", tft.width() / 2, 10, 4);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Too many failed attempts.", tft.width() / 2, 54, 1);
        tft.drawString("All account data will be", tft.width() / 2, 72, 1);
        tft.drawString("permanently deleted.", tft.width() / 2, 86, 1);

        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString("press: wipe   back: cancel", tft.width() / 2, 112, 1);

        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("This cannot be undone.", tft.width() / 2, 130, 1);

        AccountList::drawIdleFooter(tft);
    }

    void drawScreen(TFT_eSPI &tft) {
        if (mode == Mode::BootWipeWarning) {
            drawWipeWarning(tft);
            return;
        }

        tft.fillScreen(TFT_BLACK);

        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TOKEN_BLUE, TFT_BLACK);
        tft.drawString(titleFor(), tft.width() / 2, TITLE_Y, 2);

        drawBoxes(tft);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("encoder: change   press: confirm", tft.width() / 2, HINT_Y, 1);

        drawStatus(tft);
        drawAttempts(tft);

        AccountList::drawIdleFooter(tft);
    }

    void beginPass(TFT_eSPI &tft, Mode m, const String &msg = String()) {
        mode = m;
        resetDigits();
        statusMsg = msg;
        drawScreen(tft);
    }
}

void PinEntry::enterBoot(TFT_eSPI &tft) {
    beginPass(tft, PinStore::isSet() ? Mode::BootUnlock : Mode::BootSetupEnter);
}

void PinEntry::enterVerify(TFT_eSPI &tft, const char *prompt) {
    verifyPrompt = prompt;
    beginPass(tft, Mode::VerifyOnly);
}

void PinEntry::enterSetNew(TFT_eSPI &tft) {
    beginPass(tft, Mode::SetNewEnter);
}

void PinEntry::scroll(TFT_eSPI &tft, int delta) {
    if (mode == Mode::BootWipeWarning) return;
    digitVal[cursor] = ((digitVal[cursor] + delta) % 10 + 10) % 10;
    drawBox(tft, cursor);
}

PinEntry::Action PinEntry::press(TFT_eSPI &tft) {
    if (mode == Mode::BootWipeWarning) return Action::WipeRequested;

    // Confirm the active digit and advance, unless this was the last box.
    if (cursor < PinStore::PIN_LEN - 1) {
        cursor++;
        drawBox(tft, cursor - 1);
        drawBox(tft, cursor);
        return Action::None;
    }

    char pin[PinStore::PIN_LEN + 1];
    finalizePin(pin);

    switch (mode) {
        case Mode::BootSetupEnter:
            memcpy(firstPin, pin, sizeof(firstPin));
            beginPass(tft, Mode::BootSetupConfirm);
            return Action::None;

        case Mode::SetNewEnter:
            memcpy(firstPin, pin, sizeof(firstPin));
            beginPass(tft, Mode::SetNewConfirm);
            return Action::None;

        case Mode::BootSetupConfirm:
        case Mode::SetNewConfirm:
            if (strcmp(pin, firstPin) != 0) {
                Mode retry = mode == Mode::BootSetupConfirm ? Mode::BootSetupEnter : Mode::SetNewEnter;
                beginPass(tft, retry, "PINs didn't match");
                return Action::None;
            }
            PinStore::setPin(pin, sessionKeyBuf);
            return Action::PinSet;

        case Mode::BootUnlock:
            if (PinStore::verify(pin, sessionKeyBuf)) return Action::Unlocked;

            if (PinStore::failCount() >= PinStore::MAX_ATTEMPTS) {
                mode = Mode::BootWipeWarning;
                drawScreen(tft);
            } else {
                beginPass(tft, Mode::BootUnlock, "Wrong PIN");
            }
            return Action::None;

        case Mode::VerifyOnly:
            if (PinStore::verify(pin, sessionKeyBuf)) return Action::Unlocked;
            beginPass(tft, Mode::VerifyOnly, "Wrong PIN");
            return Action::None;

        case Mode::BootWipeWarning:
            return Action::WipeRequested; // unreachable, handled above
    }
    return Action::None;
}

bool PinEntry::back(TFT_eSPI &tft) {
    if (mode == Mode::BootWipeWarning) {
        PinStore::grantFinalAttempt();
        beginPass(tft, Mode::BootUnlock);
        return true;
    }

    if (cursor == 0) return false;

    digitVal[cursor] = 0;
    cursor--;
    drawBox(tft, cursor + 1);
    drawBox(tft, cursor);
    return true;
}

const uint8_t *PinEntry::sessionKey() {
    return sessionKeyBuf;
}
