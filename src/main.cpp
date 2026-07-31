#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include <RotaryEncoder.h>

#include "backlight.h"
#include "board_pins.h"
#include "power.h"
#include "ui/account_list.h"
#include "ui/boot_screen.h"
#include "ui/settings.h"

namespace {
    enum class Screen { AccountList, Settings };

    constexpr uint32_t ENCODER_KEY_DEBOUNCE_MS = 150;

    Screen currentScreen = Screen::AccountList;
    long encoderPos = 0;
    bool encoderKeyWasPressed = false;
    uint32_t lastEncoderKeyEventMs = 0;

    // Edge-detects a debounced click of the encoder's push button, distinct
    // from BOARD_USER_KEY (the side button used for hold-to-shutdown/back).
    bool pollEncoderKeyClicked() {
        bool pressed = digitalRead(ENCODER_KEY) == LOW;
        bool clicked = false;

        uint32_t now = millis();
        if (pressed && !encoderKeyWasPressed && (now - lastEncoderKeyEventMs) > ENCODER_KEY_DEBOUNCE_MS) {
            clicked = true;
            lastEncoderKeyEventMs = now;
        }

        encoderKeyWasPressed = pressed;
        return clicked;
    }
}

TFT_eSPI tft = TFT_eSPI();
Adafruit_NeoPixel pixels(WS2812_NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
RotaryEncoder encoder(ENCODER_INA, ENCODER_INB, RotaryEncoder::LatchMode::TWO03);

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
    Backlight::init();

    pixels.begin();
    pixels.setBrightness(40);
    pixels.clear();
    pixels.show();

    BootScreen::play(tft);
    AccountList::draw(tft);

    Serial.println("Token is alive.");
}

void loop() {
    encoder.tick();

    long newPos = encoder.getPosition();
    if (newPos != encoderPos) {
        int delta = (int)(newPos - encoderPos);
        encoderPos = newPos;

        if (currentScreen == Screen::AccountList) {
            AccountList::scroll(tft, delta);
        } else {
            Settings::scroll(tft, delta);
        }
    }

    if (pollEncoderKeyClicked()) {
        if (currentScreen == Screen::AccountList) {
            if (AccountList::isSettingsSelected()) {
                currentScreen = Screen::Settings;
                Settings::enter(tft);
            }
        } else {
            Settings::press(tft);
        }
    }

    bool sidePressed = digitalRead(BOARD_USER_KEY) == LOW;
    bool backClicked = Power::pollShutdownButton(sidePressed, tft, pixels);
    if (backClicked && currentScreen == Screen::Settings) {
        if (!Settings::back(tft)) {
            currentScreen = Screen::AccountList;
            AccountList::draw(tft);
        }
    }

    delay(2);
}
