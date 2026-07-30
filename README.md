# T-Embed CC1101 Hello World

Minimal firmware for the [LilyGO T-Embed CC1101](https://www.lilygo.cc) (ESP32-S3 +
1.9" ST7789 display + CC1101 sub-GHz radio). Draws "Hello, World!" on the screen,
pulses the onboard WS2812 LEDs as a heartbeat, and logs over serial.

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
  high before the display will respond.
