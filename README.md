# Token

A hardware 2FA authenticator. Custom firmware for the [LilyGO T-Embed CC1101](https://www.lilygo.cc)
that generates and displays TOTP codes on-device, with no phone, app, or
cloud involved. Selecting an account types its code over USB as a keyboard.

[Setup tool](https://token.tomttfb.com) — flashes firmware and adds accounts
from the browser over USB.
[Demo](http://192.168.0.125:8092) — the code-generation logic running in a
browser. LAN-only for now.

## Features

- PIN-locked. The PIN is never stored; it derives the key that decrypts the
  account store, so a wrong guess can't leak anything. Five failed attempts
  trigger a wipe warning.
- Up to 16 accounts, each showing a live code with a countdown, refreshed
  every second.
- Clock sync over WiFi (NTP) or Bluetooth, reading the phone's time and
  time zone directly.
- Settings menu for PIN changes, brightness, time zone, and wiping the
  device. Wiping requires a full turn of the dial plus the PIN.
- Accounts, PIN, and known WiFi networks persist across reboots. The clock
  does not, and re-syncs on wake.

## Building one

Requires the board itself, then either:

- **Setup tool.** [token.tomttfb.com](https://token.tomttfb.com) flashes
  over USB from the browser with no drivers, and adds accounts by QR scan
  or manual entry. Reflashing preserves existing accounts and the PIN
  unless erase is selected. See [setup-tool/README.md](setup-tool/README.md).
- **PlatformIO:**
  ```sh
  pio run -t upload       # build and flash over USB-C
  pio device monitor      # serial monitor, 115200 baud
  ```

Pin mappings are from LilyGO's
[T-Embed-CC1101](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101) repo.

## License

MIT. See [LICENSE](LICENSE).

