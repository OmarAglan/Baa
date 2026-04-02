#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PRESETS_FILE = ROOT / "CMakePresets.json"
DEFAULT_IGNORE = shutil.ignore_patterns(
    ".git",
    ".codex",
    "__pycache__",
    ".baa_qa_logs",
    "build",
    "build-*",
    "build-linux",
    "build-linux-*",
)
PROBE_HEADER = Path("src/support/version.h")
PROBE_MARKER = "/* فحص البناء التزايدي */"
EXPECTED_OBJECT_DIRS = ("src/driver/", "src/support/")


@dataclass
class CommandResult:
    cmd: list[str]
    returncode: int
    stdout: str
    stderr: str


def _default_preset() -> str:
    return "windows-release" if os.name == "nt" else "linux-release"


def _load_presets(root: Path) -> dict[str, object]:
    return json.loads((root / "CMakePresets.json").read_text(encoding="utf-8"))


def _resolve_binary_dir(root: Path, preset_name: str) -> Path:
    presets = _load_presets(root)
    for preset in presets.get("configurePresets", []):
        if preset.get("name") != preset_name:
            continue
        binary_dir = str(preset.get("binaryDir", ""))
        if not binary_dir:
            break
        binary_dir = binary_dir.replace("${sourceDir}", str(root))
        return Path(binary_dir)
    raise KeyError(f"configure preset not found: {preset_name}")


def _run(cmd: list[str], *, cwd: Path) -> CommandResult:
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
    )
    return CommandResult(
        cmd=cmd,
        returncode=int(proc.returncode),
        stdout=proc.stdout or "",
        stderr=proc.stderr or "",
    )


def _require_ok(result: CommandResult, stage: str) -> None:
    if result.returncode == 0:
        return
    raise RuntimeError(
        f"{stage} failed (rc={result.returncode})\n"
        f"CMD: {' '.join(result.cmd)}\n"
        f"STDOUT:\n{result.stdout}\n"
        f"STDERR:\n{result.stderr}"
    )


def _collect_object_mtimes(build_dir: Path) -> dict[str, int]:
    mtimes: dict[str, int] = {}
    for path in sorted(build_dir.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".o", ".obj"}:
            continue
        rel = str(path.relative_to(build_dir)).replace("\\", "/")
        mtimes[rel] = path.stat().st_mtime_ns
    return mtimes


def _changed_objects(before: dict[str, int], after: dict[str, int]) -> list[str]:
    changed: list[str] = []
    for rel, mtime in sorted(after.items()):
        if before.get(rel) != mtime:
            changed.append(rel)
    return changed


def _copy_repo(temp_root: Path) -> Path:
    worktree = temp_root / "repo"
    shutil.copytree(ROOT, worktree, ignore=DEFAULT_IGNORE)
    return worktree


def _touch_probe_header(worktree: Path) -> None:
    header_path = worktree / PROBE_HEADER
    text = header_path.read_text(encoding="utf-8")
    if PROBE_MARKER not in text:
        text = text.rstrip() + "\n" + PROBE_MARKER + "\n"
    else:
        text = text + "\n"
    header_path.write_text(text, encoding="utf-8")


def _validate_subset(total_objects: int, changed_objects: list[str]) -> None:
    if total_objects <= 0:
        raise RuntimeError("no object files were produced by the preset build")
    if not changed_objects:
        raise RuntimeError("header touch did not trigger any object rebuilds")
    if len(changed_objects) >= total_objects:
        raise RuntimeError("header touch rebuilt every object; invalidation is too broad")

    unexpected = [
        rel for rel in changed_objects
        if not any(expected in rel for expected in EXPECTED_OBJECT_DIRS)
    ]
    if unexpected:
        joined = ", ".join(unexpected)
        raise RuntimeError(f"unexpected object rebuilds after narrow header touch: {joined}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate persistent incremental rebuild behavior for a CMake preset."
    )
    parser.add_argument("--preset", default=_default_preset())
    parser.add_argument("--json-out", default="")
    args = parser.parse_args()

    summary: dict[str, object] = {
        "preset": args.preset,
        "probe_header": str(PROBE_HEADER).replace("\\", "/"),
        "passed": False,
        "changed_after_noop": [],
        "changed_after_probe": [],
    }

    try:
        with tempfile.TemporaryDirectory(prefix="baa_incremental_") as temp_dir:
            temp_root = Path(temp_dir)
            worktree = _copy_repo(temp_root)
            build_dir = _resolve_binary_dir(worktree, args.preset)

            configure_res = _run(["cmake", "--preset", args.preset], cwd=worktree)
            _require_ok(configure_res, "configure")

            first_build = _run(["cmake", "--build", "--preset", args.preset], cwd=worktree)
            _require_ok(first_build, "initial build")
            first_mtimes = _collect_object_mtimes(build_dir)

            second_build = _run(["cmake", "--build", "--preset", args.preset], cwd=worktree)
            _require_ok(second_build, "no-op rebuild")
            second_mtimes = _collect_object_mtimes(build_dir)
            changed_after_noop = _changed_objects(first_mtimes, second_mtimes)
            if changed_after_noop:
                raise RuntimeError(
                    "no-op rebuild changed object timestamps: " + ", ".join(changed_after_noop)
                )

            _touch_probe_header(worktree)
            third_build = _run(["cmake", "--build", "--preset", args.preset], cwd=worktree)
            _require_ok(third_build, "header-triggered rebuild")
            third_mtimes = _collect_object_mtimes(build_dir)
            changed_after_probe = _changed_objects(second_mtimes, third_mtimes)
            _validate_subset(len(third_mtimes), changed_after_probe)

            summary.update(
                {
                    "passed": True,
                    "build_dir": str(build_dir.relative_to(worktree)).replace("\\", "/"),
                    "object_count": len(third_mtimes),
                    "changed_after_noop": changed_after_noop,
                    "changed_after_probe": changed_after_probe,
                }
            )
            print(
                "incremental-build: PASS "
                f"(preset={args.preset}, object_count={len(third_mtimes)}, "
                f"recompiled={len(changed_after_probe)})"
            )
    except Exception as exc:
        summary["error"] = str(exc)
        print(f"incremental-build: FAIL ({exc})", file=sys.stderr)

    if args.json_out:
        out_path = Path(args.json_out)
        if not out_path.is_absolute():
            out_path = ROOT / out_path
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    return 0 if summary.get("passed") else 1


if __name__ == "__main__":
    raise SystemExit(main())
