#include "asteroid_pilot/imu.hpp"

#include <cstdint>

#include "asteroid_pilot/board.hpp"
#include "driver/i2c_master.h"

namespace asteroid_pilot::hardware {
namespace {

constexpr std::uint8_t kWhoAmIRegister = 0x00U;
constexpr std::uint8_t kRevisionRegister = 0x01U;
constexpr std::uint8_t kExpectedWhoAmI = 0x05U;
constexpr int kDiagnosticReads = 1000;
constexpr int kTimeoutMs = 100;

i2c_master_dev_handle_t g_imu = nullptr;

esp_err_t ensure_device() {
    if (g_imu != nullptr) {
        return ESP_OK;
    }

    esp_err_t err = board_control_init();
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t device_config{};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = board_pins().imu_address;
    device_config.scl_speed_hz = 400000;

    return i2c_master_bus_add_device(system_i2c_bus(), &device_config, &g_imu);
}

esp_err_t read_register(std::uint8_t address, std::uint8_t* value) {
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(
        g_imu,
        &address,
        sizeof(address),
        value,
        sizeof(*value),
        kTimeoutMs);
}

}  // namespace

esp_err_t imu_run_diagnostic(ImuDiagnostic* diagnostic) {
    esp_err_t err = ensure_device();
    if (err != ESP_OK) {
        return err;
    }

    std::uint8_t who_am_i = 0U;
    err = read_register(kWhoAmIRegister, &who_am_i);
    if (err != ESP_OK) {
        return err;
    }
    if (who_am_i != kExpectedWhoAmI) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    std::uint8_t revision = 0U;
    err = read_register(kRevisionRegister, &revision);
    if (err != ESP_OK) {
        return err;
    }

    int successful_reads = 0;
    for (int i = 0; i < kDiagnosticReads; ++i) {
        std::uint8_t observed = 0U;
        err = read_register(kWhoAmIRegister, &observed);
        if (err != ESP_OK) {
            return err;
        }
        if (observed != kExpectedWhoAmI) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        ++successful_reads;
    }

    if (diagnostic != nullptr) {
        diagnostic->who_am_i = who_am_i;
        diagnostic->revision = revision;
        diagnostic->successful_reads = successful_reads;
    }

    return ESP_OK;
}

}  // namespace asteroid_pilot::hardware
