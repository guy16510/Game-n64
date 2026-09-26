#pragma once

#include <cstdint>

namespace asteroid_pilot::boards::waveshare_349b_v1 {

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
inline constexpr int kLcdResetGpio = 21;
inline constexpr int kBacklightPwm = 8;
inline constexpr int kExioInterrupt = 42;

inline constexpr int kTouchSda = 17;
inline constexpr int kTouchScl = 18;
inline constexpr std::uint8_t kTouchAddress = 0x3BU;

inline constexpr int kSystemSda = 47;
inline constexpr int kSystemScl = 48;
inline constexpr std::uint8_t kImuAddress = 0x6BU;
inline constexpr std::uint8_t kIoExpanderAddress = 0x20U;

// V1 and V2 swap the LCD reset/TE path and the backlight/EXIO interrupt GPIOs.
// These values are intentionally isolated here so V1 hardware cannot silently
// inherit V2 wiring.

}  // namespace asteroid_pilot::boards::waveshare_349b_v1
