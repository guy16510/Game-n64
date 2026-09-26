# Asteroid Pilot ESP32-S3, Codex Build and Validation Plan

> Target: Waveshare ESP32-S3-Touch-LCD-3.49B
>
> Product goal: a polished first-person 3D futuristic aircraft / asteroid game using motion controls and touch.
>
> Engineering goal: GitHub should validate almost everything before a physical flash, then hardware-in-the-loop must validate the exact artifact intended for release.

## 1. Product vision

Build a realtime 3D game where the player physically pilots a futuristic aircraft by tilting the device.

Core experience:

- textured and lit 3D asteroid field
- first-person cockpit / HUD
- QMI8658 pitch and roll controls
- touch weapons, boost, shield, and pause
- lasers, particles, explosions, debris, fog, and LOD
- child-friendly Code Lab exposing simple gameplay variables
- LVGL menus outside the main realtime render loop

The target visual style is polished N64 / PS1 / early Dreamcast-inspired 3D, not ASCII and not a 2D sprite-only game.

## 2. Validation contract

Every subsystem has one state:

```text
UNTESTED
CI_VERIFIED
HIL_VERIFIED
```

- `UNTESTED`: no automated proof exists.
- `CI_VERIFIED`: validated on host simulation / GitHub-hosted CI.
- `HIL_VERIFIED`: validated on the actual target board.

Codex must never mark physical hardware as working merely because firmware compiled.

## 3. Technology stack

### Firmware

- ESP-IDF
- ESP32-S3 target
- C++17
- `esp_lcd`
- AXS15231B display driver
- QMI8658 IMU driver
- capacitive touch driver
- Jet 3D renderer pinned to an exact Git commit
- LVGL later for menus and configuration

### Rendering ownership

Gameplay:

```text
Jet -> RGB565 framebuffer -> display adapter -> esp_lcd -> AXS15231B -> LCD
```

Menu mode:

```text
LVGL -> display adapter -> LCD
```

Do not make LVGL and Jet fight over the display during the high-frequency game loop.

## 4. Repository structure

```text
.github/workflows/
  ci-host.yml
  ci-render.yml
  ci-firmware.yml
  hil.yml
  release.yml

boards/
  waveshare_349b_v1/
  waveshare_349b_v2/

core/
  game/
  flight/
  collision/
  world/
  physics/
  input/

host/
  simulator/
  render_test/
  benchmark/

firmware/
  CMakeLists.txt
  sdkconfig.defaults
  partitions.csv
  main/
  components/
    board/
    display/
    imu/
    touch/
    diagnostics/
    jet_bridge/
    app/

assets/
  source/models/
  source/textures/
  generated/

tests/
  unit/
  integration/
  scenarios/
  rendering/
  golden/
  hardware/

tools/
  build_assets.py
  rgb565_to_png.py
  compare_frames.py
  check_memory.py
  benchmark_report.py
  make_release.py
  hil.py

docs/
  ARCHITECTURE.md
  HARDWARE.md
  VALIDATION.md
  PERFORMANCE.md
  HIL.md
```

## 5. Platform-independent core

The core game must not include ESP-IDF headers.

Example input contract:

```cpp
struct InputState {
    float pitch;
    float roll;
    float yawRate;
    bool fire;
    bool boost;
};
```

Desktop tests provide synthetic input. Firmware provides the same structure from the QMI8658 and touch controller.

## 6. Dependency policy

Pin important dependencies:

- ESP-IDF release
- Jet exact commit SHA
- LVGL release
- display component version
- Python dependencies
- GitHub Actions versions

Do not upgrade dependencies as part of unrelated work.

Initial Jet pin:

```text
c56dfc0c012ba7a09a6e49b4123340ec24981441
```

## 7. Milestones

### M0, repository skeleton

Deliver CMake host build, ESP-IDF skeleton, CI, formatting, plan, and architecture docs.

Acceptance:

```text
host target compiles
ESP32-S3 target compiles
CI runs on PRs
```

### M1, desktop Jet renderer

Run Jet on Linux with a 640x172 RGB565 framebuffer.

Initial scene:

- perspective camera
- rotating textured asteroid
- directional light
- ambient light
- star background

Produce `.rgb565` and `.png` artifacts.

### M2, golden render tests

Deterministic scenes:

1. empty camera
2. single asteroid
3. textured asteroid
4. lighting
5. overlapping Z/depth scene
6. fog
7. sprite overlay
8. additive laser
9. particles
10. complete game frame

Store expected frame hashes and visual diffs. Golden frames may never update automatically.

### M3, deterministic game simulation

Use fixed timestep:

```cpp
constexpr float GAME_DT = 1.0f / 60.0f;
```

Use explicit PRNG seeds. Replay known inputs and assert transforms, velocity, projectiles, collisions, score, and PRNG state.

### M4, board abstraction

Keep board revision pin definitions isolated. Build V1 and V2 independently.

### M5, firmware CI

Build actual ESP-IDF firmware and archive:

- bootloader
- partition table
- application binary
- merged image
- ELF
- map file
- flash arguments

### M6, hardware diagnostics firmware

First physical firmware is diagnostic, not the game.

```text
BOOT
chip check
flash check
PSRAM check
LCD init
RGB/checker/gradient display test
touch init
IMU init
Jet scene
benchmark
PASS/FAIL
```

Serial diagnostics must be machine-readable JSON.

### M7, display bring-up

Bridge Jet framebuffer to the AXS15231B display.

Acceptance:

- correct orientation
- correct color order
- no offset
- no corruption
- known Jet frame visible

### M8, IMU

Expose:

```cpp
struct MotionState {
    float pitch;
    float roll;
    float yawRate;
    float accelerationMagnitude;
};
```

