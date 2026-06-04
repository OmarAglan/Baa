#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT_DIR = ROOT / ".baa_phase45_handoff"
DEFAULT_TIMEOUT_S = 900.0

REQUIRED_ARTIFACTS = [
    "docs/COMPONENT_OWNERSHIP.md",
    "docs/BOOTSTRAP_CONTRACT.md",
    "docs/BAA0_SPEC.md",
    "docs/SELF_HOSTING_PILOT_REPORT.md",
    "scripts/qa_bootstrap_gate.py",
    "tests/bootstrap",
]


@dataclass
class CheckResult:
    name: str
    passed: bool
    returncode: int
    duration_s: float
    detail: str
    summary_path: str = ""


def _rel(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def _find_baa(arg: str) -> Path:
    if arg:
        p = Path(arg)
        if p.exists():
            return p

    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = (
        [ROOT / "build" / "baa.exe", ROOT / "build" / "baa"]
        if os.name == "nt"
        else [ROOT / "build-linux" / "baa", ROOT / "build" / "baa"]
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("Could not find compiler binary. Build it first, or pass --baa.")


def _run_capture(cmd: list[str], timeout_s: float = 30.0) -> dict[str, object]:
    started = time.monotonic()
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(ROOT),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=timeout_s,
        )
        return {
            "command": cmd,
            "returncode": int(proc.returncode),
            "stdout": (proc.stdout or "").strip(),
            "stderr": (proc.stderr or "").strip(),
            "duration_s": time.monotonic() - started,
        }
    except Exception as exc:
        return {
            "command": cmd,
            "returncode": 1,
            "stdout": "",
            "stderr": str(exc),
            "duration_s": time.monotonic() - started,
        }


def _run_logged(
    name: str,
    cmd: list[str],
    out_dir: Path,
    timeout_s: float = DEFAULT_TIMEOUT_S,
) -> CheckResult:
    started = time.monotonic()
    stdout_log = out_dir / f"{_safe_name(name)}.stdout.log"
    stderr_log = out_dir / f"{_safe_name(name)}.stderr.log"
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(ROOT),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=timeout_s,
        )
        stdout_log.write_text(proc.stdout or "", encoding="utf-8")
        stderr_log.write_text(proc.stderr or "", encoding="utf-8")
        passed = proc.returncode == 0
        return CheckResult(
            name=name,
            passed=passed,
            returncode=int(proc.returncode),
            duration_s=time.monotonic() - started,
            detail="ok" if passed else "non-zero exit",
        )
    except subprocess.TimeoutExpired as exc:
        stdout_log.write_text(exc.stdout or "", encoding="utf-8")
        stderr_log.write_text(exc.stderr or "", encoding="utf-8")
        return CheckResult(
            name=name,
            passed=False,
            returncode=124,
            duration_s=time.monotonic() - started,
            detail=f"timeout ({timeout_s}s)",
        )


def _safe_name(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in ("_", "-", ".") else "_" for ch in value)


def _load_json(path: Path) -> tuple[bool, dict[str, object] | None, str]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        return False, None, f"invalid JSON: {exc}"
    if not isinstance(data, dict):
        return False, None, "JSON root is not an object"
    return True, data, "ok"


def _check_required_artifacts() -> CheckResult:
    missing = [rel for rel in REQUIRED_ARTIFACTS if not (ROOT / rel).exists()]
    return CheckResult(
        name="required-artifacts",
        passed=not missing,
        returncode=0 if not missing else 1,
        duration_s=0.0,
        detail="ok" if not missing else "missing: " + ", ".join(missing),
    )


def _toolchain_snapshot(baa: Path) -> dict[str, object]:
    gcc = shutil.which("gcc")
    return {
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "python": sys.version.split()[0],
        },
        "commands": {
            "baa": _run_capture([str(baa), "--version"]),
            "python": _run_capture([sys.executable, "--version"]),
            "cmake": _run_capture(["cmake", "--version"]),
            "gcc": _run_capture([gcc or "gcc", "--version"]) if gcc else {"returncode": 1, "stderr": "gcc not found"},
            "git_head": _run_capture(["git", "rev-parse", "HEAD"]),
            "git_status": _run_capture(["git", "status", "--short"]),
        },
    }


def _emit_sample_manifest(baa: Path, out_dir: Path) -> CheckResult:
    manifest = out_dir / "sample-build-manifest.json"
    output = out_dir / ("phase45_manifest_sample.exe" if os.name == "nt" else "phase45_manifest_sample")
    result = _run_logged(
        "sample-build-manifest",
        [
            str(baa),
            "-O2",
            "--verify",
            "--emit-build-manifest",
            str(manifest),
            "tests/integration/backend/backend_array_init_test.baa",
            "-o",
            str(output),
        ],
        out_dir,
        timeout_s=60.0,
    )
    result.summary_path = _rel(manifest)
    if not result.passed:
        return result

    ok, data, detail = _load_json(manifest)
    if not ok:
        result.passed = False
        result.returncode = 1
        result.detail = detail
        return result

    required = {"compiler_version", "target", "mode", "opt_level", "runtime_checks", "units"}
    missing = sorted(key for key in required if key not in (data or {}))
    if missing:
        result.passed = False
        result.returncode = 1
        result.detail = "manifest missing keys: " + ", ".join(missing)
    return result


