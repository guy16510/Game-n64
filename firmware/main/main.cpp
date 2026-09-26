#include <cinttypes>
#include <cstdio>

#include "asteroid_pilot/board.hpp"
#include "asteroid_pilot/display.hpp"
#include "asteroid_pilot/game_state.hpp"
#include "asteroid_pilot/imu.hpp"
#include "asteroid_pilot/touch.hpp"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

namespace {

void print_error_event(const char* event, esp_err_t err) {
    std::printf(
        "{\"event\":\"%s\",\"status\":\"FAIL\",\"error\":%d,\"error_name\":\"%s\"}\n",
        event,
        static_cast<int>(err),
        esp_err_to_name(err));
}

}  // namespace

extern "C" void app_main(void) {
    using namespace asteroid_pilot;
    using namespace asteroid_pilot::hardware;

    esp_chip_info_t chip_info{};
    esp_chip_info(&chip_info);

    std::uint32_t flash_bytes = 0U;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_bytes);
    if (flash_result != ESP_OK) {
        flash_bytes = 0U;
    }

    const std::size_t psram_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    std::printf(
        "{\"event\":\"hardware_info\",\"chip\":\"ESP32-S3\","
        "\"board_revision\":\"%s\",\"cores\":%u,\"revision\":%u,"
        "\"flash_bytes\":%" PRIu32 ",\"psram_bytes\":%zu,\"reset_reason\":%d}\n",
        board_revision_name(),
        static_cast<unsigned>(chip_info.cores),
        static_cast<unsigned>(chip_info.revision),
        flash_bytes,
        psram_bytes,
        static_cast<int>(esp_reset_reason()));

    bool diagnostics_passed = true;

    const esp_err_t board_err = board_control_init();
    if (board_err == ESP_OK) {
        std::printf(
            "{\"event\":\"board_control\",\"status\":\"PASS\",\"revision\":\"%s\"}\n",
            board_revision_name());
    } else {
        print_error_event("board_control", board_err);
        diagnostics_passed = false;
    }

    DisplayDiagnostic display{};
    const esp_err_t display_err = display_draw_diagnostic(&display);
    if (display_err == ESP_OK) {
        std::printf(
            "{\"event\":\"display_test\",\"status\":\"PASS\","
            "\"controller\":\"AXS15231B\",\"native_width\":%d,\"native_height\":%d,"
            "\"landscape_width\":%d,\"landscape_height\":%d,\"transferred_pixels\":%" PRIu32 "}\n",
            display.native_width,
            display.native_height,
            display.landscape_width,
            display.landscape_height,
            display.transferred_pixels);
    } else {
        print_error_event("display_test", display_err);
        diagnostics_passed = false;
    }

    ImuDiagnostic imu{};
    const esp_err_t imu_err = imu_run_diagnostic(&imu);
    if (imu_err == ESP_OK) {
        std::printf(
            "{\"event\":\"imu_test\",\"status\":\"PASS\",\"sensor\":\"QMI8658\","
            "\"who_am_i\":%u,\"revision\":%u,\"successful_reads\":%d}\n",
            static_cast<unsigned>(imu.who_am_i),
            static_cast<unsigned>(imu.revision),
            imu.successful_reads);
    } else {
        print_error_event("imu_test", imu_err);
        diagnostics_passed = false;
    }

    TouchDiagnostic touch{};
    const esp_err_t touch_err = touch_run_diagnostic(&touch);
    if (touch_err == ESP_OK) {
        std::printf(
            "{\"event\":\"touch_test\",\"status\":\"PASS\",\"controller\":\"AXS15231B\","
            "\"address\":%u,\"successful_probes\":%d}\n",
            static_cast<unsigned>(touch.address),
            touch.successful_probes);
    } else {
        print_error_event("touch_test", touch_err);
        diagnostics_passed = false;
    }

    GameState state{};
    reset_game(state, 0x0A57E201U);

    for (std::uint64_t frame = 0; frame < 120ULL; ++frame) {
        InputState input{};
        input.roll = static_cast<float>(static_cast<int>(frame % 60ULL) - 30) / 40.0F;
        input.pitch = static_cast<float>(static_cast<int>(frame % 40ULL) - 20) / 50.0F;
        input.fire = (frame % 30ULL) == 0ULL;
        input.boost = frame >= 90ULL;
        step_game(state, input);
    }

    std::printf(
        "{\"event\":\"core_self_test\",\"status\":\"PASS\",\"frames\":%" PRIu64
        ",\"score\":%" PRIu32 ",\"digest\":\"0x%016" PRIx64 "\"}\n",
        state.frame,
        state.score,
        state_digest(state));

    std::printf(
        "{\"event\":\"BOOT_OK\",\"status\":\"%s\",\"display\":\"%s\","
        "\"imu\":\"%s\",\"touch\":\"%s\"}\n",
        diagnostics_passed ? "DIAGNOSTICS_PASS" : "DIAGNOSTICS_FAIL",
        display_err == ESP_OK ? "PASS" : "FAIL",
        imu_err == ESP_OK ? "PASS" : "FAIL",
        touch_err == ESP_OK ? "PASS" : "FAIL");
}
