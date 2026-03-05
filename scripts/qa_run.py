#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import random
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, asdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"

DEFAULT_LOG_DIR = ROOT / ".baa_qa_logs"

CORPUS_COMPILE_TIMEOUT_S = 25.0
FUZZ_TIMEOUT_S = 8.0
MODULE_SIZE_TIMEOUT_S = 30.0


@dataclass
class StepResult:
    name: str
    passed: bool
    returncode: int
    duration_s: float
    stdout_log: str
    stderr_log: str
    detail: str


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    if os.name == "nt":
        candidates = [
            ROOT / "build" / "baa.exe",
            ROOT / "build" / "baa",
        ]
    else:
        candidates = [
            ROOT / "build-linux" / "baa",
            ROOT / "build" / "baa",
        ]

    for c in candidates:
        if c.exists():
            return c

    raise FileNotFoundError(
        "Could not find compiler binary. Build it first, or set BAA to the compiler path."
    )


def _safe_name(s: str) -> str:
    out = []
    for ch in s:
        if ch.isalnum() or ch in ("_", "-", "."):
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def _run_logged(
    name: str,
    cmd: list[str],
    *,
    cwd: Path,
    log_dir: Path,
    timeout_s: float | None = None,
    env: dict[str, str] | None = None,
) -> StepResult:
    step_key = _safe_name(name)
    stdout_log = log_dir / f"{step_key}.stdout.log"
    stderr_log = log_dir / f"{step_key}.stderr.log"

    started = time.monotonic()
    rc = 1
    detail = ""
    out_text = ""
    err_text = ""
    passed = False
    try:
        p = subprocess.run(
            cmd,
            cwd=str(cwd),
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=timeout_s,
        )
        rc = int(p.returncode)
        out_text = p.stdout or ""
        err_text = p.stderr or ""
        passed = rc == 0
        detail = "ok" if passed else "non-zero exit"
    except subprocess.TimeoutExpired as e:
        rc = 124
        out_text = e.stdout or ""
        err_text = e.stderr or ""
        passed = False
        detail = f"timeout ({timeout_s}s)"
    except Exception as e:  # pragma: no cover
        rc = 1
        passed = False
        detail = f"exception: {e}"
        err_text = f"{e}\n"

    duration = time.monotonic() - started
    stdout_log.write_text(out_text, encoding="utf-8", errors="replace")
    stderr_log.write_text(err_text, encoding="utf-8", errors="replace")

    return StepResult(
        name=name,
        passed=passed,
        returncode=rc,
        duration_s=duration,
        stdout_log=str(stdout_log.relative_to(ROOT)),
        stderr_log=str(stderr_log.relative_to(ROOT)),
        detail=detail,
    )


def _print_step(res: StepResult) -> None:
    state = "PASS" if res.passed else "FAIL"
    print(f"[{state}] {res.name} (rc={res.returncode}, {res.duration_s:.2f}s)")
    if not res.passed:
        print(f"       detail: {res.detail}")
        print(f"       stdout: {res.stdout_log}")
        print(f"       stderr: {res.stderr_log}")


def _run_verify_smoke(baa: Path, log_dir: Path) -> tuple[bool, list[StepResult]]:
    results: list[StepResult] = []
    ok = True
    exe_ext = ".exe" if os.name == "nt" else ""
    out_dir = ROOT / f".baa_verify_smoke_out_{os.getpid()}"
    out_dir.mkdir(parents=True, exist_ok=True)
    targets = [
        "tests/integration/backend/backend_test.baa",
        "tests/integration/backend/backend_static_storage_test.baa",
    ]
    try:
        for rel in targets:
            src = ROOT / rel
            out = out_dir / f"{src.stem}{exe_ext}"
            compile_res = _run_logged(
                f"verify-compile:{src.name}",
                [str(baa), "-O2", "--verify-ir", "--verify-ssa", rel, "-o", str(out)],
                cwd=ROOT,
                log_dir=log_dir,
                timeout_s=CORPUS_COMPILE_TIMEOUT_S,
            )
            _print_step(compile_res)
            results.append(compile_res)
            if not compile_res.passed:
                ok = False
                continue

            run_res = _run_logged(
                f"verify-run:{src.name}",
                [str(out)],
                cwd=ROOT,
                log_dir=log_dir,
                timeout_s=CORPUS_COMPILE_TIMEOUT_S,
            )
            _print_step(run_res)
            results.append(run_res)
            if not run_res.passed:
                ok = False
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    return ok, results


