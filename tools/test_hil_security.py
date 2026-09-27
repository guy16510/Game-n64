#!/usr/bin/env python3
"""Host-side regression tests for the HIL trust boundary."""

from __future__ import annotations

import copy
import hashlib
import tempfile
from pathlib import Path

from validate_firmware_artifact import validate_manifest
from validate_hil_run import validate_run_payload

REPOSITORY = "guy16510/Game-n64"
HEAD_SHA = "0123456789abcdef0123456789abcdef01234567"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def trusted_payload() -> dict:
    return {
        "id": 12345,
        "status": "completed",
        "conclusion": "success",
        "head_branch": "main",
        "head_sha": HEAD_SHA,
        "event": "push",
        "path": ".github/workflows/ci-firmware.yml",
        "head_repository": {"full_name": REPOSITORY},
        "repository": {"full_name": REPOSITORY},
    }


def must_reject(payload: dict, label: str) -> None:
    try:
        validate_run_payload(payload, REPOSITORY)
    except ValueError:
        return
    raise AssertionError(f"untrusted workflow run was accepted: {label}")


def test_run_trust() -> None:
    trusted = validate_run_payload(trusted_payload(), REPOSITORY)
    require(trusted["head_sha"] == HEAD_SHA, "trusted run lost head SHA")

    cases = []
    for field, value in (
        ("event", "pull_request"),
        ("head_branch", "codex/evil"),
        ("conclusion", "failure"),
        ("status", "in_progress"),
        ("path", ".github/workflows/ci-host.yml"),
    ):
        payload = trusted_payload()
        payload[field] = value
        cases.append((payload, f"{field}={value}"))

    payload = trusted_payload()
    payload["head_repository"] = {"full_name": "attacker/fork"}
    cases.append((payload, "fork head repository"))

    for payload, label in cases:
        must_reject(payload, label)


def test_manifest_binding() -> None:
    source_run = validate_run_payload(trusted_payload(), REPOSITORY)
    with tempfile.TemporaryDirectory() as temp_dir:
        image = Path(temp_dir) / "asteroid-pilot-v2-merged.bin"
        image.write_bytes(b"firmware-image" * 100)
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        manifest = {
            "schema_version": 1,
            "board_revision": "v2",
            "source": {
                "sha": HEAD_SHA,
                "ref": "main",
                "workflow_run_id": "12345",
            },
            "toolchain": {"esp_idf": "6.0.3", "target": "esp32s3"},
            "flash": {
                "image_offset": "0x0",
                "size_bytes": 16 * 1024 * 1024,
                "mode": "dio",
                "frequency": "80m",
            },
            "image": {
                "filename": image.name,
                "bytes": image.stat().st_size,
                "sha256": digest,
            },
        }
        require(not validate_manifest(manifest, source_run, "v2", image), "valid manifest rejected")

        tampered = copy.deepcopy(manifest)
        tampered["image"]["sha256"] = "0" * 64
        require(validate_manifest(tampered, source_run, "v2", image), "tampered image hash accepted")

        wrong_source = copy.deepcopy(manifest)
        wrong_source["source"]["sha"] = "f" * 40
        require(validate_manifest(wrong_source, source_run, "v2", image), "wrong source SHA accepted")


def main() -> int:
    test_run_trust()
    test_manifest_binding()
    print("HIL security regression tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
