#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests" / "bootstrap"
EXCLUDED_DIR = TESTS_DIR / "excluded"
DEFAULT_LOG_DIR = ROOT / ".baa_bootstrap_logs"

COMPILE_TIMEOUT_S = 30.0
RUN_TIMEOUT_S = 15.0

BANNED_WORDS = [
    "عشري",
    "عشري٣٢",
    "مجمع",
    "اتحاد",
    "اختر",
    "حالة",
    "افتراضي",
    "قائمة_معاملات",
    "بدء_معاملات",
    "معامل_تالي",
    "نهاية_معاملات",
    "وقت_حالي",
    "وقت_كنص",
    "عشوائي",
    "متغير_بيئة",
    "نفذ_أمر",
    "جذر_تربيعي",
    "جيب",
    "جيب_تمام",
    "ظل",
]

BANNED_PATTERNS = [
    ("function-pointer", re.compile(r"دالة\s*\(")),
    ("variadic-ellipsis", re.compile(r"\.\.\.")),
    ("multidim-array", re.compile(r"\]\s*\[")),
    ("float-literal", re.compile(r"(?<![\w\u0600-\u06FF])[\d٠-٩]+\.[\d٠-٩]+(?![\w\u0600-\u06FF])")),
]


@dataclass
class CheckResult:
    name: str
    passed: bool
    returncode: int
    duration_s: float
    detail: str


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


def _safe_name(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in ("_", "-", ".") else "_" for ch in value)


def _write_log(log_dir: Path, name: str, suffix: str, text: str) -> None:
    path = log_dir / f"{_safe_name(name)}.{suffix}.log"
    path.write_text(text, encoding="utf-8", errors="replace")


def _run(
    name: str,
    cmd: list[str],
    *,
    log_dir: Path,
    timeout_s: float,
) -> CheckResult:
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
        duration = time.monotonic() - started
        _write_log(log_dir, name, "stdout", proc.stdout or "")
        _write_log(log_dir, name, "stderr", proc.stderr or "")
        passed = proc.returncode == 0
        return CheckResult(
            name=name,
            passed=passed,
            returncode=int(proc.returncode),
            duration_s=duration,
            detail="ok" if passed else "non-zero exit",
        )
    except subprocess.TimeoutExpired as exc:
        duration = time.monotonic() - started
        _write_log(log_dir, name, "stdout", exc.stdout or "")
        _write_log(log_dir, name, "stderr", exc.stderr or "")
        return CheckResult(name, False, 124, duration, f"timeout ({timeout_s}s)")


def _print_result(result: CheckResult) -> None:
    state = "PASS" if result.passed else "FAIL"
    print(f"[{state}] {result.name} (rc={result.returncode}, {result.duration_s:.2f}s)")
    if not result.passed:
        print(f"       detail: {result.detail}")


def _strip_comments_and_literals(text: str) -> str:
    out: list[str] = []
    i = 0
    in_string = False
    in_char = False
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if in_string:
            if ch == "\\":
                out.append(" ")
                if i + 1 < len(text):
                    out.append(" ")
                    i += 2
                    continue
            elif ch == "\"":
                in_string = False
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if in_char:
            if ch == "\\":
                out.append(" ")
                if i + 1 < len(text):
                    out.append(" ")
                    i += 2
                    continue
            elif ch == "'":
                in_char = False
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if ch == "/" and nxt == "/":
            while i < len(text) and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if ch == "\"":
            in_string = True
            out.append(" ")
            i += 1
            continue
        if ch == "'":
            in_char = True
            out.append(" ")
            i += 1
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def _policy_violations(src: Path) -> list[str]:
    text = src.read_text(encoding="utf-8", errors="replace")
    stripped = _strip_comments_and_literals(text)
    violations: list[str] = []
    for word in BANNED_WORDS:
        pattern = re.compile(rf"(?<![\w\u0600-\u06FF]){re.escape(word)}(?![\w\u0600-\u06FF])")
        if pattern.search(stripped):
            violations.append(word)
    for name, pattern in BANNED_PATTERNS:
        if pattern.search(stripped):
            violations.append(name)
    return violations


def _run_markers(src: Path) -> set[str]:
    markers: set[str] = set()
    for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
        s = line.strip()
        if not s.startswith("// RUN:"):
            continue
        raw = s.split(":", 1)[1].strip()
        markers.update(part.strip().lower() for part in raw.replace(",", " ").split() if part.strip())
    if not markers:
        markers = {"expect-pass", "compile-only"}
    if "runtime" in markers:
        markers.discard("compile-only")
    return markers


