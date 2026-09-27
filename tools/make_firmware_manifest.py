#!/usr/bin/env python3
"""Create a provenance manifest for one merged firmware image."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

FLASH_SIZE_BYTES = 16 * 1024 * 1024


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", choices=["v1", "v2"], required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--workflow-run-id", required=True)
    parser.add_argument("--idf-version", default="6.0.3")
    args = parser.parse_args()

    image = Path(args.image)
    expected_name = f"asteroid-pilot-{args.board}-merged.bin"
    if image.name != expected_name:
        raise SystemExit(
            f"board {args.board} requires image name {expected_name!r}, got {image.name!r}"
        )
    if not image.is_file():
        raise SystemExit(f"image not found: {image}")

    size = image.stat().st_size
    if size <= 0x10000 or size > FLASH_SIZE_BYTES:
        raise SystemExit(f"implausible merged image size: {size} bytes")

    source_sha = args.source_sha.lower()
    if len(source_sha) != 40 or any(ch not in "0123456789abcdef" for ch in source_sha):
        raise SystemExit(f"invalid source SHA: {args.source_sha!r}")

    manifest = {
        "schema_version": 1,
        "board_revision": args.board,
        "source": {
            "sha": source_sha,
            "ref": args.source_ref,
            "workflow_run_id": str(args.workflow_run_id),
        },
        "toolchain": {
            "esp_idf": args.idf_version,
            "target": "esp32s3",
        },
        "flash": {
            "image_offset": "0x0",
            "size_bytes": FLASH_SIZE_BYTES,
            "mode": "dio",
            "frequency": "80m",
        },
        "image": {
            "filename": image.name,
            "bytes": size,
            "sha256": sha256_file(image),
        },
    }

    output = Path(args.output)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote firmware manifest: {output}")
    print(f"Image SHA256: {manifest['image']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
