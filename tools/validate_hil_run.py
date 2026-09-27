#!/usr/bin/env python3
"""Fail closed unless a requested HIL source run is trusted firmware CI on main."""

from __future__ import annotations

import argparse
import json
import os
import re
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

TRUSTED_WORKFLOW = ".github/workflows/ci-firmware.yml"
TRUSTED_BRANCH = "main"
TRUSTED_EVENTS = {"push", "workflow_dispatch"}
SHA_RE = re.compile(r"^[0-9a-f]{40}$")


def validate_run_payload(payload: dict[str, Any], repository: str) -> dict[str, Any]:
    problems: list[str] = []

    if payload.get("status") != "completed":
        problems.append(f"run status is {payload.get('status')!r}, expected 'completed'")
    if payload.get("conclusion") != "success":
        problems.append(f"run conclusion is {payload.get('conclusion')!r}, expected 'success'")
    if payload.get("head_branch") != TRUSTED_BRANCH:
        problems.append(
            f"run branch is {payload.get('head_branch')!r}, only {TRUSTED_BRANCH!r} is trusted for HIL"
        )
    if payload.get("event") not in TRUSTED_EVENTS:
        problems.append(
            f"run event is {payload.get('event')!r}, pull-request and other untrusted events are rejected"
        )

    workflow_path = str(payload.get("path") or "").split("@", 1)[0]
    if workflow_path != TRUSTED_WORKFLOW:
        problems.append(f"unexpected workflow path: {workflow_path!r}")

    head_repository = payload.get("head_repository") or {}
    run_repository = payload.get("repository") or {}
    if head_repository.get("full_name") != repository:
        problems.append(
            f"head repository is {head_repository.get('full_name')!r}, expected {repository!r}"
        )
    if run_repository.get("full_name") != repository:
        problems.append(
            f"workflow repository is {run_repository.get('full_name')!r}, expected {repository!r}"
        )

    head_sha = str(payload.get("head_sha") or "").lower()
    if not SHA_RE.fullmatch(head_sha):
        problems.append(f"invalid head SHA: {payload.get('head_sha')!r}")

    run_id = payload.get("id")
    try:
        normalized_run_id = int(run_id)
    except (TypeError, ValueError):
        problems.append(f"invalid workflow run id: {run_id!r}")
        normalized_run_id = 0

    if problems:
        raise ValueError("; ".join(problems))

    return {
        "run_id": normalized_run_id,
        "head_sha": head_sha,
        "head_branch": payload["head_branch"],
        "event": payload["event"],
        "workflow_path": workflow_path,
        "repository": repository,
    }


def fetch_run(repository: str, run_id: str, token: str) -> dict[str, Any]:
    url = f"https://api.github.com/repos/{repository}/actions/runs/{run_id}"
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "asteroid-pilot-hil-validator",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise SystemExit(f"GitHub API rejected workflow run lookup ({exc.code}): {body}") from exc
    except urllib.error.URLError as exc:
        raise SystemExit(f"GitHub API workflow run lookup failed: {exc}") from exc


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY"))
    parser.add_argument("--output", default="hil-source-run.json")
    args = parser.parse_args()

    if not args.repository:
        raise SystemExit("repository is required via --repository or GITHUB_REPOSITORY")
    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        raise SystemExit("GITHUB_TOKEN is required to validate HIL firmware provenance")

    payload = fetch_run(args.repository, args.run_id, token)
    try:
        trusted = validate_run_payload(payload, args.repository)
    except ValueError as exc:
        raise SystemExit(f"Refusing untrusted firmware workflow run: {exc}") from exc

    Path(args.output).write_text(json.dumps(trusted, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        f"Trusted firmware run {trusted['run_id']} on {trusted['head_branch']} at {trusted['head_sha']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
