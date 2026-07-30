#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>

// Board pin map for the LilyGO T-Embed CC1101.
// Source: Xinyuan-LilyGO/T-Embed-CC1101, examples/utilities.h
#define BOARD_PWR_EN    15   // Peripheral power rail enable, must be set before using the display
#define BOARD_SD_CS     13   // TF card and CC1101 share the display's SPI bus
#define BOARD_LORA_CS   12
#define ENCODER_KEY     0    // Rotary encoder push button

#define WS2812_DATA_PIN 14
#define WS2812_NUM_LEDS 8

TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel pixels(WS2812_NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);

    // The display, SD slot and CC1101 module share one SPI bus. Enable the
    // peripheral power rail and deselect the other devices before the
    // display claims the bus.
    pinMode(BOARD_PWR_EN, OUTPUT);
    digitalWrite(BOARD_PWR_EN, HIGH);
    delay(10);

    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);

    pinMode(ENCODER_KEY, INPUT_PULLUP);

    tft.begin();
    tft.setRotation(3); // landscape, 320x170
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Hello, World!", tft.width() / 2, tft.height() / 2, 4);

    pixels.begin();
    pixels.setBrightness(40);

    Serial.println("Hello, World! T-Embed CC1101 is alive.");
}

void loop() {
    // Heartbeat rainbow on the onboard WS2812 strip so it's obvious the
    // board is alive even without watching the serial monitor.
    static uint16_t hue = 0;
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        pixels.setPixelColor(i, pixels.ColorHSV((hue + i * 8192) % 65536));
    }
    pixels.show();
    hue += 512;

    if (digitalRead(ENCODER_KEY) == LOW) {
        Serial.println("Encoder button pressed");
    }

    delay(20);
}
