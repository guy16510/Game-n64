#include <cinttypes>
#include <cstdio>

#include "asteroid_pilot/game_state.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

extern "C" void app_main(void) {
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
        "\"cores\":%u,\"revision\":%u,\"flash_bytes\":%" PRIu32
        ",\"psram_bytes\":%zu,\"reset_reason\":%d}\n",
        static_cast<unsigned>(chip_info.cores),
        static_cast<unsigned>(chip_info.revision),
        flash_bytes,
        psram_bytes,
        static_cast<int>(esp_reset_reason()));

    asteroid_pilot::GameState state{};
    asteroid_pilot::reset_game(state, 0x0A57E201U);

    for (std::uint64_t frame = 0; frame < 120ULL; ++frame) {
        asteroid_pilot::InputState input{};
        input.roll = static_cast<float>(static_cast<int>(frame % 60ULL) - 30) / 40.0F;
        input.pitch = static_cast<float>(static_cast<int>(frame % 40ULL) - 20) / 50.0F;
        input.fire = (frame % 30ULL) == 0ULL;
        input.boost = frame >= 90ULL;
        asteroid_pilot::step_game(state, input);
    }

    std::printf(
        "{\"event\":\"core_self_test\",\"frames\":%" PRIu64
        ",\"score\":%" PRIu32 ",\"digest\":\"0x%016" PRIx64 "\"}\n",
        state.frame,
        state.score,
        asteroid_pilot::state_digest(state));

    std::printf(
        "{\"event\":\"BOOT_OK\",\"status\":\"CI_FIRMWARE_BASELINE\","
        "\"display\":\"UNTESTED\",\"imu\":\"UNTESTED\",\"touch\":\"UNTESTED\"}\n");
}
