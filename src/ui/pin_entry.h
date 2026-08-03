#pragma once

#include <TFT_eSPI.h>

#include "pin_store.h"

// 4-digit PIN entry/set screen. One shared digit-grid widget drives three
// flows -- see plan/ui.md's "PIN Entry" and "Wipe Warning" screens:
//
//  - enterBoot(): the boot-time screen. First-time setup (enter a PIN
//    twice) if no PIN is configured yet; otherwise "Enter PIN" with a fail
//    counter that leads to the wipe-warning screen at the limit.
//  - enterVerify()/enterSetNew(): the same widget, reused by Settings'
//    Change PIN flow -- verify the current PIN, then set a new one.
//
// Each enterX() call fully resets internal state; press()/back()'s
// behavior depends on whichever flow is currently active.
namespace PinEntry {
    enum class Action { None, Unlocked, PinSet, WipeRequested };

    void enterBoot(TFT_eSPI &tft);
    void enterVerify(TFT_eSPI &tft, const char *prompt);
    void enterSetNew(TFT_eSPI &tft);

    // Cycles the active digit box's value by delta (wrapping 0-9), then
    // redraws just that box.
    void scroll(TFT_eSPI &tft, int delta);

    // Confirms the active digit and advances to the next box. On the
    // final box, acts on the completed PIN:
    //  - enterBoot's unlock mode / enterVerify: attempts
    //    PinStore::verify(). Success returns Unlocked (sessionKey() now
    //    valid). Failure redraws the entry -- enterBoot's unlock mode also
    //    tracks the fail counter and switches to the wipe-warning screen
    //    at the limit -- and always returns None otherwise.
    //  - the two-pass setup flows (enterBoot's first-time setup,
    //    enterSetNew): the first pass just stores the entry and starts the
    //    second; the second calls PinStore::setPin() and returns PinSet
    //    once the two entries match (a mismatch silently restarts the
    //    two-pass entry).
    //  - the wipe-warning screen: returns WipeRequested. The caller
    //    actually performs the wipe (AccountStore::wipeAll(),
    //    PinStore::wipe()) -- PinEntry only owns the confirmation UI.
    Action press(TFT_eSPI &tft);

    // Side-button gesture: steps back one digit box, clearing it. On the
    // wipe-warning screen, cancels back to one final unlock attempt
    // (PinStore::grantFinalAttempt()) instead. Returns false when there's
    // nowhere left to back into (box 0 of the active pass) -- callers
    // reached via enterVerify/enterSetNew treat that as "abandon this
    // flow"; enterBoot has nowhere else to go, so its caller ignores the
    // return value.
    bool back(TFT_eSPI &tft);

    // The 32-byte AES key derived from the PIN that just unlocked or was
    // just set. Only valid immediately after press() returns
    // Unlocked/PinSet, until the next enterX() call.
    const uint8_t *sessionKey();
}
