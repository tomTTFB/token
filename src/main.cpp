#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include <RotaryEncoder.h>

#include "backlight.h"
#include "battery.h"
#include "board_pins.h"
#include "power.h"
#include "wifi_auto.h"
#include "wifi_store.h"
#include "ui/account_list.h"
#include "ui/boot_screen.h"
#include "ui/keyboard.h"
#include "ui/settings.h"
#include "ui/sync_status.h"
#include "ui/wifi_list.h"

namespace {
    enum class Screen { AccountList, Settings, WifiList, Keyboard, SyncStatus };

    constexpr uint32_t ENCODER_KEY_DEBOUNCE_MS = 150;
    constexpr uint32_t HEADER_REFRESH_MS = 1000;

    Screen currentScreen = Screen::AccountList;
    long encoderPos = 0;
    bool encoderKeyWasPressed = false;
    uint32_t lastEncoderKeyEventMs = 0;
    uint32_t lastHeaderRefreshMs = 0;

    // The network picked in WifiList, carried through Keyboard (if a
    // password is needed) to SyncStatus.
    String pendingSsid;

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
    Battery::init();

    pixels.begin();
    pixels.setBrightness(40);
    pixels.clear();
    pixels.show();

    WifiStore::begin();

    BootScreen::play(tft);
    AccountList::draw(tft);

    // Started after the list is on screen -- the whole sequence runs from
    // loop(), so the UI is usable while it works in the background. The
    // header clock switches from "--:--" to the real time when it lands.
    WifiAuto::begin();

    Serial.println("Token is alive.");
}

void loop() {
    encoder.tick();

    long newPos = encoder.getPosition();
    if (newPos != encoderPos) {
        // The encoder's quadrature phase reads backwards relative to its
        // printed rotation direction on this board, so invert here rather
        // than swapping ENCODER_INA/ENCODER_INB.
        int delta = (int)(encoderPos - newPos);
        encoderPos = newPos;

        switch (currentScreen) {
            case Screen::AccountList: AccountList::scroll(tft, delta); break;
            case Screen::Settings: Settings::scroll(tft, delta); break;
            case Screen::WifiList: WifiList::scroll(tft, delta); break;
            case Screen::Keyboard: Keyboard::scroll(tft, delta); break;
            case Screen::SyncStatus: break; // read-only result screen
        }
    }

    if (pollEncoderKeyClicked()) {
        switch (currentScreen) {
            case Screen::AccountList:
                if (AccountList::isSettingsSelected()) {
                    currentScreen = Screen::Settings;
                    Settings::enter(tft);
                }
                break;

            case Screen::Settings:
                if (Settings::press(tft) == Settings::Action::OpenWifiList) {
                    // The manual flow drives the radio itself; a background
                    // auto-connect still in flight would be competing with
                    // it for the same hardware.
                    WifiAuto::cancel();
                    currentScreen = Screen::WifiList;
                    WifiList::enter(tft);
                }
                break;

            case Screen::WifiList:
                pendingSsid = WifiList::selectedSsid();
                if (WifiList::selectedIsOpen()) {
                    currentScreen = Screen::SyncStatus;
                    SyncStatus::run(tft, pendingSsid, "");
                } else {
                    currentScreen = Screen::Keyboard;
                    Keyboard::enter(tft, "Password:");
                }
                break;

            case Screen::Keyboard:
                if (Keyboard::press(tft)) {
                    currentScreen = Screen::SyncStatus;
                    SyncStatus::run(tft, pendingSsid, Keyboard::buffer());
                }
                break;

            case Screen::SyncStatus:
                break; // only the back button does anything here
        }
    }

    bool sidePressed = digitalRead(BOARD_USER_KEY) == LOW;
    bool backClicked = Power::pollShutdownButton(sidePressed, tft, pixels);
    if (backClicked) {
        switch (currentScreen) {
            case Screen::AccountList:
                break; // already top level

            case Screen::Settings:
                if (!Settings::back(tft)) {
                    currentScreen = Screen::AccountList;
                    AccountList::draw(tft);
                }
                break;

            case Screen::WifiList:
                currentScreen = Screen::Settings;
                Settings::redraw(tft);
                break;

            case Screen::Keyboard:
                // The side button backspaces here rather than navigating
                // back; an empty buffer instead cancels out to the list.
                if (!Keyboard::backspace(tft)) {
                    currentScreen = Screen::WifiList;
                    WifiList::redraw(tft);
                }
                break;

            case Screen::SyncStatus:
                currentScreen = Screen::Settings;
                Settings::redraw(tft);
                break;
        }
    }

    WifiAuto::poll();

    if (currentScreen == Screen::Settings) {
        Settings::poll(tft);
    }

    uint32_t now = millis();
    if (currentScreen == Screen::AccountList && now - lastHeaderRefreshMs >= HEADER_REFRESH_MS) {
        Battery::poll();
        AccountList::refreshHeaderWidgets(tft);
        lastHeaderRefreshMs = now;
    }

    delay(2);
}
