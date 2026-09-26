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

## Validation model

Subsystems are tracked as:

- `UNTESTED`
- `CI_VERIFIED`
- `HIL_VERIFIED`

A successful compile is never treated as proof that physical hardware works.

## Renderer

Jet is pinned by commit and used as the realtime game renderer. Upstream: https://github.com/CubeCoders/Jet

## Project status

Bootstrap in progress. See `CODEX_PLAN.md` for the execution plan and validation gates.
