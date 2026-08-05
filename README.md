# Token

A little hardware 2FA authenticator. It's a [LilyGO T-Embed CC1101](https://www.lilygo.cc)
running custom firmware that generates and shows TOTP codes right on its own
screen — no phone, no app, no cloud. Click an account and it types the code
for you, like a tiny USB keyboard.

**[Try the setup tool](https://token.tomttfb.com)** — flash firmware and add
accounts straight from your browser, no drivers or CLI.
**[Try the demo](http://192.168.0.125:8092)** — see the code-generation
logic running in a browser before you build anything. LAN-only for now.

## What it does

- **Locked behind a PIN.** The PIN itself is never stored — it's used to
  derive the key that also decrypts your accounts, so guessing wrong can't
  leak anything. Five bad attempts trips a wipe warning.
- **Real, live codes.** Up to 16 accounts, each with a running countdown,
  refreshing every second. Click one to type it into whatever you're
  logging into.
- **Syncs its own clock**, over WiFi or by reading the time straight off
  your phone over Bluetooth — picks up your timezone automatically too.
- **A proper Settings menu** — change your PIN, adjust brightness, set a
  timezone by hand, or wipe the device. Wiping takes a full turn of the
  dial plus your PIN, so it can't happen by accident.
- Accounts, PIN, and known WiFi networks all survive a reboot. The clock
  doesn't, so it just re-syncs on wake.

## Building one

You'll need the board itself, then either:

- **Flash from the browser** — [the setup tool](https://token.tomttfb.com)
  does it over USB with no drivers or install. It also handles adding
  accounts (scan a QR code, or type a secret in by hand), all straight from
  your browser talking to the device over USB. Reflashing leaves your
  accounts and PIN alone unless you tell it not to. See
  [`setup-tool/README.md`](setup-tool/README.md) for the details.
- **Build it yourself** with [PlatformIO](https://platformio.org/):
  ```sh
  pio run -t upload       # build and flash over USB-C
  pio device monitor      # serial monitor, 115200 baud
  ```

Pin mappings come from LilyGO's own
[Xinyuan-LilyGO/T-Embed-CC1101](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101)
repo.

## If you're poking at the code

A few things that aren't obvious just from reading it:

- The display, TF card slot, and CC1101 radio all share one SPI bus — the
  SD and radio chip-selects get deselected before the display initializes,
  or nothing on the bus behaves.
- `BOARD_PWR_EN` (GPIO15) gates power to the rest of the board and has to
  go high before the display responds. It's cut again on shutdown.
- The rotary encoder's push button (`ENCODER_KEY`, GPIO0) and the physical
  side button (`BOARD_USER_KEY`, GPIO6) are separate inputs — the side
  button is what drives hold-to-shutdown.
- Backlight brightness is PWM on `TFT_BL` (GPIO21) via LEDC, since
  TFT_eSPI only ever drives that pin fully on.
- Battery percentage comes from the BQ25896 charger's voltage reading
  against a typical LiPo curve — there's no real fuel gauge, so treat it
  as an estimate.
- Saved WiFi passwords sit in flash unencrypted, unlike accounts and the
  PIN (which are AES-GCM'd) — don't reuse a password you care about for a
  network you add here.
