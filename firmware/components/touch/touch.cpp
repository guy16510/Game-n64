#include "asteroid_pilot/touch.hpp"

#include "asteroid_pilot/board.hpp"
#include "driver/i2c_master.h"

namespace asteroid_pilot::hardware {
namespace {

constexpr int kProbeCount = 50;
constexpr int kProbeTimeoutMs = 100;
i2c_master_bus_handle_t g_touch_bus = nullptr;

esp_err_t ensure_touch_bus() {
    if (g_touch_bus != nullptr) {
        return ESP_OK;
    }

    const BoardPins& pins = board_pins();
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_1;
    config.sda_io_num = static_cast<gpio_num_t>(pins.touch_sda);
    config.scl_io_num = static_cast<gpio_num_t>(pins.touch_scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.intr_priority = 0;
    config.trans_queue_depth = 0;
    config.flags.enable_internal_pullup = true;

    return i2c_new_master_bus(&config, &g_touch_bus);
}

}  // namespace

esp_err_t touch_run_diagnostic(TouchDiagnostic* diagnostic) {
    esp_err_t err = ensure_touch_bus();
    if (err != ESP_OK) {
        return err;
    }

    const std::uint8_t address = board_pins().touch_address;
    int successful_probes = 0;
    for (int i = 0; i < kProbeCount; ++i) {
        err = i2c_master_probe(g_touch_bus, address, kProbeTimeoutMs);
        if (err != ESP_OK) {
            return err;
        }
        ++successful_probes;
    }

    if (diagnostic != nullptr) {
        diagnostic->address = address;
        diagnostic->successful_probes = successful_probes;
    }

    return ESP_OK;
}

}  // namespace asteroid_pilot::hardware