def _run_multifile_smoke(baa: Path, log_dir: Path) -> tuple[bool, list[StepResult]]:
    results: list[StepResult] = []
    exe_ext = ".exe" if os.name == "nt" else ""
    out_dir = ROOT / f".baa_multifile_smoke_out_{os.getpid()}"
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"multifile_counter{exe_ext}"
    try:
        compile_res = _run_logged(
            "multifile-compile",
            [
                str(baa),
                "-O2",
                "--verify",
                "tests/fixtures/multifile_counter_main.baa",
                "tests/fixtures/multifile_counter_lib.baa",
                "-o",
                str(out),
            ],
            cwd=ROOT,
            log_dir=log_dir,
            timeout_s=CORPUS_COMPILE_TIMEOUT_S,
        )
        _print_step(compile_res)
        results.append(compile_res)
        if not compile_res.passed:
            return False, results

        run_res = _run_logged(
            "multifile-run",
            [str(out)],
            cwd=ROOT,
            log_dir=log_dir,
            timeout_s=CORPUS_COMPILE_TIMEOUT_S,
        )
        _print_step(run_res)
        results.append(run_res)
        return run_res.passed, results
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)


def _run_stress_suite(baa: Path, log_dir: Path) -> tuple[bool, list[StepResult]]:
    results: list[StepResult] = []
    ok = True
    stress_dir = TESTS_DIR / "stress"
    stress_files = sorted(stress_dir.glob("*.baa"))
    if not stress_files:
        res = StepResult(
            name="stress-suite",
            passed=False,
            returncode=1,
            duration_s=0.0,
            stdout_log="",
            stderr_log="",
            detail="no tests/stress/*.baa files found",
        )
        _print_step(res)
        return False, [res]

    exe_ext = ".exe" if os.name == "nt" else ""
    out_dir = ROOT / f".baa_stress_out_{os.getpid()}"
    out_dir.mkdir(parents=True, exist_ok=True)
    try:
        for src in stress_files:
            rel = str(src.relative_to(ROOT))
            out = out_dir / f"{src.stem}{exe_ext}"
            compile_res = _run_logged(
                f"stress-compile:{src.name}",
                [str(baa), "-O2", "--verify", rel, "-o", str(out)],
                cwd=ROOT,
                log_dir=log_dir,
                timeout_s=CORPUS_COMPILE_TIMEOUT_S,
            )
            _print_step(compile_res)
            results.append(compile_res)
            if not compile_res.passed:
                ok = False
                continue

            run_res = _run_logged(
                f"stress-run:{src.name}",
                [str(out)],
                cwd=ROOT,
                log_dir=log_dir,
                timeout_s=CORPUS_COMPILE_TIMEOUT_S,
            )
            _print_step(run_res)
            results.append(run_res)
            if not run_res.passed:
                ok = False
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    return ok, results


def _build_fuzz_case(rng: random.Random, idx: int) -> str:
    valid_templates = [
        "صحيح الرئيسية() {\n    إرجع ٠.\n}\n",
        "صحيح ف(صحيح س) { إرجع س + ١. }\nصحيح الرئيسية() { إرجع ف(٣). }\n",
        "صحيح الرئيسية() {\n    صحيح س = ٠.\n    لكل (صحيح ع = ٠؛ ع < ٥؛ ع = ع + ١) { س = س + ع. }\n    إرجع ٠.\n}\n",
        "صحيح الرئيسية() {\n    نص رسالة = \"مرحبا\".\n    اطبع رسالة.\n    إرجع ٠.\n}\n",
    ]
    invalid_templates = [
        "صحيح الرئيسية() {\n    ثابت صحيح س.\n    إرجع ٠.\n}\n",
        "صحيح الرئيسية() {\n    نص س = \"مرحبا\\q\".\n    إرجع ٠.\n}\n",
        "صحيح الرئيسية() {\n    توقف.\n}\n",
        "ساكن صحيح دالة() { إرجع ٠. }\n",
    ]
    if rng.random() < 0.6:
        return valid_templates[idx % len(valid_templates)]
    return invalid_templates[idx % len(invalid_templates)]


def _run_fuzz_lite(baa: Path, log_dir: Path, cases: int, seed: int) -> tuple[bool, list[StepResult]]:
    results: list[StepResult] = []
    ok = True
    rng = random.Random(seed)
    out_dir = ROOT / f".baa_fuzz_out_{os.getpid()}"
    out_dir.mkdir(parents=True, exist_ok=True)
    try:
        for i in range(cases):
            src = out_dir / f"fuzz_{i:03d}.baa"
            asm = out_dir / f"fuzz_{i:03d}.s"
            src.write_text(_build_fuzz_case(rng, i), encoding="utf-8")
            rel_src = str(src.relative_to(ROOT))
            rel_asm = str(asm.relative_to(ROOT))
            res = _run_logged(
                f"fuzz:{src.name}",
                [str(baa), "-O0", "-S", rel_src, "-o", rel_asm],
                cwd=ROOT,
                log_dir=log_dir,
                timeout_s=FUZZ_TIMEOUT_S,
            )
            # Fuzz-lite policy: no hangs; exit codes 0/1 are acceptable.
            if res.returncode not in (0, 1):
                res.passed = False
                res.detail = f"unexpected exit code in fuzz-lite: {res.returncode}"
            else:
                res.passed = True
                res.detail = "ok"
            _print_step(res)
            results.append(res)
            if not res.passed:
                ok = False
                break
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    return ok, results


