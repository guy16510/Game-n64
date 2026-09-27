#!/usr/bin/env python3
"""Validate a downloaded firmware artifact against trusted workflow metadata."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

EXPECTED_IDF_VERSION = "6.0.3"
EXPECTED_FLASH_SIZE = 16 * 1024 * 1024


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_manifest(
    manifest: dict[str, Any],
    source_run: dict[str, Any],
    board: str,
    image: Path,
) -> list[str]:
    failures: list[str] = []

    if manifest.get("schema_version") != 1:
        failures.append(f"unexpected manifest schema: {manifest.get('schema_version')!r}")
    if manifest.get("board_revision") != board:
        failures.append(
            f"manifest board {manifest.get('board_revision')!r} does not match requested board {board!r}"
        )

    source = manifest.get("source") or {}
    if source.get("sha") != source_run.get("head_sha"):
        failures.append(
            f"manifest source SHA {source.get('sha')!r} does not match trusted run {source_run.get('head_sha')!r}"
        )
    if str(source.get("workflow_run_id")) != str(source_run.get("run_id")):
        failures.append(
            f"manifest workflow run {source.get('workflow_run_id')!r} does not match trusted run {source_run.get('run_id')!r}"
        )

    toolchain = manifest.get("toolchain") or {}
    if toolchain.get("target") != "esp32s3":
        failures.append(f"unexpected manifest target: {toolchain.get('target')!r}")
    if toolchain.get("esp_idf") != EXPECTED_IDF_VERSION:
        failures.append(f"unexpected ESP-IDF version: {toolchain.get('esp_idf')!r}")

    flash = manifest.get("flash") or {}
    expected_flash = {
        "image_offset": "0x0",
        "size_bytes": EXPECTED_FLASH_SIZE,
        "mode": "dio",
        "frequency": "80m",
    }
    for key, expected in expected_flash.items():
        if flash.get(key) != expected:
            failures.append(
                f"unexpected flash {key}: {flash.get(key)!r}, expected {expected!r}"
            )

    image_meta = manifest.get("image") or {}
    if image_meta.get("filename") != image.name:
        failures.append(
            f"manifest filename {image_meta.get('filename')!r} does not match {image.name!r}"
        )
    if image_meta.get("bytes") != image.stat().st_size:
        failures.append(
            f"manifest size {image_meta.get('bytes')!r} does not match {image.stat().st_size}"
        )
    actual_sha = sha256_file(image)
    if image_meta.get("sha256") != actual_sha:
        failures.append(
            f"manifest SHA256 {image_meta.get('sha256')!r} does not match calculated {actual_sha}"
        )

    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", choices=["v1", "v2"], required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--source-run", required=True)
    args = parser.parse_args()

    image = Path(args.image)
    if not image.is_file():
        raise SystemExit(f"firmware image not found: {image}")

    manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    source_run = json.loads(Path(args.source_run).read_text(encoding="utf-8"))
    failures = validate_manifest(manifest, source_run, args.board, image)
    if failures:
        for failure in failures:
            print(f"ARTIFACT VALIDATION FAILURE: {failure}")
        return 1

    print(
        f"Firmware artifact verified: {image.name}, board={args.board}, "
        f"source={source_run['head_sha']}, sha256={sha256_file(image)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
