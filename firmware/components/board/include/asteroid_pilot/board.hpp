#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

namespace asteroid_pilot::hardware {

enum class BoardRevision : std::uint8_t {
    V1 = 1,
    V2 = 2,
};

struct BoardPins {
    int lcd_cs;
    int lcd_clock;
    int lcd_data0;
    int lcd_data1;
    int lcd_data2;
    int lcd_data3;
    int lcd_te;
    int lcd_reset_gpio;
    int backlight_gpio;
    int exio_interrupt_gpio;

    int touch_sda;
    int touch_scl;
    std::uint8_t touch_address;

    int system_sda;
    int system_scl;
    std::uint8_t imu_address;
    std::uint8_t io_expander_address;
};

inline constexpr int kNativeWidth = 172;
inline constexpr int kNativeHeight = 640;
inline constexpr int kLandscapeWidth = 640;
inline constexpr int kLandscapeHeight = 172;

BoardRevision board_revision();
const char* board_revision_name();
const BoardPins& board_pins();

// Initializes the shared system I2C bus, board expander and display control
// GPIOs. Calling this more than once is safe.
esp_err_t board_control_init();

i2c_master_bus_handle_t system_i2c_bus();

// Hardware-specific display controls. V2 reset/backlight enable use the
// TCA9554, while V1 uses direct GPIO for the revision-specific paths.
esp_err_t lcd_hardware_reset();
esp_err_t lcd_backlight_set(bool enabled);

}  // namespace asteroid_pilot::hardware
