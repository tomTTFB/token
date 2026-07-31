#pragma once

// Board pin map for the LilyGO T-Embed CC1101.
// Source: Xinyuan-LilyGO/T-Embed-CC1101, examples/utilities.h
#define BOARD_PWR_EN    15   // Peripheral power rail enable, must be set before using the display
#define BOARD_SD_CS     13   // TF card and CC1101 share the display's SPI bus
#define BOARD_LORA_CS   12
#define ENCODER_KEY     0    // Rotary encoder push button
#define ENCODER_INA     4    // Rotary encoder quadrature channel A
#define ENCODER_INB     5    // Rotary encoder quadrature channel B
#define BOARD_USER_KEY  6    // Physical side button

#define TFT_BACKLIGHT_PWM_CHANNEL 0 // LEDC channel driving TFT_BL (GPIO21)

#define WS2812_DATA_PIN 14
#define WS2812_NUM_LEDS 8
