#pragma once

#include <cstdint>

#include "esp_err.h"

namespace asteroid_pilot::hardware {

struct TouchDiagnostic {
    std::uint8_t address = 0;
    int successful_probes = 0;
};

// Verifies that the AXS15231B touch endpoint ACKs on its dedicated I2C bus.
// This validates transport/presence only. Human touch-coordinate behavior is a
// separate HIL milestone.
esp_err_t touch_run_diagnostic(TouchDiagnostic* diagnostic = nullptr);

}  // namespace asteroid_pilot::hardware
