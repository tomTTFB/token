#include "battery.h"

// XPOWERS_CHIP_BQ25896 is defined project-wide in platformio.ini.
#include <XPowersLib.h>

#include "board_pins.h"

namespace {
    // Single-cell LiPo open-circuit-voltage -> state-of-charge curve. Not a
    // substitute for a coulomb-counting gauge, but a reasonable estimate
    // with only a voltage reading available.
    struct VoltagePoint {
        uint16_t millivolts;
        uint8_t percent;
    };

    const VoltagePoint CURVE[] = {
        {3000, 0}, {3300, 5}, {3500, 10}, {3600, 20}, {3650, 30},
        {3700, 40}, {3750, 50}, {3800, 60}, {3850, 70}, {3900, 80},
        {4000, 90}, {4100, 95}, {4200, 100},
    };
    constexpr int CURVE_POINTS = sizeof(CURVE) / sizeof(CURVE[0]);

    uint8_t voltageToPercent(uint16_t mv) {
        if (mv <= CURVE[0].millivolts) return CURVE[0].percent;
        if (mv >= CURVE[CURVE_POINTS - 1].millivolts) return CURVE[CURVE_POINTS - 1].percent;

        for (int i = 1; i < CURVE_POINTS; i++) {
            if (mv <= CURVE[i].millivolts) {
                const VoltagePoint &lo = CURVE[i - 1];
                const VoltagePoint &hi = CURVE[i];
                float frac = (float)(mv - lo.millivolts) / (hi.millivolts - lo.millivolts);
                return (uint8_t)(lo.percent + frac * (hi.percent - lo.percent));
            }
        }
        return CURVE[CURVE_POINTS - 1].percent; // unreachable
    }

    XPowersPPM charger;
    bool available = false;
    uint8_t lastPercent = 0;
    bool charging = false;
}

bool Battery::init() {
    available = charger.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, BQ25896_SLAVE_ADDRESS);
    if (available) {
        charger.enableMeasure();
        poll();
    }
    return available;
}

void Battery::poll() {
    if (!available) return;
    lastPercent = voltageToPercent(charger.getBattVoltage());
    charging = charger.isCharging();
}

uint8_t Battery::percent() {
    return available ? lastPercent : 0;
}

bool Battery::isCharging() {
    return available && charging;
}

bool Battery::isAvailable() {
    return available;
}
