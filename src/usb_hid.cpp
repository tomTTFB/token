#include "usb_hid.h"

#include <USB.h>
#include <USBHIDKeyboard.h>

namespace {
    USBHIDKeyboard keyboard;
    bool started = false;
}

void UsbHid::begin() {
    keyboard.begin();
    USB.begin();
    started = true;
}

void UsbHid::typeCode(const String &code, bool pressEnter) {
    if (!started) return;

    keyboard.print(code);
    if (pressEnter) keyboard.write(KEY_RETURN);
}
