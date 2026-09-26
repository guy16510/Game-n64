#!/usr/bin/env python3
"""Hardware-in-the-loop validator for the Waveshare ESP32-S3 target.

The validator only promotes capabilities actually observed on the physical
board. Display transport is distinct from visual pixel validation, IMU presence
is distinct from calibrated motion, and touch presence is distinct from human
coordinate interaction.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--board", choices=["v1", "v2"], required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=45.0)
    parser.add_argument("--report", default="hil-report.json")
    return parser.parse_args()


def as_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def main() -> int:
    args = parse_args()
    deadline = time.monotonic() + args.timeout
    events: dict[str, dict[str, Any]] = {}
    raw_lines: list[str] = []

    with serial.Serial(args.port, args.baud, timeout=0.25) as port:
        # Attempt a hardware reset after the monitor owns the port so boot JSON
        # is not lost between the esptool step and this process.
        try:
            port.dtr = False
            port.rts = True
            time.sleep(0.12)
            port.rts = False
            time.sleep(0.25)
        except (OSError, ValueError):
            pass

        required = {
            "hardware_info",
            "display_test",
            "imu_test",
            "touch_test",
            "core_self_test",
            "BOOT_OK",
        }

        while time.monotonic() < deadline:
            raw = port.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            raw_lines.append(line)
            print(line)

            start = line.find("{")
            if start < 0:
                continue

            try:
                payload = json.loads(line[start:])
            except json.JSONDecodeError:
                continue

            event = payload.get("event")
            if isinstance(event, str):
                events[event] = payload

            if required.issubset(events):
                break

    failures: list[str] = []

    hardware = events.get("hardware_info")
    if hardware is None:
        failures.append("missing hardware_info event")
    else:
        if hardware.get("chip") != "ESP32-S3":
            failures.append(f"unexpected chip: {hardware.get('chip')!r}")
        if hardware.get("board_revision") != args.board:
            failures.append(
                f"firmware board revision {hardware.get('board_revision')!r} does not match physical selection {args.board!r}"
            )
        if as_int(hardware.get("flash_bytes")) < 15 * 1024 * 1024:
            failures.append(f"flash size too small: {hardware.get('flash_bytes')!r}")
        if as_int(hardware.get("psram_bytes")) < 7 * 1024 * 1024:
            failures.append(f"PSRAM size too small: {hardware.get('psram_bytes')!r}")

    display = events.get("display_test")
    if display is None:
        failures.append("missing display_test event")
    elif display.get("status") != "PASS":
        failures.append(f"display transport diagnostic failed: {display}")
    else:
        if as_int(display.get("native_width")) != 172 or as_int(display.get("native_height")) != 640:
            failures.append(f"unexpected display geometry: {display}")
        if as_int(display.get("transferred_pixels")) != 172 * 640:
            failures.append(f"incomplete diagnostic framebuffer transfer: {display}")

    imu = events.get("imu_test")
    if imu is None:
        failures.append("missing imu_test event")
    elif imu.get("status") != "PASS":
        failures.append(f"IMU presence diagnostic failed: {imu}")
    else:
        if as_int(imu.get("who_am_i")) != 0x05:
            failures.append(f"unexpected QMI8658 WHO_AM_I: {imu.get('who_am_i')!r}")
        if as_int(imu.get("successful_reads")) < 1000:
            failures.append(f"insufficient stable IMU reads: {imu.get('successful_reads')!r}")

    touch = events.get("touch_test")
    if touch is None:
        failures.append("missing touch_test event")
    elif touch.get("status") != "PASS":
        failures.append(f"touch presence diagnostic failed: {touch}")
    elif as_int(touch.get("successful_probes")) < 50:
        failures.append(f"insufficient stable touch probes: {touch.get('successful_probes')!r}")

    core = events.get("core_self_test")
    if core is None:
        failures.append("missing core_self_test event")
    else:
        if core.get("status") != "PASS":
            failures.append(f"core self-test failed: {core}")
        if as_int(core.get("frames")) != 120:
            failures.append(f"core self-test frame count mismatch: {core.get('frames')!r}")

    boot = events.get("BOOT_OK")
    if boot is None:
        failures.append("missing BOOT_OK event")
    elif boot.get("status") != "DIAGNOSTICS_PASS":
        failures.append(f"firmware diagnostics did not pass: {boot}")

    passed = not failures
    report = {
        "passed": passed,
        "board_revision": args.board,
        "failures": failures,
        "events": events,
        "raw_serial": raw_lines,
        "validation_scope": {
            "boot": "HIL_VERIFIED" if passed else "UNTESTED",
            "display_transport": "HIL_VERIFIED" if passed else "UNTESTED",
            "display_physical_pixels": "UNTESTED",
            "imu_presence": "HIL_VERIFIED" if passed else "UNTESTED",
            "imu_calibrated_motion": "UNTESTED",
            "touch_presence": "HIL_VERIFIED" if passed else "UNTESTED",
            "touch_coordinates": "UNTESTED",
        },
    }
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if failures:
        for failure in failures:
            print(f"HIL FAILURE: {failure}", file=sys.stderr)
        return 1

    print(
        "HIL diagnostics passed: boot, flash/PSRAM, display transport, QMI8658 presence, "
        "touch presence, and shared core are verified on the physical board."
    )
    print("Physical pixel appearance, calibrated IMU motion, and touch coordinates remain unverified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