def _run_release_qa(out_dir: Path) -> CheckResult:
    summary = out_dir / "release-summary.json"
    log_dir = out_dir / "release-logs"
    result = _run_logged(
        "release-qa",
        [
            sys.executable,
            str(ROOT / "scripts" / "qa_run.py"),
            "--mode",
            "release",
            "--log-dir",
            str(log_dir),
            "--summary-json",
            str(summary),
        ],
        out_dir,
    )
    result.summary_path = _rel(summary)
    if not result.passed:
        return result
    ok, data, detail = _load_json(summary)
    if not ok:
        result.passed = False
        result.returncode = 1
        result.detail = detail
    elif not bool((data or {}).get("overall_passed", False)):
        result.passed = False
        result.returncode = 1
        result.detail = "release summary reports failure"
    return result


def _run_gate(name: str, script: str, baa: Path, out_dir: Path) -> CheckResult:
    summary = out_dir / f"{name}-summary.json"
    log_dir = out_dir / f"{name}-logs"
    result = _run_logged(
        name,
        [
            sys.executable,
            str(ROOT / "scripts" / script),
            "--baa",
            str(baa),
            "--log-dir",
            str(log_dir),
            "--summary-json",
            str(summary),
        ],
        out_dir,
        timeout_s=180.0,
    )
    result.summary_path = _rel(summary)
    if not result.passed:
        return result
    ok, data, detail = _load_json(summary)
    if not ok:
        result.passed = False
        result.returncode = 1
        result.detail = detail
    elif not bool((data or {}).get("passed", False)):
        result.passed = False
        result.returncode = 1
        result.detail = f"{name} summary reports failure"
    return result


def _platform_signoff(release_result: CheckResult, skip_release_qa: bool) -> dict[str, object]:
    host = platform.system().lower()
    windows_passed = host == "windows" and release_result.passed and not skip_release_qa
    linux_passed = host == "linux" and release_result.passed and not skip_release_qa
    return {
        "windows": {
            "quick_full_release_local": windows_passed,
            "status": "present" if windows_passed else "missing" if host != "windows" else "not-run",
        },
        "linux": {
            "quick_full_release_local": linux_passed,
            "status": "present" if linux_passed else "missing" if host != "linux" else "not-run",
        },
        "phase45_exit_ready": windows_passed and linux_passed,
    }


def _write_summary(
    out_dir: Path,
    baa: Path,
    skip_release_qa: bool,
    toolchain: dict[str, object],
    results: list[CheckResult],
    platform_signoff: dict[str, object],
) -> Path:
    passed = all(result.passed for result in results)
    summary = {
        "gate": "phase45-handoff",
        "compiler": str(baa),
        "handoff_bundle_ready": passed,
        "phase45_exit_ready": bool(platform_signoff.get("phase45_exit_ready", False)),
        "skip_release_qa": skip_release_qa,
        "out_dir": _rel(out_dir),
        "toolchain": toolchain,
        "platform_signoff": platform_signoff,
        "required_artifacts": REQUIRED_ARTIFACTS,
        "results": [asdict(result) for result in results],
    }
    out = out_dir / "handoff-summary.json"
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 4.5 bootstrap handoff bundle gate")
    parser.add_argument("--baa", default="")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    parser.add_argument("--skip-release-qa", action="store_true")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    out_dir = out_dir.resolve()
    if out_dir == ROOT or ROOT not in out_dir.parents:
        raise SystemExit("Refusing to write handoff bundle outside repository root.")
    if out_dir.exists():
        shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    baa = _find_baa(args.baa)
    toolchain = _toolchain_snapshot(baa)

    results: list[CheckResult] = []
    results.append(_check_required_artifacts())
    results.append(_emit_sample_manifest(baa, out_dir))

    if args.skip_release_qa:
        release_result = CheckResult("release-qa", True, 0, 0.0, "skipped by --skip-release-qa")
        results.append(_run_gate("bootstrap-gate", "qa_bootstrap_gate.py", baa, out_dir))
    else:
        release_result = _run_release_qa(out_dir)
        results.append(release_result)

    signoff = _platform_signoff(release_result, args.skip_release_qa)
    summary_path = _write_summary(out_dir, baa, args.skip_release_qa, toolchain, results, signoff)

    for result in results:
        state = "PASS" if result.passed else "FAIL"
        suffix = f" summary={result.summary_path}" if result.summary_path else ""
        print(f"[{state}] {result.name} (rc={result.returncode}, {result.duration_s:.2f}s) - {result.detail}{suffix}")
    print(f"phase45-handoff: summary={_rel(summary_path)}")
    print(f"phase45-handoff: handoff_bundle_ready={all(result.passed for result in results)}")
    print(f"phase45-handoff: phase45_exit_ready={signoff['phase45_exit_ready']}")

    return 0 if all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
