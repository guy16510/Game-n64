#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

namespace asteroid_pilot::hardware {

struct DisplayDiagnostic {
    int native_width = 0;
    int native_height = 0;
    int landscape_width = 0;
    int landscape_height = 0;
    std::uint32_t transferred_pixels = 0;
};

// Initializes the AXS15231B QSPI panel using the active board revision.
esp_err_t display_init();

// Draws a deterministic RGB/checkerboard diagnostic in the controller's native
// 172x640 orientation and waits for every DMA transfer to finish.
esp_err_t display_draw_diagnostic(DisplayDiagnostic* diagnostic = nullptr);

// Accepts the game's natural 640x172 RGB565 framebuffer and rotates it into
// the panel's native 172x640 transfer orientation. Input RGB565 values are host
// endian, the adapter performs the required byte swap for the panel transport.
esp_err_t display_draw_landscape_rgb565(const std::uint16_t* framebuffer);

esp_lcd_panel_handle_t display_panel();

}  // namespace asteroid_pilot::hardware
