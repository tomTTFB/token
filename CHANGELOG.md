# Changelog

Notable changes to Token's firmware. Dates are when a change landed, not
when it shipped in any particular build.

## 2026-07-31

### Fixed
- Screens no longer clear-and-repaint the whole display on every input.
  Moving the account list selection, adjusting a settings value, and
  moving the cursor on the keyboard screen now only redraw the rows/cells
  that actually changed, which was causing a visible flicker on every
  encoder tick.
- NTP sync could report success against a stale RTC-backed clock left
  over from a previous attempt (the ESP32's system clock survives deep
  sleep, since that's what lets the side button wake the device) without
  ever confirming a fresh reply had actually arrived. Surfaced as a wildly
  wrong synced time (e.g. a date years in the future).
- The keyboard screen's header divider line overlapped the typed
  password text; widened the header layout and per-key padding.
- Power-off countdown on the side button now only turns the footer blue
  and starts counting down for the last 3 seconds of the hold, instead of
  from the moment the button is touched.
- Rotary encoder direction was inverted relative to its physical rotation.

### Added
- Rotary encoder scrolling through the account list, with a trailing
  Settings row.
- Settings screen: Sync Time (WiFi, plus a Bluetooth placeholder), System
  (backlight brightness over PWM, free heap, uptime), Time Zone (manual
  UTC offset with a live local-time preview), About (firmware build info,
  chip model, flash size).
- WiFi network scanning and connecting, with a full-screen on-device
  keyboard (letters/digits/symbols, encoder-navigated) for password
  entry, followed by an NTP time sync.
- Battery percentage (estimated from the BQ25896 charger's voltage
  reading -- there's no coulomb-counting fuel gauge wired up) and the
  synced clock, shown on the account list header.

### Changed
- The side button's short-press behavior is now context-dependent: back
  on most screens, backspace on the keyboard screen.
