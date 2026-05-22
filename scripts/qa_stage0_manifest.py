#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "docs" / "STAGE0_TOOLCHAIN_MANIFEST.json"

SCHEMA = 1
METADATA_VERSION = "0.9.0.1"
STAGE0_COMPILER_VERSION = "0.5.9"
STAGE0_TAG = "bootstrap-stage0-v0.9.0.1"
STAGE0_COMMIT = "619619ab04d30cb30bde5fc7d726814bf3c4c933"
BASE_RELEASE_TAG = "0.5.9"
BASE_RELEASE_COMMIT = "c0ec754fd410f01d60fd663d8de1073b6c5a7d92"

FROZEN_ARTIFACTS = [
    "docs/COMPONENT_OWNERSHIP.md",
    "docs/BOOTSTRAP_CONTRACT.md",
    "docs/BAA0_SPEC.md",
    "docs/SELF_HOSTING_PILOT_REPORT.md",
    "docs/PHASE_4_5_HANDOFF.md",
    "scripts/qa_bootstrap_gate.py",
    "scripts/qa_selfhost_pilot.py",
    "scripts/qa_phase45_handoff.py",
    "tests/bootstrap",
]


def _git(args: list[str]) -> str:
    proc = subprocess.run(
        ["git", *args],
        cwd=str(ROOT),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout or "").strip()
        raise RuntimeError(f"git {' '.join(args)} failed: {detail}")
    return proc.stdout.strip()


def _resolve_commit(ref: str) -> str:
    return _git(["rev-parse", "--verify", f"{ref}^{{}}^{{commit}}"])


def _object_id(ref: str, path: str) -> str:
    return _git(["rev-parse", "--verify", f"{ref}:{path}"])


def _object_type(object_id: str) -> str:
    return _git(["cat-file", "-t", object_id])


def _build_manifest() -> dict[str, Any]:
    artifacts = []
    for path in FROZEN_ARTIFACTS:
        object_id = _object_id(STAGE0_TAG, path)
        artifacts.append(
            {
                "path": path,
                "object_type": _object_type(object_id),
                "git_object_id": object_id,
            }
        )

    return {
        "schema": SCHEMA,
        "metadata_version": METADATA_VERSION,
        "stage0": {
            "tag": STAGE0_TAG,
            "commit": STAGE0_COMMIT,
            "compiler_version": STAGE0_COMPILER_VERSION,
        },
        "base_release": {
            "tag": BASE_RELEASE_TAG,
            "commit": BASE_RELEASE_COMMIT,
        },
        "signoff": {
            "windows": "present",
            "linux": "deferred",
            "phase45_exit_ready": False,
        },
        "frozen_artifacts": artifacts,
    }


def _load_manifest() -> dict[str, Any]:
    try:
        data = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    except Exception as exc:
        raise RuntimeError(f"failed to read {MANIFEST_PATH}: {exc}") from exc
    if not isinstance(data, dict):
        raise RuntimeError("manifest root must be a JSON object")
    return data


def _require(value: bool, message: str, errors: list[str]) -> None:
    if not value:
        errors.append(message)


def _validate_manifest(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    _require(data.get("schema") == SCHEMA, "schema mismatch", errors)
    _require(data.get("metadata_version") == METADATA_VERSION, "metadata_version mismatch", errors)

    stage0 = data.get("stage0")
    _require(isinstance(stage0, dict), "stage0 must be an object", errors)
    if isinstance(stage0, dict):
        _require(stage0.get("tag") == STAGE0_TAG, "stage0 tag mismatch", errors)
        _require(stage0.get("commit") == STAGE0_COMMIT, "stage0 commit mismatch", errors)
        _require(
            stage0.get("compiler_version") == STAGE0_COMPILER_VERSION,
            "stage0 compiler_version mismatch",
            errors,
        )

    base = data.get("base_release")
    _require(isinstance(base, dict), "base_release must be an object", errors)
    if isinstance(base, dict):
        _require(base.get("tag") == BASE_RELEASE_TAG, "base_release tag mismatch", errors)
        _require(base.get("commit") == BASE_RELEASE_COMMIT, "base_release commit mismatch", errors)

    signoff = data.get("signoff")
    _require(isinstance(signoff, dict), "signoff must be an object", errors)
    if isinstance(signoff, dict):
        _require(signoff.get("windows") == "present", "windows signoff mismatch", errors)
        _require(signoff.get("linux") == "deferred", "linux signoff mismatch", errors)
        _require(signoff.get("phase45_exit_ready") is False, "phase45_exit_ready must be false", errors)

    try:
        actual_stage0 = _resolve_commit(STAGE0_TAG)
        _require(actual_stage0 == STAGE0_COMMIT, f"{STAGE0_TAG} resolves to {actual_stage0}", errors)
    except RuntimeError as exc:
        errors.append(str(exc))

    try:
        actual_base = _resolve_commit(BASE_RELEASE_TAG)
        _require(actual_base == BASE_RELEASE_COMMIT, f"{BASE_RELEASE_TAG} resolves to {actual_base}", errors)
    except RuntimeError as exc:
        errors.append(str(exc))

    artifacts = data.get("frozen_artifacts")
    _require(isinstance(artifacts, list), "frozen_artifacts must be an array", errors)
    actual_by_path: dict[str, dict[str, Any]] = {}
    if isinstance(artifacts, list):
        for item in artifacts:
            if isinstance(item, dict) and isinstance(item.get("path"), str):
                actual_by_path[item["path"]] = item

    for path in FROZEN_ARTIFACTS:
        item = actual_by_path.get(path)
        if not item:
            errors.append(f"missing frozen artifact: {path}")
            continue
        try:
            object_id = _object_id(STAGE0_TAG, path)
            object_type = _object_type(object_id)
        except RuntimeError as exc:
            errors.append(str(exc))
            continue
        _require(item.get("git_object_id") == object_id, f"{path} object id mismatch", errors)
        _require(item.get("object_type") == object_type, f"{path} object type mismatch", errors)

    extra = sorted(set(actual_by_path) - set(FROZEN_ARTIFACTS))
    for path in extra:
        errors.append(f"unexpected frozen artifact: {path}")

    return errors


def _write_manifest() -> None:
    data = _build_manifest()
    MANIFEST_PATH.write_text(
        json.dumps(data, ensure_ascii=False, indent=2, sort_keys=False) + "\n",
        encoding="utf-8",
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Validate the v0.9.0.1 Stage-0 manifest")
    parser.add_argument("--write", action="store_true", help="Regenerate the manifest deterministically")
    args = parser.parse_args(argv)

    try:
        if args.write:
            _write_manifest()
        errors = _validate_manifest(_load_manifest())
    except RuntimeError as exc:
        print(f"stage0-manifest: FAIL: {exc}")
        return 1

    if errors:
        for error in errors:
            print(f"stage0-manifest: FAIL: {error}")
        return 1

    print(f"stage0-manifest: PASS {MANIFEST_PATH.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