def _run_module_size_guard(log_dir: Path) -> StepResult:
    summary_rel = log_dir / "module-size-summary.json"
    res = _run_logged(
        "module-size-guard",
        [
            sys.executable,
            str(ROOT / "scripts" / "check_module_sizes.py"),
            "--json-out",
            str(summary_rel.relative_to(ROOT)),
        ],
        cwd=ROOT,
        log_dir=log_dir,
        timeout_s=MODULE_SIZE_TIMEOUT_S,
    )

    try:
        summary = json.loads(summary_rel.read_text(encoding="utf-8"))
        warning_count = int(summary.get("warning_count", 0))
        error_count = int(summary.get("error_count", 0))
        file_count = int(summary.get("file_count", 0))
        res.detail = f"{warning_count} warnings, {error_count} errors across {file_count} files"
    except Exception:
        pass

    return res


def _write_summary(
    mode: str,
    compiler: Path | None,
    overall_ok: bool,
    all_results: list[StepResult],
    log_dir: Path,
    summary_json: str,
) -> int:
    summary = {
        "mode": mode,
        "compiler": str(compiler) if compiler is not None else "",
        "overall_passed": overall_ok,
        "total_steps": len(all_results),
        "passed_steps": sum(1 for r in all_results if r.passed),
        "failed_steps": sum(1 for r in all_results if not r.passed),
        "results": [asdict(r) for r in all_results],
        "log_dir": str(log_dir.relative_to(ROOT)),
    }

    if summary_json:
        out = Path(summary_json)
        if not out.is_absolute():
            out = ROOT / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    print("qa: SUMMARY " + json.dumps({
        "mode": summary["mode"],
        "overall_passed": summary["overall_passed"],
        "total_steps": summary["total_steps"],
        "passed_steps": summary["passed_steps"],
        "failed_steps": summary["failed_steps"],
        "log_dir": summary["log_dir"],
    }, ensure_ascii=False))

    return 0 if overall_ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Baa QA orchestrator")
    parser.add_argument("--mode", choices=["quick", "full", "stress"], default="quick")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR))
    parser.add_argument("--summary-json", default="")
    parser.add_argument("--fuzz-cases", type=int, default=24)
    parser.add_argument("--seed", type=int, default=1337)
    args = parser.parse_args()

    log_dir = Path(args.log_dir)
    if log_dir.exists():
        shutil.rmtree(log_dir, ignore_errors=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    baa: Path | None = None
    all_results: list[StepResult] = []
    overall_ok = True

    print(f"qa: mode={args.mode}")

    if args.mode in ("full", "stress"):
        module_size_res = _run_module_size_guard(log_dir)
        _print_step(module_size_res)
        all_results.append(module_size_res)
        overall_ok = overall_ok and module_size_res.passed
        if not module_size_res.passed:
            return _write_summary(args.mode, baa, overall_ok, all_results, log_dir, args.summary_json)

    baa = _find_baa()
    print(f"qa: compiler={baa}")

    # A) Integration tiers
    test_res = _run_logged(
        "tests/test.py",
        [sys.executable, str(TESTS_DIR / "test.py")],
        cwd=ROOT,
        log_dir=log_dir,
        timeout_s=300.0,
    )
    _print_step(test_res)
    all_results.append(test_res)
    overall_ok = overall_ok and test_res.passed

    if args.mode in ("full", "stress"):
        regress_res = _run_logged(
            "tests/regress.py",
            [sys.executable, str(TESTS_DIR / "regress.py")],
            cwd=ROOT,
            log_dir=log_dir,
            timeout_s=600.0,
        )
        _print_step(regress_res)
        all_results.append(regress_res)
        overall_ok = overall_ok and regress_res.passed

        smoke_ok, smoke_results = _run_verify_smoke(baa, log_dir)
        all_results.extend(smoke_results)
        overall_ok = overall_ok and smoke_ok

        mf_ok, mf_results = _run_multifile_smoke(baa, log_dir)
        all_results.extend(mf_results)
        overall_ok = overall_ok and mf_ok

    if args.mode == "stress":
        stress_ok, stress_results = _run_stress_suite(baa, log_dir)
        all_results.extend(stress_results)
        overall_ok = overall_ok and stress_ok

        fuzz_ok, fuzz_results = _run_fuzz_lite(baa, log_dir, args.fuzz_cases, args.seed)
        all_results.extend(fuzz_results)
        overall_ok = overall_ok and fuzz_ok

    return _write_summary(args.mode, baa, overall_ok, all_results, log_dir, args.summary_json)


if __name__ == "__main__":
    raise SystemExit(main())
