# Token

Firmware for the [LilyGO T-Embed CC1101](https://www.lilygo.cc) (ESP32-S3 +
1.9" ST7789 display + CC1101 sub-GHz radio), built toward a hardware TOTP
(2FA) authenticator: codes generated and shown entirely offline on the
device, no phone or cloud involved.

Currently implemented:

- Boot animation — a Token-blue digital rain effect with an ASCII key logo
  and wordmark layered on top
- An account list screen (static entries, `000000` fake codes) navigable with
  the rotary encoder, windowed to the visible rows with scroll indicators
- A Settings screen reached by scrolling past the last account and pressing
  the encoder: Sync Time (placeholder until WiFi is implemented), System
  (live backlight brightness over PWM, free heap, uptime), and About
  (firmware build time, chip model, flash size)
- Hold-to-shutdown — holding the physical side button for 5 seconds shows a
  countdown, then powers the device down; pressing the same button wakes it
  back up. A short press instead acts as "back" out of a settings screen.

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
