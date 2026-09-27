#include <cinttypes>
#include <cstddef>
#include <cstdint>
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

constexpr std::uint32_t kExpectedFlashBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMinimumPsramBytes = 7U * 1024U * 1024U;
constexpr std::size_t kPsramTestBytes = 512U * 1024U;
constexpr std::uint64_t kExpectedCoreDigest = 0x29c50c6c2dce480eULL;

void print_error_event(const char* event, esp_err_t err) {
    std::printf(
        "{\"event\":\"%s\",\"status\":\"FAIL\",\"error\":%d,\"error_name\":\"%s\"}\n",
        event,
        static_cast<int>(err),
        esp_err_to_name(err));
}

const char* detected_chip_name(const esp_chip_info_t& chip_info) {
    return chip_info.model == CHIP_ESP32S3 ? "ESP32-S3" : "UNEXPECTED";
}

esp_err_t run_psram_memory_test(std::size_t* tested_bytes) {
    if (tested_bytes != nullptr) {
        *tested_bytes = 0U;
    }

    void* raw = heap_caps_malloc(kPsramTestBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (raw == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    auto* words = static_cast<volatile std::uint32_t*>(raw);
    constexpr std::size_t kWordCount = kPsramTestBytes / sizeof(std::uint32_t);
    for (std::size_t i = 0; i < kWordCount; ++i) {
        const std::uint32_t index = static_cast<std::uint32_t>(i);
        words[i] = 0xA5A50000U ^ (index * 0x9E3779B9U);
    }

    esp_err_t result = ESP_OK;
    for (std::size_t i = 0; i < kWordCount; ++i) {
        const std::uint32_t index = static_cast<std::uint32_t>(i);
        const std::uint32_t expected = 0xA5A50000U ^ (index * 0x9E3779B9U);
        if (words[i] != expected) {
            result = ESP_ERR_INVALID_RESPONSE;
            break;
        }
    }

    heap_caps_free(raw);
    if (result == ESP_OK && tested_bytes != nullptr) {
        *tested_bytes = kPsramTestBytes;
    }
    return result;
}

}  // namespace

extern "C" void app_main(void) {
    using namespace asteroid_pilot;
    using namespace asteroid_pilot::hardware;

    esp_chip_info_t chip_info{};
    esp_chip_info(&chip_info);

    std::uint32_t flash_bytes = 0U;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_bytes);
    const std::size_t psram_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    const bool chip_ok = chip_info.model == CHIP_ESP32S3;
    const bool flash_ok = flash_result == ESP_OK && flash_bytes >= kExpectedFlashBytes;
    const bool psram_capacity_ok = psram_bytes >= kMinimumPsramBytes;

    std::printf(
        "{\"event\":\"hardware_info\",\"chip\":\"%s\",\"chip_model\":%d,"
        "\"board_revision\":\"%s\",\"cores\":%u,\"revision\":%u,"
        "\"flash_bytes\":%" PRIu32 ",\"psram_bytes\":%zu,\"reset_reason\":%d}\n",
        detected_chip_name(chip_info),
        static_cast<int>(chip_info.model),
        board_revision_name(),
        static_cast<unsigned>(chip_info.cores),
        static_cast<unsigned>(chip_info.revision),
        flash_bytes,
        psram_bytes,
        static_cast<int>(esp_reset_reason()));

    bool diagnostics_passed = true;

    const bool platform_ok = chip_ok && flash_ok && psram_capacity_ok;
    std::printf(
        "{\"event\":\"platform_test\",\"status\":\"%s\",\"chip_ok\":%s,"
        "\"flash_ok\":%s,\"psram_capacity_ok\":%s}\n",
        platform_ok ? "PASS" : "FAIL",
        chip_ok ? "true" : "false",
        flash_ok ? "true" : "false",
        psram_capacity_ok ? "true" : "false");
    if (!platform_ok) {
        diagnostics_passed = false;
    }

    std::size_t psram_tested_bytes = 0U;
    const esp_err_t psram_test_err = run_psram_memory_test(&psram_tested_bytes);
    if (psram_test_err == ESP_OK) {
        std::printf(
            "{\"event\":\"memory_test\",\"status\":\"PASS\",\"psram_tested_bytes\":%zu}\n",
            psram_tested_bytes);
    } else {
        print_error_event("memory_test", psram_test_err);
        diagnostics_passed = false;
    }

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

    const std::uint64_t core_digest = state_digest(state);
    const bool core_ok = state.frame == 120ULL && core_digest == kExpectedCoreDigest;
    std::printf(
        "{\"event\":\"core_self_test\",\"status\":\"%s\",\"frames\":%" PRIu64
        ",\"score\":%" PRIu32 ",\"digest\":\"0x%016" PRIx64
        "\",\"expected_digest\":\"0x%016" PRIx64 "\"}\n",
        core_ok ? "PASS" : "FAIL",
        state.frame,
        state.score,
        core_digest,
        kExpectedCoreDigest);
    if (!core_ok) {
        diagnostics_passed = false;
    }

    std::printf(
        "{\"event\":\"DIAGNOSTICS_COMPLETE\",\"status\":\"%s\",\"display\":\"%s\","
        "\"imu\":\"%s\",\"touch\":\"%s\"}\n",
        diagnostics_passed ? "PASS" : "FAIL",
        display_err == ESP_OK ? "PASS" : "FAIL",
        imu_err == ESP_OK ? "PASS" : "FAIL",
        touch_err == ESP_OK ? "PASS" : "FAIL");

    std::printf(
        "{\"event\":\"%s\",\"status\":\"%s\",\"board_revision\":\"%s\"}\n",
        diagnostics_passed ? "BOOT_OK" : "BOOT_FAIL",
        diagnostics_passed ? "PASS" : "FAIL",
        board_revision_name());
}
