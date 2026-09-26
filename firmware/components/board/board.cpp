#include "asteroid_pilot/board.hpp"

#include <cstdint>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace asteroid_pilot::hardware {
namespace {

constexpr BoardPins kV1Pins{
    .lcd_cs = 9,
    .lcd_clock = 10,
    .lcd_data0 = 11,
    .lcd_data1 = 12,
    .lcd_data2 = 13,
    .lcd_data3 = 14,
    .lcd_te = -1,
    .lcd_reset_gpio = 21,
    .backlight_gpio = 8,
    .exio_interrupt_gpio = 42,
    .touch_sda = 17,
    .touch_scl = 18,
    .touch_address = 0x3BU,
    .system_sda = 47,
    .system_scl = 48,
    .imu_address = 0x6BU,
    .io_expander_address = 0x20U,
};

constexpr BoardPins kV2Pins{
    .lcd_cs = 9,
    .lcd_clock = 10,
    .lcd_data0 = 11,
    .lcd_data1 = 12,
    .lcd_data2 = 13,
    .lcd_data3 = 14,
    .lcd_te = 21,
    .lcd_reset_gpio = -1,
    .backlight_gpio = 42,
    .exio_interrupt_gpio = 8,
    .touch_sda = 17,
    .touch_scl = 18,
    .touch_address = 0x3BU,
    .system_sda = 47,
    .system_scl = 48,
    .imu_address = 0x6BU,
    .io_expander_address = 0x20U,
};

constexpr std::uint8_t kTcaOutputRegister = 0x01U;
constexpr std::uint8_t kTcaConfigRegister = 0x03U;
constexpr std::uint8_t kExioTouchInt = 1U << 0U;
constexpr std::uint8_t kExioBacklightEnable = 1U << 1U;
constexpr std::uint8_t kExioImuInt1 = 1U << 2U;
constexpr std::uint8_t kExioImuInt2 = 1U << 3U;
constexpr std::uint8_t kExioLcdReset = 1U << 5U;
constexpr int kI2cTimeoutMs = 100;
constexpr std::uint32_t kSystemI2cSpeedHz = 400000U;

bool g_initialized = false;
i2c_master_bus_handle_t g_system_i2c = nullptr;
i2c_master_dev_handle_t g_io_expander = nullptr;
std::uint8_t g_expander_output = 0xFFU;

esp_err_t configure_output_gpio(int gpio_number, int initial_level) {
    if (gpio_number < 0) {
        return ESP_OK;
    }

    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << static_cast<unsigned>(gpio_number);
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return gpio_set_level(static_cast<gpio_num_t>(gpio_number), initial_level);
}

esp_err_t initialize_system_i2c() {
    const BoardPins& pins = board_pins();

    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = static_cast<gpio_num_t>(pins.system_sda);
    config.scl_io_num = static_cast<gpio_num_t>(pins.system_scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.intr_priority = 0;
    config.trans_queue_depth = 0;
    config.flags.enable_internal_pullup = true;

    return i2c_new_master_bus(&config, &g_system_i2c);
}

esp_err_t expander_read_register(std::uint8_t reg, std::uint8_t* value) {
    if (g_io_expander == nullptr || value == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(
        g_io_expander,
        &reg,
        sizeof(reg),
        value,
        sizeof(*value),
        kI2cTimeoutMs);
}

esp_err_t expander_write_register(std::uint8_t reg, std::uint8_t value) {
    if (g_io_expander == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const std::uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(g_io_expander, payload, sizeof(payload), kI2cTimeoutMs);
}

esp_err_t expander_set_level(std::uint8_t mask, bool high) {
    if (high) {
        g_expander_output = static_cast<std::uint8_t>(g_expander_output | mask);
    } else {
        g_expander_output = static_cast<std::uint8_t>(g_expander_output & ~mask);
    }
    return expander_write_register(kTcaOutputRegister, g_expander_output);
}

esp_err_t initialize_expander() {
    if (board_revision() != BoardRevision::V2) {
        return ESP_OK;
    }

    const BoardPins& pins = board_pins();
    i2c_device_config_t device_config{};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = pins.io_expander_address;
    device_config.scl_speed_hz = kSystemI2cSpeedHz;

    esp_err_t err = i2c_master_bus_add_device(g_system_i2c, &device_config, &g_io_expander);
    if (err != ESP_OK) {
        return err;
    }

    std::uint8_t config = 0xFFU;
    err = expander_read_register(kTcaConfigRegister, &config);
    if (err != ESP_OK) {
        return err;
    }
    err = expander_read_register(kTcaOutputRegister, &g_expander_output);
    if (err != ESP_OK) {
        return err;
    }

    // Match Waveshare's V2 BSP without taking ownership of unrelated EXIO pins.
    // Inputs: touch INT, IMU INT1/INT2. Outputs: backlight enable and LCD reset.
    config = static_cast<std::uint8_t>(config | kExioTouchInt | kExioImuInt1 | kExioImuInt2);
    config = static_cast<std::uint8_t>(config & ~(kExioBacklightEnable | kExioLcdReset));

    // Set safe output levels before changing the direction bits so the panel
    // cannot briefly flash or reset from stale TCA9554 output-latch values.
    g_expander_output = static_cast<std::uint8_t>(
        (g_expander_output & ~kExioBacklightEnable) | kExioLcdReset);
    err = expander_write_register(kTcaOutputRegister, g_expander_output);
    if (err != ESP_OK) {
        return err;
    }
    return expander_write_register(kTcaConfigRegister, config);
}

}  // namespace

BoardRevision board_revision() {
#if CONFIG_AP_BOARD_V1
    return BoardRevision::V1;
#else
    return BoardRevision::V2;
#endif
}

const char* board_revision_name() {
    return board_revision() == BoardRevision::V1 ? "v1" : "v2";
}

const BoardPins& board_pins() {
    return board_revision() == BoardRevision::V1 ? kV1Pins : kV2Pins;
}

esp_err_t board_control_init() {
    if (g_initialized) {
        return ESP_OK;
    }

    esp_err_t err = initialize_system_i2c();
    if (err != ESP_OK) {
        return err;
    }

    err = initialize_expander();
    if (err != ESP_OK) {
        return err;
    }

    const BoardPins& pins = board_pins();
    // LCD_BL is active-low on both Waveshare revisions. Start dark so V1
    // cannot flash before the panel has finished its reset/init sequence.
    err = configure_output_gpio(pins.backlight_gpio, 1);
    if (err != ESP_OK) {
        return err;
    }

    if (board_revision() == BoardRevision::V1) {
        err = configure_output_gpio(pins.lcd_reset_gpio, 1);
        if (err != ESP_OK) {
            return err;
        }
    }

    g_initialized = true;
    return ESP_OK;
}

i2c_master_bus_handle_t system_i2c_bus() {
    return g_system_i2c;
}

esp_err_t lcd_hardware_reset() {
    if (!g_initialized) {
        const esp_err_t init_err = board_control_init();
        if (init_err != ESP_OK) {
            return init_err;
        }
    }

    if (board_revision() == BoardRevision::V2) {
        esp_err_t err = expander_set_level(kExioLcdReset, true);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
        err = expander_set_level(kExioLcdReset, false);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        err = expander_set_level(kExioLcdReset, true);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
        return ESP_OK;
    }

    const gpio_num_t reset_gpio = static_cast<gpio_num_t>(board_pins().lcd_reset_gpio);
    esp_err_t err = gpio_set_level(reset_gpio, 1);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
    err = gpio_set_level(reset_gpio, 0);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
    err = gpio_set_level(reset_gpio, 1);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
    return ESP_OK;
}

esp_err_t lcd_backlight_set(bool enabled) {
    if (!g_initialized) {
        const esp_err_t init_err = board_control_init();
        if (init_err != ESP_OK) {
            return init_err;
        }
    }

    if (board_revision() == BoardRevision::V2) {
        const esp_err_t expander_err = expander_set_level(kExioBacklightEnable, enabled);
        if (expander_err != ESP_OK) {
            return expander_err;
        }
    }

    // Waveshare's brightness presets are inverted (255 brightness == duty 0),
    // so a low LCD_BL level is full-on and a high level is off.
    return gpio_set_level(
        static_cast<gpio_num_t>(board_pins().backlight_gpio),
        enabled ? 0 : 1);
}

}  // namespace asteroid_pilot::hardware
