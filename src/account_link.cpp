#include "account_link.h"

#include "account_store.h"
#include "totp.h"

namespace {
    constexpr size_t MAX_LINE = 256;

    AccountLink::State currentState = AccountLink::State::Idle;
    String lineBuf;
    String message;

    // Splits "ADD|name|issuer|secret" on '|' into exactly 4 fields.
    // Returns false if the field count doesn't match (a caller-side typo,
    // not a partial line -- poll() only calls this once a full line has
    // arrived).
    bool splitFields(const String &line, String fields[4]) {
        int start = 0;
        for (int i = 0; i < 4; i++) {
            int sep = line.indexOf('|', start);
            bool last = (i == 3);
            if (last ? (sep != -1) : (sep == -1)) return false;
            fields[i] = last ? line.substring(start) : line.substring(start, sep);
            start = sep + 1;
        }
        return true;
    }

    void handleLine(const String &rawLine) {
        String line = rawLine;
        line.trim();
        if (line.isEmpty()) return;

        String fields[4];
        if (!splitFields(line, fields) || fields[0] != "ADD") {
            currentState = AccountLink::State::Failed;
            message = "malformed line";
            Serial.println("ERR: malformed line");
            return;
        }

        const String &name = fields[1];
        const String &issuer = fields[2];
        const String &secret = fields[3];

        if (name.isEmpty()) {
            currentState = AccountLink::State::Failed;
            message = "name is required";
            Serial.println("ERR: name is required");
            return;
        }

        if (!Totp::isValidSecret(secret)) {
            currentState = AccountLink::State::Failed;
            message = "secret is not valid base32";
            Serial.println("ERR: secret is not valid base32");
            return;
        }

        if (!AccountStore::add(name, issuer, secret)) {
            currentState = AccountLink::State::Failed;
            message = "account storage is full";
            Serial.println("ERR: account storage is full");
            return;
        }

        currentState = AccountLink::State::Success;
        message = name;
        Serial.println("OK");
    }
}

void AccountLink::begin() {
    while (Serial.available()) Serial.read();
    lineBuf = "";
    message = "";
    currentState = State::Listening;
    Serial.println("Token: waiting for ADD|name|issuer|secret");
}

void AccountLink::poll() {
    if (currentState != State::Listening) return;

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            handleLine(lineBuf);
            lineBuf = "";
            return; // handleLine already moved currentState off Listening
        }
        if (c == '\r') continue;
        if (lineBuf.length() < MAX_LINE) lineBuf += c;
    }
}

AccountLink::State AccountLink::state() {
    return currentState;
}

const String &AccountLink::lastMessage() {
    return message;
}
