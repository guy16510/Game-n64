#pragma once

#include <cstdint>

namespace asteroid_pilot::boards::waveshare_349b_v2 {

inline constexpr int kNativeWidth = 172;
inline constexpr int kNativeHeight = 640;
inline constexpr int kLandscapeWidth = 640;
inline constexpr int kLandscapeHeight = 172;

inline constexpr int kLcdCs = 9;
inline constexpr int kLcdClock = 10;
inline constexpr int kLcdData0 = 11;
inline constexpr int kLcdData1 = 12;
inline constexpr int kLcdData2 = 13;
inline constexpr int kLcdData3 = 14;
inline constexpr int kLcdTe = 21;
inline constexpr int kLcdResetGpio = -1;  // Reset is TCA9554 bit 5 on V2.
inline constexpr int kBacklightPwm = 42;
inline constexpr int kExioInterrupt = 8;

inline constexpr int kTouchSda = 17;
inline constexpr int kTouchScl = 18;
inline constexpr std::uint8_t kTouchAddress = 0x3BU;

inline constexpr int kSystemSda = 47;
inline constexpr int kSystemScl = 48;
inline constexpr std::uint8_t kImuAddress = 0x6BU;
inline constexpr std::uint8_t kIoExpanderAddress = 0x20U;

inline constexpr std::uint8_t kExioTouchInt = 1U << 0U;
inline constexpr std::uint8_t kExioBacklightEnable = 1U << 1U;
inline constexpr std::uint8_t kExioImuInt1 = 1U << 2U;
inline constexpr std::uint8_t kExioImuInt2 = 1U << 3U;
inline constexpr std::uint8_t kExioRtcInt = 1U << 4U;
inline constexpr std::uint8_t kExioLcdReset = 1U << 5U;
inline constexpr std::uint8_t kExioSystemEnable = 1U << 6U;
inline constexpr std::uint8_t kExioNoiseSuppression = 1U << 7U;

}  // namespace asteroid_pilot::boards::waveshare_349b_v2
