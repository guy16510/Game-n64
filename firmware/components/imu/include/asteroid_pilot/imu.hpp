#pragma once

#include <cstdint>

#include "esp_err.h"

namespace asteroid_pilot::hardware {

struct ImuDiagnostic {
    std::uint8_t who_am_i = 0;
    std::uint8_t revision = 0;
    int successful_reads = 0;
};

// Verifies QMI8658 presence and performs repeated register reads. This does not
// yet configure the accelerometer/gyro for gameplay, so motion data remains a
// later milestone.
esp_err_t imu_run_diagnostic(ImuDiagnostic* diagnostic = nullptr);

}  // namespace asteroid_pilot::hardware
