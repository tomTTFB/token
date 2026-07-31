#include "wifi_store.h"

#include <Preferences.h>

namespace {
    // NVS namespace and key names. Keys stay short deliberately -- NVS caps
    // them at 15 characters.
    constexpr const char *NVS_NAMESPACE = "wifinets";
    constexpr const char *KEY_COUNT = "n";

    struct Entry {
        String ssid;
        String password;
    };

    Preferences prefs;
    bool opened = false;

    Entry entries[WifiStore::MAX_ENTRIES];
    int entryCount = 0;

    const String EMPTY_SSID;

    String ssidKey(int i) { return "s" + String(i); }
    String pwKey(int i) { return "p" + String(i); }

    // Rewrites the whole list. It's at most MAX_ENTRIES short strings and
    // only happens on a successful connect, so there's no need for anything
    // smarter than a full overwrite.
    void persist() {
        prefs.putInt(KEY_COUNT, entryCount);
        for (int i = 0; i < entryCount; i++) {
            prefs.putString(ssidKey(i).c_str(), entries[i].ssid);
            prefs.putString(pwKey(i).c_str(), entries[i].password);
        }
    }

    int indexOf(const String &ssid) {
        for (int i = 0; i < entryCount; i++) {
            if (entries[i].ssid == ssid) return i;
        }
        return -1;
    }

    void removeAt(int index) {
        for (int i = index; i + 1 < entryCount; i++) entries[i] = entries[i + 1];
        entryCount--;
    }
}

void WifiStore::begin() {
    opened = prefs.begin(NVS_NAMESPACE, false);
    if (!opened) {
        Serial.println("WifiStore: failed to open NVS namespace");
        return;
    }

    entryCount = constrain(prefs.getInt(KEY_COUNT, 0), 0, MAX_ENTRIES);
    for (int i = 0; i < entryCount; i++) {
        entries[i].ssid = prefs.getString(ssidKey(i).c_str(), "");
        entries[i].password = prefs.getString(pwKey(i).c_str(), "");
    }

    // A saved entry with no SSID means a partial/corrupt write -- drop it
    // rather than letting an unmatchable entry sit in the list forever.
    for (int i = entryCount - 1; i >= 0; i--) {
        if (entries[i].ssid.isEmpty()) removeAt(i);
    }

    Serial.printf("WifiStore: %d saved network(s)\n", entryCount);
}

bool WifiStore::remember(const String &ssid, const String &password) {
    if (!opened || ssid.isEmpty()) return false;

    int existing = indexOf(ssid);
    if (existing == 0 && entries[0].password == password) {
        return true; // already the most recent entry, nothing changed
    }

    if (existing >= 0) {
        removeAt(existing);
    } else if (entryCount == MAX_ENTRIES) {
        entryCount--; // evict the least recently connected
    }

    for (int i = entryCount; i > 0; i--) entries[i] = entries[i - 1];
    entries[0].ssid = ssid;
    entries[0].password = password;
    entryCount++;

    persist();
    return true;
}

bool WifiStore::isKnown(const String &ssid) {
    return indexOf(ssid) >= 0;
}

bool WifiStore::passwordFor(const String &ssid, String &out) {
    int i = indexOf(ssid);
    if (i < 0) return false;
    out = entries[i].password;
    return true;
}

void WifiStore::forget(const String &ssid) {
    if (!opened) return;
    int i = indexOf(ssid);
    if (i < 0) return;

    removeAt(i);
    // The tail keys from the old list would otherwise linger in NVS and be
    // read back on the next boot if the list later grows again.
    prefs.remove(ssidKey(entryCount).c_str());
    prefs.remove(pwKey(entryCount).c_str());
    persist();
}

int WifiStore::count() {
    return entryCount;
}

const String &WifiStore::ssidAt(int index) {
    if (index < 0 || index >= entryCount) return EMPTY_SSID;
    return entries[index].ssid;
}
