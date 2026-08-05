#pragma once

#include <Arduino.h>

// Adds accounts over the USB serial port. The setup-tool web app
// (setup-tool/serial_link.py) is the intended sender, but a person in a
// serial terminal works just as well: write one line of the form
//
//   ADD|name|issuer|base32secret\n
//
// and Token replies with "OK" or "ERR: <reason>" on the next line. `name`
// and `issuer` may be empty but `secret` must decode as base32 (see
// Totp::isValidSecret) -- digits/period aren't settable this way yet and
// default to AccountStore's RFC 6238 defaults (6 digits, 30s).
namespace AccountLink {
    enum class State { Idle, Listening, Success, Failed };

    // Flushes any stale input sitting in the serial buffer and starts
    // listening for one ADD line. Safe to call again to listen for
    // another account after a Success/Failed result.
    void begin();

    // Must be polled every loop() iteration while listening. No-op once a
    // line has been consumed and the result (Success/Failed) hasn't been
    // cleared by another begin() yet.
    void poll();

    State state();

    // Human-readable result of the last completed attempt: the account
    // name on Success, or the failure reason on Failed. Empty otherwise.
    const String &lastMessage();

    // The added account's issuer on Success, mirroring the same field the
    // web setup tool previews before it sends. May be empty (the field is
    // optional). Meaningless outside Success.
    const String &lastIssuer();
}
