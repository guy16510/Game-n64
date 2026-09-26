# Validation Strategy

## Status vocabulary

Every subsystem is classified as one of:

- `UNTESTED`
- `CI_VERIFIED`
- `HIL_VERIFIED`

A compiler success cannot produce `HIL_VERIFIED`.

## CI layers

### Host core

`Host CI` builds the portable C++ game core with GCC and Clang, runs unit tests, executes a deterministic replay, and runs ASan/UBSan against the core.

This validates:

- game state transitions
- deterministic seeded simulation
- fixed input behavior
- host memory safety
- compiler portability

### Native Jet render

Jet is pinned to an exact upstream commit. The native smoke target renders a 640x172 RGB565 scene containing real Jet geometry and lighting.

The render workflow executes the exact same scene twice and requires identical output bytes.

The first successful run establishes the candidate golden render. A later reviewed commit will pin the reference SHA256 instead of silently updating it.

### Firmware CI

The firmware workflow uses ESP-IDF 6.0.3 and builds for `esp32s3`.

It validates:

- Xtensa cross-compilation
- shared `core/` compatibility with ESP-IDF
- 16 MB flash configuration
- octal PSRAM configuration
- partition layout
- dependency resolution for `esp_lcd_axs15231b` 2.1.1
- application/bootloader/partition output
- merged flash image generation

Firmware CI does not validate physical hardware.

## HIL layer

The HIL workflow runs only on a trusted self-hosted runner physically attached to the board.

It downloads the exact `esp32s3-firmware` artifact from a selected successful firmware run. It does not rebuild firmware.

Current baseline HIL checks:

- esptool sees ESP32-S3
- exact merged artifact flashes successfully
- firmware boots
- firmware reports expected flash size
- firmware reports expected PSRAM capacity
- shared game core self-test executes on the ESP32
- `BOOT_OK` is observed

Display, IMU and touch remain `UNTESTED` until their diagnostic implementations land.

## Future HIL gates

### Display

- initialize AXS15231B
- enable V1/V2-correct backlight/reset path
- RGB primary-color test
- checkerboard
- gradient
- known RGB565 frame
- Jet scene
- optionally verify physical pixels with a fixture camera

### IMU

- QMI8658 responds at expected address
- accelerometer magnitude is plausible at rest
- gyro values are finite
- 1,000 consecutive reads succeed
- calibration and deadzone behavior passes

### Touch

- controller initializes
- valid coordinate frames can be read
- coordinate transformation agrees with landscape display orientation

### Performance

- 1,000-frame benchmark
- average FPS
- p50/p95/p99 frame time
- minimum FPS
- rendered triangle count
- heap/PSRAM start and end
- unexpected reset count

### Soak

At least five minutes of automated gameplay simulation with no watchdog, panic, unexpected reset or meaningful memory loss.

## Release rule

No stable release should be created until the exact release binary has passed the currently required HIL gates.
