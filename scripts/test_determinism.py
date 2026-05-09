#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TIMEOUT_S = 30.0


@dataclass
class CheckResult:
    name: str
    passed: bool
    detail: str
    duration_s: float


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = []
    if os.name == "nt":
        candidates.extend([ROOT / "build" / "baa.exe", ROOT / "build" / "baa"])
    candidates.extend([ROOT / "build-linux" / "baa", ROOT / "build" / "baa"])

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("Could not find compiler binary. Build it first or set BAA.")


def _run(cmd: list[str], timeout_s: float = DEFAULT_TIMEOUT_S) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(ROOT),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=timeout_s,
    )


def _normalize_text(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def _result(name: str, started: float, passed: bool, detail: str) -> CheckResult:
    return CheckResult(name=name, passed=passed, detail=detail, duration_s=time.monotonic() - started)


def _expect_success(name: str, process: subprocess.CompletedProcess[str]) -> str | None:
    if process.returncode == 0:
        return None
    return (
        f"{name} failed with exit {process.returncode}\n"
        f"stdout:\n{process.stdout}\n"
        f"stderr:\n{process.stderr}"
    )


def _compare_stdout(name: str, cmd: list[str]) -> CheckResult:
    started = time.monotonic()
    first = _run(cmd)
    err = _expect_success(f"{name}: first run", first)
    if err:
        return _result(name, started, False, err)

    second = _run(cmd)
    err = _expect_success(f"{name}: second run", second)
    if err:
        return _result(name, started, False, err)

    first_out = _normalize_text(first.stdout)
    second_out = _normalize_text(second.stdout)
    if first_out != second_out:
        return _result(name, started, False, "stdout differed between identical runs")
    return _result(name, started, True, "stable stdout")


def _compare_output_file(name: str, cmd: list[str], out_file: Path) -> CheckResult:
    started = time.monotonic()
    out_file.unlink(missing_ok=True)
    first = _run(cmd)
    err = _expect_success(f"{name}: first run", first)
    if err:
        return _result(name, started, False, err)
    try:
        first_bytes = out_file.read_bytes()
    except OSError as exc:
        return _result(name, started, False, f"could not read first output: {exc}")

    out_file.unlink(missing_ok=True)
    second = _run(cmd)
    err = _expect_success(f"{name}: second run", second)
    if err:
        return _result(name, started, False, err)
    try:
        second_bytes = out_file.read_bytes()
    except OSError as exc:
        return _result(name, started, False, f"could not read second output: {exc}")

    if first_bytes != second_bytes:
        return _result(name, started, False, "output file bytes differed between identical runs")
    return _result(name, started, True, "stable output file")


def _check_manifest(baa: Path, out_dir: Path) -> CheckResult:
    manifest = out_dir / "manifest.json"
    output = out_dir / ("determinism_app.exe" if os.name == "nt" else "determinism_app")
    cmd = [
        str(baa),
        "-O2",
        "--verify",
        "--emit-build-manifest",
        str(manifest),
        "tests/integration/backend/backend_array_init_test.baa",
        "-o",
        str(output),
    ]
    result = _compare_output_file("manifest-byte-stability", cmd, manifest)
    if not result.passed:
        return result

    started = time.monotonic()
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
    except Exception as exc:
        return _result("manifest-shape", started, False, f"manifest is not valid JSON: {exc}")

    required = {"compiler_version", "target", "mode", "opt_level", "runtime_checks", "units"}
    missing = sorted(k for k in required if k not in data)
    if missing:
        return _result("manifest-shape", started, False, f"manifest missing keys: {missing}")
    return _result("manifest-shape", started, True, "manifest has required deterministic keys")


def _check_cross_target_assembly(baa: Path, out_dir: Path) -> list[CheckResult]:
    results: list[CheckResult] = []
    src = "tests/integration/ir/ir_test.baa"
    targets = [
        ("x86_64-windows", out_dir / "cross_windows.s", [".section .rdata", ".globl main"]),
        ("x86_64-linux", out_dir / "cross_linux.s", [".section .rodata", ".globl main"]),
    ]
    for target, output, markers in targets:
        cmd = [str(baa), "-O2", "--verify", "-S", f"--target={target}", src, "-o", str(output)]
        result = _compare_output_file(f"cross-target-asm:{target}", cmd, output)
        results.append(result)
        if not result.passed:
            continue
        started = time.monotonic()
        try:
            asm = output.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            results.append(_result(f"cross-target-markers:{target}", started, False, str(exc)))
            continue
        missing = [marker for marker in markers if marker not in asm]
        if missing:
            results.append(
                _result(f"cross-target-markers:{target}", started, False, f"missing {missing}")
            )
        else:
            results.append(_result(f"cross-target-markers:{target}", started, True, "markers found"))
    return results


def _check_verifier_gate(baa: Path, out_dir: Path) -> CheckResult:
    started = time.monotonic()
    output = out_dir / ("verify_gate.exe" if os.name == "nt" else "verify_gate")
    p = _run([
        str(baa),
        "-O2",
        "--verify-gate",
        "tests/integration/ir/ir_test.baa",
        "-o",
        str(output),
    ])
    err = _expect_success("verify-gate", p)
    if err:
        return _result("verify-gate", started, False, err)
    return _result("verify-gate", started, True, "verify gate accepted stable IR corpus")


def _sha256_text(text: str) -> str:
    return hashlib.sha256(_normalize_text(text).encode("utf-8")).hexdigest()


def _check_ir_snapshots(baa: Path) -> list[CheckResult]:
    started = time.monotonic()
    snapshot_file = ROOT / "tests" / "snapshots" / "ir_hashes.json"
    try:
        snapshots = json.loads(snapshot_file.read_text(encoding="utf-8"))
    except Exception as exc:
        return [_result("ir-snapshot-load", started, False, f"could not load snapshots: {exc}")]

    results: list[CheckResult] = []
    for rel, expected in sorted(snapshots.items()):
        ir = _run([str(baa), "--dump-ir", rel])
        err = _expect_success(f"ir-snapshot:{rel}: dump-ir", ir)
        if err:
            results.append(_result(f"ir-snapshot:{rel}:dump-ir", started, False, err))
            continue
        actual_ir = _sha256_text(ir.stdout)
        expected_ir = str(expected.get("dump_ir_sha256", ""))
        results.append(
            _result(
                f"ir-snapshot:{rel}:dump-ir",
                started,
                actual_ir == expected_ir,
                "snapshot matched" if actual_ir == expected_ir else f"expected {expected_ir}, got {actual_ir}",
            )
        )

        opt = _run([str(baa), "-O2", "--dump-ir-opt", rel])
        err = _expect_success(f"ir-snapshot:{rel}: dump-ir-opt", opt)
        if err:
            results.append(_result(f"ir-snapshot:{rel}:dump-ir-opt", started, False, err))
            continue
        actual_opt = _sha256_text(opt.stdout)
        expected_opt = str(expected.get("dump_ir_opt_o2_sha256", ""))
        results.append(
            _result(
                f"ir-snapshot:{rel}:dump-ir-opt",
                started,
                actual_opt == expected_opt,
                "snapshot matched" if actual_opt == expected_opt else f"expected {expected_opt}, got {actual_opt}",
            )
        )
    return results


def run_checks(baa: Path, out_dir: Path) -> list[CheckResult]:
    ir_src = "tests/integration/ir/ir_test.baa"
    results = [
        _compare_stdout("dump-ir-stability", [str(baa), "--dump-ir", ir_src]),
        _compare_stdout("dump-ir-opt-stability", [str(baa), "-O2", "--dump-ir-opt", ir_src]),
    ]

    asm_output = out_dir / "stable_ir_test.s"
    results.append(
        _compare_output_file(
            "assembly-byte-stability",
            [str(baa), "-O2", "--verify", "-S", ir_src, "-o", str(asm_output)],
            asm_output,
        )
    )
    results.extend(_check_cross_target_assembly(baa, out_dir))
    results.append(_check_manifest(baa, out_dir))
    results.append(_check_verifier_gate(baa, out_dir))
    results.extend(_check_ir_snapshots(baa))
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description="Baa determinism and release-gate checks")
    parser.add_argument("--summary-json", default="")
    parser.add_argument("--keep-output", action="store_true")
    args = parser.parse_args()

    baa = _find_baa()
    out_dir = ROOT / f".baa_determinism_out_{os.getpid()}"
    if out_dir.exists():
        shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    try:
        results = run_checks(baa, out_dir)
    finally:
        if not args.keep_output:
            shutil.rmtree(out_dir, ignore_errors=True)

    ok = all(result.passed for result in results)
    for result in results:
        state = "PASS" if result.passed else "FAIL"
        print(f"[{state}] {result.name} ({result.duration_s:.2f}s) - {result.detail}")

    summary = {
        "overall_passed": ok,
        "compiler": str(baa),
        "total_steps": len(results),
        "passed_steps": sum(1 for result in results if result.passed),
        "failed_steps": sum(1 for result in results if not result.passed),
        "results": [asdict(result) for result in results],
    }

    if args.summary_json:
        out = Path(args.summary_json)
        if not out.is_absolute():
            out = ROOT / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    print("determinism: SUMMARY " + json.dumps({
        "overall_passed": summary["overall_passed"],
        "total_steps": summary["total_steps"],
        "passed_steps": summary["passed_steps"],
        "failed_steps": summary["failed_steps"],
    }, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
