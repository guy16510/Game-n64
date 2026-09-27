#!/usr/bin/env python3
"""Flash a CI-produced Asteroid Pilot merged image onto an ESP32-S3.

The script deliberately requires an explicit board revision because V1 and V2
route LCD control signals differently. It validates the image shape and optional
published checksum, verifies that the connected MCU reports as an ESP32-S3,
writes the merged image at 0x0, and asks esptool to verify flash afterwards.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import subprocess
import sys
from pathlib import Path

FLASH_SIZE_BYTES = 16 * 1024 * 1024
ESP_IMAGE_MAGIC = 0xE9
SUPPORTED_USB_VIDS = {0x303A, 0x1A86, 0x10C4, 0x0403}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", choices=["v1", "v2"], required=True)
    parser.add_argument("--port", help="Serial port. If omitted, exactly one likely USB serial device must be present.")
    parser.add_argument("--image", help="Merged .bin. Defaults to asteroid-pilot-<board>-merged.bin.")
    parser.add_argument("--checksum-file", help="SHA256SUMS file. If omitted, an adjacent SHA256SUMS is used when present.")
    parser.add_argument("--require-checksum", action="store_true", help="Fail unless the image checksum is validated.")
    parser.add_argument("--baud", type=int, default=460800)
    return parser.parse_args()


def require_esptool() -> None:
    if importlib.util.find_spec("esptool") is None or importlib.util.find_spec("serial") is None:
        raise SystemExit(
            "esptool/pyserial are required. Install them with: python3 -m pip install 'esptool==5.3.1' 'pyserial==3.5'"
        )


def auto_detect_port() -> str:
    from serial.tools import list_ports

    likely = []
    for info in list_ports.comports():
        description = (info.description or "").lower()
        manufacturer = (info.manufacturer or "").lower()
        looks_usb = info.vid in SUPPORTED_USB_VIDS or any(
            token in description or token in manufacturer
            for token in ("esp32", "espressif", "usb", "uart", "wch", "cp210", "ftdi", "jtag")
        )
        if looks_usb:
            likely.append(info.device)

    likely = sorted(set(likely))
    if len(likely) == 1:
        print(f"Auto-detected serial port: {likely[0]}")
        return likely[0]
    if not likely:
        raise SystemExit("No likely ESP32 USB serial port found. Reconnect the board and pass --port explicitly.")
    raise SystemExit(
        "Multiple likely USB serial ports found: " + ", ".join(likely) + ". Pass --port explicitly."
    )


def resolve_image(board: str, explicit: str | None) -> Path:
    expected_name = f"asteroid-pilot-{board}-merged.bin"
    candidates = [Path(explicit)] if explicit else [Path(expected_name), Path("firmware/build") / expected_name]
    for candidate in candidates:
        if candidate.is_file():
            if candidate.name != expected_name:
                raise SystemExit(
                    f"Refusing image {candidate.name!r}: --board {board} requires filename {expected_name!r}."
                )
            return candidate.resolve()
    raise SystemExit(
        f"Could not find {expected_name}. Download/extract the matching GitHub Actions artifact or pass --image."
    )


def validate_image(path: Path) -> None:
    size = path.stat().st_size
    if size <= 0x10000:
        raise SystemExit(f"Merged image is implausibly small ({size} bytes): {path}")
    if size > FLASH_SIZE_BYTES:
        raise SystemExit(f"Merged image exceeds the board's 16 MiB flash ({size} bytes): {path}")
    with path.open("rb") as handle:
        first = handle.read(1)
    if first != bytes([ESP_IMAGE_MAGIC]):
        raise SystemExit(
            f"Merged image does not start with ESP image magic 0x{ESP_IMAGE_MAGIC:02x}: {path}"
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_checksum_file(image: Path, explicit: str | None) -> Path | None:
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            raise SystemExit(f"Checksum file does not exist: {path}")
        return path.resolve()
    adjacent = image.parent / "SHA256SUMS"
    return adjacent if adjacent.is_file() else None


def validate_checksum(image: Path, checksum_file: Path | None, required: bool) -> None:
    if checksum_file is None:
        if required:
            raise SystemExit("Checksum validation is required but no SHA256SUMS file was provided or found beside the image.")
        print("WARNING: no SHA256SUMS found, image integrity is not independently authenticated.")
        return

    expected: str | None = None
    for raw_line in checksum_file.read_text(encoding="utf-8").splitlines():
        parts = raw_line.strip().split()
        if len(parts) < 2:
            continue
        filename = parts[-1].lstrip("*")
        if Path(filename).name == image.name:
            expected = parts[0].lower()
            break

    if expected is None:
        raise SystemExit(f"No checksum entry for {image.name!r} in {checksum_file}")
    if len(expected) != 64 or any(ch not in "0123456789abcdef" for ch in expected):
        raise SystemExit(f"Invalid SHA256 entry for {image.name!r} in {checksum_file}: {expected!r}")

    actual = sha256_file(image)
    if actual != expected:
        raise SystemExit(
            f"SHA256 mismatch for {image.name}: expected {expected}, calculated {actual}. Refusing to flash."
        )
    print(f"SHA256 verified: {actual}")


def esptool(port: str, baud: int, *args: str) -> None:
    command = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        "esp32s3",
        "--port",
        port,
        "--baud",
        str(baud),
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        *args,
    ]
    print("+", " ".join(command))
    subprocess.run(command, check=True)


def main() -> int:
    args = parse_args()
    require_esptool()
    image = resolve_image(args.board, args.image)
    validate_image(image)
    checksum_file = resolve_checksum_file(image, args.checksum_file)
    validate_checksum(image, checksum_file, args.require_checksum)
    port = args.port or auto_detect_port()

    print(f"Board revision: {args.board}")
    print(f"Image: {image}")
    print("Checking target chip before erase/write...")
    esptool(port, args.baud, "chip-id")

    print("Writing the complete merged image at flash offset 0x0...")
    esptool(
        port,
        args.baud,
        "write-flash",
        "--flash-mode",
        "dio",
        "--flash-freq",
        "80m",
        "--flash-size",
        "16MB",
        "0x0",
        str(image),
    )

    print("Verifying flash contents against the exact merged image...")
    esptool(port, args.baud, "verify-flash", "0x0", str(image))

    print("Flash and verification completed successfully.")
    print("The board should hard-reset automatically. Watch serial at 115200 baud for BOOT_OK.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
