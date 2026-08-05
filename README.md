# Token

Firmware for the [LilyGO T-Embed CC1101](https://www.lilygo.cc) (ESP32-S3 +
1.9" ST7789 display + CC1101 sub-GHz radio), built toward a hardware TOTP
(2FA) authenticator: codes generated and shown entirely offline on the
device, no phone or cloud involved.

Currently implemented:

- Boot animation — a Token-blue digital rain effect with an ASCII key logo
  and wordmark layered on top
- A PIN lock in front of everything else. First boot asks you to set a
  4-digit PIN (entered twice); after that it's "enter PIN to unlock" every
  time. The PIN is never stored — it's run through PBKDF2 to derive an
  AES-256-GCM key that both unlocks the UI and decrypts the account store,
  so a wrong guess fails closed rather than leaking anything. Five wrong
  attempts trips a wipe-warning screen; backing out of that grants one last
  attempt instead of resetting the counter
- A real account list: live TOTP codes (RFC 6238) regenerating every
  second with a per-row countdown bar, navigable with the rotary encoder
  and windowed to the visible rows. Clicking a row types that code over USB
  as a real keyboard (native USB-HID, no companion software) — handy for
  logging into things the browser can't paste into. Accounts are encrypted
  at rest (AES-256-GCM, keyed off the PIN) in flash, up to 16 of them
- A Settings screen (scroll past the last account, press to enter) with
  eight pages: Sync Time, Add Account, Change PIN, Typing, System, Time
  Zone, Wipe Device, and About
- **Sync Time** offers WiFi and Bluetooth. WiFi scans, connects (a
  full-screen keyboard handles the password), and syncs over NTP — and the
  device now also tries this automatically and non-blockingly on boot
  against any network it's connected to before. Bluetooth pairs with your
  phone's own clock over the standard Current Time Service, no companion
  app needed, and picks up the phone's timezone automatically when it
  offers one (falling back to the manual Time Zone setting otherwise)
- **Add Account** listens on USB serial for an account pushed from the web
  setup tool (or typed by hand over a serial terminal)
- **Change PIN** re-verifies the current PIN, then walks through setting
  and confirming a new one, re-encrypting the stored accounts under it
- **Typing** toggles whether a trailing Enter is sent after a typed code
- **System** shows live backlight brightness (PWM, encoder-adjustable),
  free heap, and uptime
- **Time Zone** sets a manual UTC offset in 30-minute steps, used for the
  header clock and as Bluetooth sync's fallback
- **Wipe Device** is a deliberate three-stage confirmation: a plain-language
  warning, a second "are you sure" that spells out what's still ahead, a
  full turn of the rotary encoder (a gesture a stray button press can't
  fake), and finally your PIN — only then does it erase everything and
  reboot
- **About** shows firmware build time, chip model/revision, CPU frequency,
  and flash size
- Battery percentage is estimated from the BQ25896 charger's voltage reading
  against a typical single-cell LiPo curve; there's no coulomb-counting fuel
  gauge wired up
- Hold-to-shutdown — holding the physical side button for 5 seconds shows a
  countdown, then powers the device down; pressing the same button wakes it
  back up. A short press instead acts as "back" (or backspace, on the
  keyboard screen)

Pin mappings are sourced from LilyGO's official
[Xinyuan-LilyGO/T-Embed-CC1101](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101)
repo (`examples/utilities.h` and the `Setup214_LilyGo_T_Embed_PN532.h` TFT_eSPI setup).

## Build and flash

Requires [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

```sh
pio run                 # build
pio run -t upload       # flash over USB-C
pio device monitor      # serial monitor (115200 baud)
```

Or use the PlatformIO VS Code extension's Build/Upload/Monitor buttons with this
folder open — `platformio.ini` already selects the `T_Embed_CC1101` environment.

### Web setup tool

[`setup-tool/`](setup-tool/) is a local Flask + Bootstrap app that flashes a
`pio run` build to the device straight from the browser (via Web Serial, no
drivers needed) and adds 2FA accounts over USB — scan an `otpauth://` QR
code or enter one by hand. Reflashing leaves existing accounts and your PIN
in place unless you explicitly check "Erase device," and the serial port
picker hides ports without a USB description (Linux's phantom `ttyS*` ports)
behind a "show all" toggle so the real one isn't buried. See
[`setup-tool/README.md`](setup-tool/README.md) for the details, including
what the USB protocol deliberately doesn't support yet (no remote list,
delete, or wipe).

## Demo

Two things running on the home LAN sandbox, for poking at without a device
in hand:

- **TOTP demo** — [http://192.168.0.125:8092](http://192.168.0.125:8092),
  the code-generation algorithm running in a browser so you can see it work
  without flashing hardware.
- **Token setup tool** — [http://192.168.0.125:5002](http://192.168.0.125:5002),
  the same web setup app from above, deployed so it's reachable without
  running it locally.

Both are LAN-only — they won't resolve off the home network.

## Notes

- `boards/T_Embed_CC1101.json` declares the board's 16MB flash / 8MB PSRAM (QIO
  OPI) layout so the build matches the actual hardware.
- The display, TF card slot, and CC1101 radio share one SPI bus; `src/main.cpp`
  deselects the SD and radio chip-selects before initializing the display.
- `BOARD_PWR_EN` (GPIO15) gates the peripheral power rail and must be driven
  high before the display will respond; it's also cut on shutdown.
- `BOARD_USER_KEY` (GPIO6) is the physical side button used for the
  hold-to-shutdown gesture — separate from `ENCODER_KEY` (GPIO0), the rotary
  encoder's push button.
- The rotary encoder's quadrature channels (`ENCODER_INA`/`ENCODER_INB`, GPIO4/5)
  are decoded with the `RotaryEncoder` library; `ENCODER_KEY` is read directly
  and debounced in `main.cpp`.
- Backlight brightness is driven over LEDC PWM on `TFT_BL` (GPIO21), since
  TFT_eSPI only ever drives that pin high on init.
- The BQ25896 charger sits on the same I2C bus as the board's other
  peripherals (`BOARD_I2C_SDA`/`BOARD_I2C_SCL`, GPIO8/18), read via
  `XPowersLib`.
- Accounts, the PIN, and up to 8 known WiFi networks persist across reboots
  in NVS. The synced clock doesn't — every boot needs a fresh WiFi or
  Bluetooth sync, which is most of why the boot-time WiFi auto-reconnect
  exists.
- Saved WiFi passwords are stored in NVS unencrypted (unlike accounts and
  the PIN, which are AES-GCM'd) — they're not treated as part of the same
  threat model, so don't reuse a sensitive password for a network you add
  here.
