# Architecture

## Runtime ownership

Gameplay rendering and UI rendering are deliberately separated.

```text
GAME MODE

Input adapters
  ├─ QMI8658 motion
  └─ touch actions
        ↓
platform-independent InputState
        ↓
core game simulation
        ↓
Jet scene construction / update
        ↓
RGB565 framebuffer
        ↓
ESP-IDF display adapter
        ↓
AXS15231B QSPI LCD
```

```text
MENU MODE

Touch
  ↓
LVGL
  ↓
ESP-IDF display adapter
  ↓
AXS15231B QSPI LCD
```

Jet should own the high-frequency framebuffer during gameplay. LVGL should initially be used only when the game renderer is not active.

## Platform-independent core

`core/` is ordinary C++17 and must not include ESP-IDF headers.

Responsibilities:

- deterministic fixed-timestep game simulation
- flight model
- asteroid state
- projectile state
- collision state
- scoring
- deterministic PRNG state
- input contracts

The same source is compiled by desktop CI and ESP-IDF firmware CI.

## Input boundary

The current contract is:

```cpp
struct InputState {
    float pitch;
    float roll;
    float yaw_rate;
    bool fire;
    bool boost;
};
```

Desktop tests synthesize this structure. Firmware will populate it from calibrated QMI8658 data and touch actions.

## Determinism

Simulation uses a fixed 60 Hz timestep and an explicit xorshift32 PRNG state.

Rendering tests use a pinned Jet commit and a deterministic scene.

The project maintains two different kinds of render checks:

1. same-run determinism, two identical native renders must match byte-for-byte
2. golden regression, a reviewed reference hash/image will be pinned after the first successful CI baseline

## Jet integration

Pinned upstream revision:

```text
CubeCoders/Jet
c56dfc0c012ba7a09a6e49b4123340ec24981441
```

Jet is built natively in CI using the same strategy used by JetExamples screenshot tooling: compile Jet's portable `src/*.cpp` files with a frontend-specific `JetConfig.hpp`.

The ESP32 firmware integration will use a separate embedded `JetConfig.hpp` tuned for half-width / field-buffer modes after the LCD transport is benchmarked.

## Firmware diagnostics

The initial firmware intentionally does not pretend display/IMU/touch work.

It emits JSON lines for:

- hardware information
- shared-core self-test
- `BOOT_OK`

The HIL harness parses those messages and only promotes capabilities it actually observed.

Future diagnostics will add explicit events such as:

```json
{"event":"display_test","status":"PASS"}
{"event":"imu_test","status":"PASS"}
{"event":"touch_test","status":"PASS"}
{"event":"render_benchmark","fps_avg":47.3,"fps_p99":38.9}
```

## Memory policy

Realtime code must avoid heap allocation in the game/render hot loop.

Preferred patterns:

- fixed-size arrays
- object pools
- startup-time asset allocation
- build-time asset conversion
- bounded particle systems

OBJ parsing and expensive image conversion should not occur during gameplay.
