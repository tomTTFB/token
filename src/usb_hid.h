#pragma once

#include <Arduino.h>

// USB HID keyboard emulation, used to type a TOTP code straight into
// whatever field has focus on the host machine instead of the user
// transcribing it by hand. Relies on the ESP32-S3's native USB-OTG
// peripheral (see ARDUINO_USB_MODE in platformio.ini) rather than the
// USB-Serial-JTAG controller the board defaults to.
namespace UsbHid {
    // Starts the composite USB device (CDC for Serial + HID keyboard). Call
    // once during setup(), before the first typeCode().
    void begin();

    // Types `code` as a sequence of keystrokes, followed by Enter if
    // `pressEnter` is set. No-op if begin() hasn't run.
    void typeCode(const String &code, bool pressEnter);
}
