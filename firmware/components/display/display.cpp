#include "asteroid_pilot/display.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "asteroid_pilot/board.hpp"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_axs15231b.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace asteroid_pilot::hardware {
namespace {

constexpr int kStripRows = 64;
constexpr std::size_t kTransferPixels =
    static_cast<std::size_t>(kNativeWidth) * static_cast<std::size_t>(kStripRows);
constexpr std::size_t kTransferBytes = kTransferPixels * sizeof(std::uint16_t);
constexpr int kTransferTimeoutMs = 1500;

static const axs15231b_lcd_init_cmd_t kLcdInitCommands[] = {
    {0x11, nullptr, 0, 100},
    {0x29, nullptr, 0, 100},
};

bool g_initialized = false;
esp_lcd_panel_io_handle_t g_panel_io = nullptr;
esp_lcd_panel_handle_t g_panel = nullptr;
SemaphoreHandle_t g_transfer_done = nullptr;
std::uint16_t* g_transfer_buffer = nullptr;

std::uint16_t byte_swap_565(std::uint16_t value) {
    return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
}

bool on_color_transfer_done(
    esp_lcd_panel_io_handle_t,
    esp_lcd_panel_io_event_data_t*,
    void* user_ctx) {
    auto semaphore = static_cast<SemaphoreHandle_t>(user_ctx);
    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(semaphore, &high_task_woken);
    return high_task_woken == pdTRUE;
}

esp_err_t transfer_native_strip(int y_start, int rows) {
    const esp_err_t draw_err = esp_lcd_panel_draw_bitmap(
        g_panel,
        0,
        y_start,
        kNativeWidth,
        y_start + rows,
        g_transfer_buffer);
    if (draw_err != ESP_OK) {
        return draw_err;
    }

    if (xSemaphoreTake(g_transfer_done, pdMS_TO_TICKS(kTransferTimeoutMs)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

std::uint16_t diagnostic_color(int native_x, int native_y) {
    // Long-axis color bands make rotation/orientation errors obvious on the
    // physical 3.49-inch panel. The white border and checker center expose
    // clipping, byte-order and transfer-corruption problems.
    if (native_x == 0 || native_x == (kNativeWidth - 1) ||
        native_y == 0 || native_y == (kNativeHeight - 1)) {
        return 0xFFFFU;
    }

    const int band = (native_y * 4) / kNativeHeight;
    switch (band) {
        case 0:
            return 0xF800U;  // red
        case 1:
            return 0x07E0U;  // green
        case 2:
            return 0x001FU;  // blue
        default:
            return (((native_x / 12) + (native_y / 12)) & 1) == 0 ? 0xFFFFU : 0x0000U;
    }
}

}  // namespace

esp_err_t display_init() {
    if (g_initialized) {
        return ESP_OK;
    }

    esp_err_t err = board_control_init();
    if (err != ESP_OK) {
        return err;
    }

    err = lcd_backlight_set(false);
    if (err != ESP_OK) {
        return err;
    }

    g_transfer_done = xSemaphoreCreateBinary();
    if (g_transfer_done == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    const BoardPins& pins = board_pins();

    spi_bus_config_t bus_config{};
    bus_config.sclk_io_num = pins.lcd_clock;
    bus_config.data0_io_num = pins.lcd_data0;
    bus_config.data1_io_num = pins.lcd_data1;
    bus_config.data2_io_num = pins.lcd_data2;
    bus_config.data3_io_num = pins.lcd_data3;
    bus_config.max_transfer_sz = static_cast<int>(kTransferBytes);

    err = spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }

    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.cs_gpio_num = static_cast<gpio_num_t>(pins.lcd_cs);
    io_config.dc_gpio_num = static_cast<gpio_num_t>(-1);
    io_config.spi_mode = 3;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 2;
    io_config.on_color_trans_done = on_color_transfer_done;
    io_config.user_ctx = g_transfer_done;
    io_config.lcd_cmd_bits = 32;
    io_config.lcd_param_bits = 8;
    io_config.flags.quad_mode = true;

    err = esp_lcd_new_panel_io_spi(
        static_cast<esp_lcd_spi_bus_handle_t>(SPI3_HOST),
        &io_config,
        &g_panel_io);
    if (err != ESP_OK) {
        return err;
    }

    axs15231b_vendor_config_t vendor_config{};
    vendor_config.init_cmds = kLcdInitCommands;
    vendor_config.init_cmds_size = static_cast<std::uint16_t>(
        sizeof(kLcdInitCommands) / sizeof(kLcdInitCommands[0]));
    vendor_config.flags.use_qspi_interface = 1;

    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = static_cast<gpio_num_t>(-1);
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = &vendor_config;

    err = esp_lcd_new_panel_axs15231b(g_panel_io, &panel_config, &g_panel);
    if (err != ESP_OK) {
        return err;
    }

    err = lcd_hardware_reset();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_lcd_panel_init(g_panel);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_lcd_panel_disp_on_off(g_panel, true);
    if (err != ESP_OK) {
        return err;
    }

    g_transfer_buffer = static_cast<std::uint16_t*>(
        heap_caps_malloc(kTransferBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (g_transfer_buffer == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    g_initialized = true;
    return ESP_OK;
}

esp_err_t display_draw_diagnostic(DisplayDiagnostic* diagnostic) {
    esp_err_t err = display_init();
    if (err != ESP_OK) {
        return err;
    }

    std::uint32_t transferred_pixels = 0U;
    for (int y_start = 0; y_start < kNativeHeight; y_start += kStripRows) {
        const int rows = std::min(kStripRows, kNativeHeight - y_start);
        for (int row = 0; row < rows; ++row) {
            for (int x = 0; x < kNativeWidth; ++x) {
                const std::size_t index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(kNativeWidth) +
                    static_cast<std::size_t>(x);
                g_transfer_buffer[index] = byte_swap_565(diagnostic_color(x, y_start + row));
            }
        }

        err = transfer_native_strip(y_start, rows);
        if (err != ESP_OK) {
            return err;
        }
        transferred_pixels += static_cast<std::uint32_t>(rows * kNativeWidth);
    }

    err = lcd_backlight_set(true);
    if (err != ESP_OK) {
        return err;
    }

    if (diagnostic != nullptr) {
        diagnostic->native_width = kNativeWidth;
        diagnostic->native_height = kNativeHeight;
        diagnostic->landscape_width = kLandscapeWidth;
        diagnostic->landscape_height = kLandscapeHeight;
        diagnostic->transferred_pixels = transferred_pixels;
    }

    return ESP_OK;
}

esp_err_t display_draw_landscape_rgb565(const std::uint16_t* framebuffer) {
    if (framebuffer == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = display_init();
    if (err != ESP_OK) {
        return err;
    }

    // Clockwise software rotation: a natural 640x172 game framebuffer becomes
    // the controller's 172x640 native memory orientation.
    for (int native_y_start = 0; native_y_start < kNativeHeight; native_y_start += kStripRows) {
        const int rows = std::min(kStripRows, kNativeHeight - native_y_start);
        for (int row = 0; row < rows; ++row) {
            const int native_y = native_y_start + row;
            const int source_x = native_y;
            for (int native_x = 0; native_x < kNativeWidth; ++native_x) {
                const int source_y = (kLandscapeHeight - 1) - native_x;
                const std::size_t source_index =
                    static_cast<std::size_t>(source_y) * static_cast<std::size_t>(kLandscapeWidth) +
                    static_cast<std::size_t>(source_x);
                const std::size_t destination_index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(kNativeWidth) +
                    static_cast<std::size_t>(native_x);
                g_transfer_buffer[destination_index] = byte_swap_565(framebuffer[source_index]);
            }
        }

        err = transfer_native_strip(native_y_start, rows);
        if (err != ESP_OK) {
            return err;
        }
    }

    return lcd_backlight_set(true);
}

esp_lcd_panel_handle_t display_panel() {
    return g_panel;
}

}  // namespace asteroid_pilot::hardware