Apply calibration, low-pass filtering, and deadzone outside the game logic.

### M9, flight model

Arcade controls:

- roll device -> aircraft bank
- pitch device -> climb / dive
- shake -> boost

### M10, asteroid gameplay

Implement fly, evade, fire, destroy, particles/debris, score.

### M11, asset pipeline

Source assets remain OBJ/PNG. Build-time tools convert them to bounded device-ready formats.

Enforce polygon, texture, and asset-memory budgets in CI.

### M12, performance modes

Implement and measure:

```text
A: 640x172 progressive, target >= 30 FPS
B: 320x172 internal -> 640x172 output, target >= 45 FPS
C: half-width / interlaced optimization, target near 60 FPS
```

Only physical hardware can establish ESP32 FPS targets.

### M13, touch

Actions:

- fire
- shield / special
- pause
- optional boost

### M14, LVGL menus

Use LVGL for start, aircraft select, settings, IMU calibration, diagnostics, and Code Lab.

### M15, Kid Code Lab

Expose variables such as:

```cpp
struct KidGameConfig {
    uint8_t asteroidCount;
    uint8_t shipSpeed;
    uint8_t laserPower;
    bool infiniteBoost;
};
```

The UI should later show the equivalent code so gameplay customization becomes a bridge into programming.

## 8. GitHub Actions

### `ci-host.yml`

Run on every PR:

- GCC build
- Clang build
- warnings-as-errors where practical
- unit tests
- deterministic simulation
- ASan
- UBSan
- asset validation

### `ci-render.yml`

Render deterministic scenes and upload:

- expected PNG
- actual PNG
- diff PNG
- metadata JSON

Use strict framebuffer hashes in the pinned CI environment.

### `ci-firmware.yml`

Build matrix for board revision and render mode. Run `idf.py build` and `idf.py size`. Fail on memory/flash budget regressions.

### `hil.yml`

Runs only on trusted branches/tags or manual approval using a self-hosted runner attached to the physical ESP32.

Never run untrusted fork PR code on the HIL runner.

## 9. Hardware-in-the-loop contract

HIL downloads the exact artifact produced by GitHub-hosted CI. It must not rebuild firmware locally.

Sequence:

1. detect serial port
2. verify ESP32-S3 with `esptool`
3. flash exact `merged.bin`
4. reset
5. capture serial
6. require `BOOT_OK`
7. parse diagnostics JSON
8. verify flash and PSRAM
9. verify display init
10. verify touch init
11. verify QMI8658 init
12. run known render test
13. run 1000-frame benchmark
14. deterministic gameplay replay
15. memory leak check
16. 5-10 minute soak test
17. archive logs and metrics

## 10. Performance telemetry

Collect:

- average FPS
- p50/p95/p99 frame time
- minimum FPS
- free heap start/end
- largest free block
- PSRAM usage
- rendered triangles
- particle count
- watchdog resets
- unexpected resets

## 11. Optional physical pixel validation

Add a USB webcam over the hardware fixture.

```text
flash -> calibration image -> camera capture -> crop/perspective correction -> compare to expected
```

Detect black display, wrong orientation, RGB/BGR swap, mirroring, offset, and gross corruption.

## 12. Release gate

Stable release requires:

```text
host build
unit tests
deterministic replay
golden render tests
asset validation
firmware build
memory budgets
binary generation
HIL flash
physical boot
display init
IMU init
touch init
performance threshold
soak test
```

Release artifacts:

```text
asteroid-pilot-vX.Y.Z-waveshare-349b-v2.bin
SHA256SUMS
release-manifest.json
```

## 13. One-command flashing

Provide `flash.sh` and `flash.ps1` that detect the serial port, verify ESP32-S3, flash the merged image, reset, and optionally follow serial logs.

## 14. Realtime code rules

- no per-frame heap allocation
- no unbounded containers in hot paths
- no runtime OBJ parsing during gameplay
- no expensive asset conversion on-device
- preallocate particle/object pools
- preserve deterministic behavior where practical

## 15. Codex rules

1. Never claim hardware works because firmware compiled.
2. Keep ESP-IDF APIs out of `core/`.
3. Pin dependencies.
4. Do not silently upgrade Jet, ESP-IDF, LVGL, or drivers.
5. Do not automatically accept new golden frames.
6. Do not weaken tests just to make CI green.
7. Keep `main` buildable.
8. Add tests with every subsystem.
9. No per-frame heap allocation.
10. Measure render-path performance changes.
11. Keep board revisions isolated.
12. Build diagnostics before gameplay.
13. HIL-test the exact artifact intended for release.
14. Document anything that remains impossible to automate.

## 16. Execution order

```text
M0  Repository skeleton
M1  Desktop Jet renderer
M2  Golden-frame CI
M3  Deterministic game core
M4  Board abstraction
M5  ESP-IDF firmware CI
M6  Hardware diagnostics
M7  LCD bring-up
M8  Jet -> LCD bridge
M9  QMI8658
M10 Flight model
M11 Asteroid scene
M12 Shooting/collisions
M13 Particles
M14 Asset pipeline
M15 LOD
M16 Render-quality modes
M17 Touch
M18 Performance tuning
M19 LVGL menus
M20 Kid Code Lab
M21 HIL runner
M22 Soak testing
M23 Release pipeline
M24 One-command flasher
M25 Webcam visual validation
```

## 17. Initial definition of done

Version 0.1.0 is complete when:

```text
device boots reliably
display initializes
3D textured asteroid renders
camera responds to physical tilt
touch fires laser
asteroid can be destroyed
particles render
CI reproduces game simulation
CI reproduces renderer output
GitHub produces merged flash image
HIL flashes that exact image
HIL verifies physical boot
frame rate meets target
soak test finds no meaningful leak
one-command flashing works
```