def _positive_sources() -> list[Path]:
    return sorted(p for p in TESTS_DIR.glob("*.baa") if p.is_file())


def _excluded_sources() -> list[Path]:
    return sorted(p for p in EXCLUDED_DIR.glob("*.baa") if p.is_file())


def _policy_check_positive() -> list[CheckResult]:
    results: list[CheckResult] = []
    for src in _positive_sources():
        violations = _policy_violations(src)
        passed = not violations
        results.append(CheckResult(
            name=f"policy-positive:{src.name}",
            passed=passed,
            returncode=0 if passed else 1,
            duration_s=0.0,
            detail="ok" if passed else "banned Baa0 feature(s): " + ", ".join(violations),
        ))
    return results


def _policy_check_excluded() -> list[CheckResult]:
    results: list[CheckResult] = []
    for src in _excluded_sources():
        violations = _policy_violations(src)
        passed = bool(violations)
        results.append(CheckResult(
            name=f"policy-excluded:{src.name}",
            passed=passed,
            returncode=0 if passed else 1,
            duration_s=0.0,
            detail="ok" if passed else "fixture did not trigger Baa0 policy rejection",
        ))
    return results


def _compile_and_run_sources(baa: Path, log_dir: Path) -> list[CheckResult]:
    results: list[CheckResult] = []
    exe_ext = ".exe" if os.name == "nt" else ""
    out_dir = ROOT / f".baa_bootstrap_out_{os.getpid()}"
    out_dir.mkdir(parents=True, exist_ok=True)
    try:
        for src in _positive_sources():
            rel = src.relative_to(ROOT)
            markers = _run_markers(src)
            out = out_dir / f"{src.stem}{exe_ext}"
            compile_result = _run(
                f"bootstrap-compile:{src.name}",
                [str(baa), "-O2", "--verify-ir", "--verify-ssa", str(rel), "-o", str(out)],
                log_dir=log_dir,
                timeout_s=COMPILE_TIMEOUT_S,
            )
            results.append(compile_result)
            if not compile_result.passed or "runtime" not in markers:
                continue

            run_result = _run(
                f"bootstrap-run:{src.name}",
                [str(out)],
                log_dir=log_dir,
                timeout_s=RUN_TIMEOUT_S,
            )
            results.append(run_result)
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    return results


def _cross_target_assembly(baa: Path, log_dir: Path) -> list[CheckResult]:
    results: list[CheckResult] = []
    sources = _positive_sources()
    if not sources:
        return [CheckResult("cross-target-assembly", False, 1, 0.0, "no bootstrap sources found")]

    src = sources[0]
    rel = src.relative_to(ROOT)
    out_dir = ROOT / f".baa_bootstrap_asm_{os.getpid()}"
    out_dir.mkdir(parents=True, exist_ok=True)
    try:
        for target in ("x86_64-windows", "x86_64-linux"):
            asm_out = out_dir / f"{src.stem}_{target}.s"
            result = _run(
                f"bootstrap-asm:{target}",
                [str(baa), "-O2", "--verify-ir", "--verify-ssa", f"--target={target}", "-S", str(rel), "-o", str(asm_out)],
                log_dir=log_dir,
                timeout_s=COMPILE_TIMEOUT_S,
            )
            results.append(result)
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    return results


def _write_summary(path: str, baa: Path, results: list[CheckResult], log_dir: Path) -> None:
    if not path:
        return
    out = ROOT / path if not Path(path).is_absolute() else Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "gate": "bootstrap",
        "compiler": str(baa),
        "log_dir": str(log_dir.relative_to(ROOT) if log_dir.is_relative_to(ROOT) else log_dir),
        "passed": all(r.passed for r in results),
        "total": len(results),
        "failed": sum(1 for r in results if not r.passed),
        "results": [asdict(r) for r in results],
    }
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Baa0 bootstrap admission gate")
    parser.add_argument("--baa", default="")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR))
    parser.add_argument("--summary-json", default="")
    args = parser.parse_args()

    log_dir = Path(args.log_dir)
    if log_dir.exists():
        shutil.rmtree(log_dir, ignore_errors=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    baa = _find_baa(args.baa)
    print(f"bootstrap-gate: compiler={baa}")

    results: list[CheckResult] = []
    results.extend(_policy_check_positive())
    results.extend(_policy_check_excluded())
    results.extend(_compile_and_run_sources(baa, log_dir))
    results.extend(_cross_target_assembly(baa, log_dir))

    for result in results:
        _print_result(result)

    _write_summary(args.summary_json, baa, results, log_dir)
    return 0 if all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
