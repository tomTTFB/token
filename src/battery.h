#pragma once

#include <cstdint>

namespace Battery {
    // Probes the BQ25896 charger over I2C. Safe to call even if no battery
    // or charger is fitted -- isAvailable() reports whether it responded.
    bool init();

    // Re-reads voltage and charge state from the BQ25896. Cheap enough to
    // call every time the header widget redraws.
    void poll();

    // Percentage estimated from battery voltage against a typical single-cell
    // LiPo discharge curve -- there's no coulomb-counting gauge wired up, so
    // this is an estimate, not a precise state of charge. 0 if the charger
    // was never found.
    uint8_t percent();

    bool isCharging();

    // False until init() has successfully talked to the chip.
    bool isAvailable();
}
