# Hardware Target

## Primary board

Waveshare **ESP32-S3-Touch-LCD-3.49B**.

Expected platform characteristics:

- ESP32-S3R8, dual-core up to 240 MHz
- 8 MB PSRAM
- 16 MB flash
- 3.49-inch IPS LCD
- native panel geometry 172x640, used as 640x172 landscape for the game
- AXS15231B LCD/touch controller
- QSPI LCD transport
- QMI8658 6-axis IMU
- TCA9554 GPIO expander

Official product documentation:

- https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.49
- https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.49/Resources-And-Documents

## Board revisions

Waveshare documents a V1 and V2 revision. V1 is discontinued, and units shipped on or after June 8, 2026 are V2.

V2 identification:

- PCB silkscreen includes `Rev1.1`
- case QC label includes `V2`

Important revision changes documented by Waveshare:

- TP_INT exposure changed
- LCD_TE and LCD_RESET paths were swapped
- LCD_BL and EXIO_INT GPIOs were swapped
- battery charging was revised

This is why board wiring is isolated under `boards/` rather than scattered through display code.

## V2 wiring baseline

The V2 values below are taken from Waveshare's official ESP-IDF LVGL v9 example.

### LCD

| Signal | Value |
|---|---:|
| SPI host | SPI3 |
| CS | GPIO 9 |
| CLK | GPIO 10 |
| DATA0 | GPIO 11 |
| DATA1 | GPIO 12 |
| DATA2 | GPIO 13 |
| DATA3 | GPIO 14 |
| TE | GPIO 21 |
| LCD reset | TCA9554 bit 5 |
| Backlight PWM | GPIO 42 |
| EXIO interrupt | GPIO 8 |

### Touch

| Signal | Value |
|---|---:|
| SDA | GPIO 17 |
| SCL | GPIO 18 |
| address | `0x3B` |

### System I2C

| Signal | Value |
|---|---:|
| SDA | GPIO 47 |
| SCL | GPIO 48 |
| QMI8658 | `0x6B` |
| TCA9554 | `0x20` |

Official V2 source:

https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2

## V1 wiring baseline

The V1 abstraction is intentionally separate. Known V1 values include:

- LCD reset on GPIO 21
- backlight on GPIO 8
- EXIO interrupt on GPIO 42
- LCD QSPI pins remain GPIO 9 through 14
- touch remains GPIO 17/18
- system I2C remains GPIO 47/48

The V1 path must receive its own physical HIL validation before being labeled `HIL_VERIFIED`.

## Current hardware status

| Capability | Status |
|---|---|
| Host game core | CI target exists |
| Native Jet renderer | CI target exists |
| ESP32-S3 firmware compile | CI target exists |
| Flash/PSRAM boot diagnostics | HIL harness exists, physical run pending |
| LCD | UNTESTED |
| QMI8658 | UNTESTED |
| Touch | UNTESTED |
| Audio | UNTESTED |

No hardware subsystem is considered working merely because its code compiles.
