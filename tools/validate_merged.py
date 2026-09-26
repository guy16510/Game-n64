#!/usr/bin/env python3
"""Validate an ESP-IDF merged raw image against flasher_args.json."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

FLASH_SIZE_BYTES = 16 * 1024 * 1024


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="firmware/build")
    parser.add_argument("--image", required=True)
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    image = Path(args.image)
    if not image.is_absolute() and not image.exists():
        image = build_dir / image

    merged = image.read_bytes()
    if len(merged) > FLASH_SIZE_BYTES:
        raise SystemExit(f"merged image exceeds 16 MiB: {len(merged)} bytes")
    if not merged or merged[0] != 0xE9:
        raise SystemExit("merged image does not begin with ESP image magic 0xE9")

    metadata = json.loads((build_dir / "flasher_args.json").read_text(encoding="utf-8"))
    flash_files = metadata.get("flash_files")
    if not isinstance(flash_files, dict) or not flash_files:
        raise SystemExit("flasher_args.json does not contain flash_files")

    checked = []
    for offset_text, relative_path in flash_files.items():
        offset = int(offset_text, 0)
        segment_path = build_dir / relative_path
        segment = segment_path.read_bytes()
        end = offset + len(segment)
        if end > FLASH_SIZE_BYTES:
            raise SystemExit(f"segment {relative_path} exceeds 16 MiB flash boundary")
        if merged[offset:end] != segment:
            raise SystemExit(f"merged image mismatch at {offset_text} for {relative_path}")
        checked.append((offset_text, relative_path, len(segment)))

    print(f"Validated {image} ({len(merged)} bytes) against {len(checked)} ESP-IDF flash segments:")
    for offset, relative_path, size in checked:
        print(f"  {offset}: {relative_path} ({size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
