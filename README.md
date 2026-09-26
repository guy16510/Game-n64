# Asteroid Pilot ESP32-S3

A first-person 3D asteroid-flight game for the **Waveshare ESP32-S3-Touch-LCD-3.49B**.

The player physically tilts the device to pilot a futuristic aircraft through a realtime 3D asteroid field. Jet provides the native RGB565 3D renderer, ESP-IDF provides the firmware/runtime, and LVGL is reserved for menus and configuration screens.

## Goals

- Real textured 3D rendering on the ESP32-S3
- 640x172 landscape output with adaptive lower-cost render modes
- QMI8658 motion controls for pitch and roll
- Touch controls for weapons, boost, shield, and pause
- Deterministic host simulation and render regression testing in GitHub Actions
- ESP-IDF firmware builds for Waveshare board revisions
- Hardware-in-the-loop validation using the exact release artifact
- One-command flashing after hardware validation
- A child-friendly Code Lab for changing visible game variables

## Board revision, do not guess

Waveshare changed the LCD control routing between revisions. Build and flash the artifact that matches the physical board:

- **V2 / Rev1.1**: PCB silkscreen says `Rev1.1` or the case QC label says `V2`. Waveshare says units shipped on or after June 8, 2026 use V2.
- **V1**: earlier board. Use the `v1` artifact.

The V1 and V2 images are intentionally separate so the wrong LCD reset/backlight routing cannot be selected by accident.

## Flashing a CI image

GitHub Actions produces a single merged image for each board revision:

- `esp32s3-firmware-v2/asteroid-pilot-v2-merged.bin`
- `esp32s3-firmware-v1/asteroid-pilot-v1-merged.bin`

Install esptool once:

```bash
python3 -m pip install --upgrade esptool
```

Then, from the extracted artifact or repository root, flash with:

```bash
python3 tools/flash_firmware.py --board v2
```

The script will auto-detect the port only when there is exactly one likely USB serial device. Otherwise pass it explicitly, for example:

```bash
python3 tools/flash_firmware.py --board v2 --port /dev/cu.usbmodem101
```

The flasher refuses a mismatched board/image filename, checks the merged image shape, confirms the connected target is an ESP32-S3, writes the complete image at `0x0`, then runs `esptool verify-flash` against the exact binary.

If the board will not enter download mode, hold **BOOT**, press and release **RESET**, then release **BOOT** and run the flash command again.

After boot, serial output is 115200 baud. The hardware diagnostic firmware should draw the panel test pattern and emit a final JSON event whose `event` is `BOOT_OK`. A physical HIL run is still the only proof of the actual panel, IMU, touch device, and connected board revision.

## Validation model

Subsystems are tracked as:

- `UNTESTED`
- `CI_VERIFIED`
- `HIL_VERIFIED`

A successful compile is never treated as proof that physical hardware works.

Firmware CI validates both board revisions, creates the raw merged image, and compares every bootloader/partition/app segment in that merged binary against ESP-IDF's generated `flasher_args.json` offsets before publishing the artifact.

## Renderer

Jet is pinned by commit and used as the realtime game renderer. Upstream: https://github.com/CubeCoders/Jet

## Project status

Bootstrap in progress. See `CODEX_PLAN.md` for the execution plan and validation gates.
