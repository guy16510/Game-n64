#include "asteroid_pilot/board.hpp"

#include <cstdint>

#include "driver/gpio.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
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

constexpr std::uint32_t kExioTouchInt = 1U << 0U;
constexpr std::uint32_t kExioBacklightEnable = 1U << 1U;
constexpr std::uint32_t kExioImuInt1 = 1U << 2U;
constexpr std::uint32_t kExioImuInt2 = 1U << 3U;
constexpr std::uint32_t kExioLcdReset = 1U << 5U;

bool g_initialized = false;
i2c_master_bus_handle_t g_system_i2c = nullptr;
esp_io_expander_handle_t g_io_expander = nullptr;

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

esp_err_t initialize_expander() {
    esp_err_t err = esp_io_expander_new_i2c_tca9554(
        g_system_i2c,
        ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
        &g_io_expander);
    if (err != ESP_OK) {
        return err;
    }

    const std::uint32_t inputs = kExioTouchInt | kExioImuInt1 | kExioImuInt2;
    err = esp_io_expander_set_dir(g_io_expander, inputs, IO_EXPANDER_INPUT);
    if (err != ESP_OK) {
        return err;
    }

    if (board_revision() == BoardRevision::V2) {
        err = esp_io_expander_set_dir(
            g_io_expander,
            kExioBacklightEnable | kExioLcdReset,
            IO_EXPANDER_OUTPUT);
        if (err != ESP_OK) {
            return err;
        }
        err = esp_io_expander_set_level(g_io_expander, kExioBacklightEnable, 0);
        if (err != ESP_OK) {
            return err;
        }
        err = esp_io_expander_set_level(g_io_expander, kExioLcdReset, 1);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
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
    err = configure_output_gpio(pins.backlight_gpio, 0);
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
        esp_err_t err = esp_io_expander_set_level(g_io_expander, kExioLcdReset, 1);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
        err = esp_io_expander_set_level(g_io_expander, kExioLcdReset, 0);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        err = esp_io_expander_set_level(g_io_expander, kExioLcdReset, 1);
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
        const esp_err_t expander_err = esp_io_expander_set_level(
            g_io_expander,
            kExioBacklightEnable,
            enabled ? 1 : 0);
        if (expander_err != ESP_OK) {
            return expander_err;
        }
    }

    return gpio_set_level(
        static_cast<gpio_num_t>(board_pins().backlight_gpio),
        enabled ? 1 : 0);
}

}  // namespace asteroid_pilot::hardware
