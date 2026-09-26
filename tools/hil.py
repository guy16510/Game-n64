#!/usr/bin/env python3
"""Minimal hardware-in-the-loop serial validator.

This intentionally validates only facts that the current diagnostic firmware
actually reports. Display, IMU and touch remain UNTESTED until those drivers are
implemented and emit explicit diagnostic events.
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
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--report", default="hil-report.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    deadline = time.monotonic() + args.timeout
    events: dict[str, dict[str, Any]] = {}
    raw_lines: list[str] = []

    with serial.Serial(args.port, args.baud, timeout=0.25) as port:
        # Pulse reset through USB-UART control lines when supported. Some native
        # USB/JTAG setups ignore this harmlessly, in which case the preceding
        # esptool write-flash reset supplies the boot.
        try:
            port.dtr = False
            port.rts = True
            time.sleep(0.08)
            port.rts = False
        except (OSError, ValueError):
            pass

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

            if "BOOT_OK" in events and "hardware_info" in events and "core_self_test" in events:
                break

    failures: list[str] = []

    hardware = events.get("hardware_info")
    if hardware is None:
        failures.append("missing hardware_info event")
    else:
        if hardware.get("chip") != "ESP32-S3":
            failures.append(f"unexpected chip: {hardware.get('chip')!r}")
        if int(hardware.get("flash_bytes", 0)) < 15 * 1024 * 1024:
            failures.append(f"flash size too small: {hardware.get('flash_bytes')!r}")
        if int(hardware.get("psram_bytes", 0)) < 7 * 1024 * 1024:
            failures.append(f"PSRAM size too small: {hardware.get('psram_bytes')!r}")

    core = events.get("core_self_test")
    if core is None:
        failures.append("missing core_self_test event")
    elif int(core.get("frames", 0)) != 120:
        failures.append(f"core self-test frame count mismatch: {core.get('frames')!r}")

    boot = events.get("BOOT_OK")
    if boot is None:
        failures.append("missing BOOT_OK event")

    report = {
        "passed": not failures,
        "failures": failures,
        "events": events,
        "raw_serial": raw_lines,
        "validation_scope": {
            "boot": "HIL_VERIFIED" if not failures else "FAILED",
            "display": "UNTESTED",
            "imu": "UNTESTED",
            "touch": "UNTESTED",
        },
    }
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if failures:
        for failure in failures:
            print(f"HIL FAILURE: {failure}", file=sys.stderr)
        return 1

    print("HIL baseline passed: boot, flash, PSRAM and shared-core self-test verified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
