#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>

#include "board_pins.h"
#include "power.h"
#include "ui/account_list.h"
#include "ui/boot_screen.h"

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
    pinMode(BOARD_USER_KEY, INPUT_PULLUP);

    tft.begin();
    tft.setRotation(3); // landscape, 320x170

    pixels.begin();
    pixels.setBrightness(40);
    pixels.clear();
    pixels.show();

    BootScreen::play(tft);
    AccountList::draw(tft);

    Serial.println("Token is alive.");
}

void loop() {
    bool sidePressed = digitalRead(BOARD_USER_KEY) == LOW;
    Power::pollShutdownButton(sidePressed, tft, pixels);
    delay(20);
}
