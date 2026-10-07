#!/usr/bin/env python3
"""Differential optimization gate (v0.7.3).

Every runtime program is compiled at ``-O0`` (the baseline) and at ``-O1``,
``-O2`` and ``-O2 -funroll-loops``. Every build must succeed, and each
optimized executable must reproduce the baseline's exit status, stdout and
stderr exactly. An optimization that changes observable behavior fails here
even when the program still returns 0.

Corpus:

- ``tests/differential/*.baa``: programs written for this gate. They print many
  intermediate values, so the baseline must exit 0 with non-empty stdout.
- ``tests/integration/backend/*.baa`` runtime tests, honoring their ``FLAGS``,
  ``ARGS`` and ``STDIN`` markers. Tests with ``EXPECT-ASM`` markers or
  ``expect-fail`` are compile-only contracts and are not part of this corpus.

A program whose behavior legitimately depends on the optimization level opts
out with ``// DIFF-SKIP: <reason>``; the reason is required.

``BAA_DIFF_JOBS`` sets how many programs are checked concurrently (default 2).
The levels of one program always run one after another.
"""

from __future__ import annotations

import importlib.util
import os
import subprocess
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
DIFFERENTIAL_DIR = TESTS_DIR / "differential"
BACKEND_DIR = TESTS_DIR / "integration" / "backend"

BASELINE = ("-O0",)
VARIANTS = (("-O1",), ("-O2",), ("-O2", "-funroll-loops"))

COMPILE_TIMEOUT_S = 60
RUN_TIMEOUT_S = 30
DIFF_SKIP_MARKER = "// DIFF-SKIP:"


def _load_markers_module():
    # tests/test.py owns the marker grammar; load it under a private name
    # because "test" collides with the standard library package.
    spec = importlib.util.spec_from_file_location("baa_integration_markers", TESTS_DIR / "test.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


MARKERS = _load_markers_module()


def _diff_skip_reason(src: Path) -> str | None:
    for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
        s = line.strip()
        if s.startswith(DIFF_SKIP_MARKER):
            return s[len(DIFF_SKIP_MARKER):].strip()
    return None


def _is_backend_runtime_case(src: Path) -> bool:
    markers = MARKERS._resolve_run_markers(src)
    if "skip" in markers or "expect-fail" in markers or "runtime" not in markers:
        return False
    if MARKERS._expect_asm_markers(src) or MARKERS._expect_not_asm_markers(src):
        return False
    return True


def collect_corpus() -> tuple[list[Path], list[tuple[Path, str]]]:
    """Return (programs to check, programs skipped with their reasons)."""
    programs: list[Path] = []
    skipped: list[tuple[Path, str]] = []
    candidates = sorted(DIFFERENTIAL_DIR.glob("*.baa"))
    candidates += [p for p in sorted(BACKEND_DIR.glob("*.baa")) if _is_backend_runtime_case(p)]
    for src in candidates:
        reason = _diff_skip_reason(src)
        if reason is None:
            programs.append(src)
        else:
            skipped.append((src, reason))
    return programs, skipped


def _strip_opt_flags(flags: list[str]) -> list[str]:
    return [f for f in flags if not (f.startswith("-O") or f == "-funroll-loops")]


def _observe(baa: Path, src: Path, level: tuple[str, ...], out_dir: Path) -> tuple:
    """Compile and run one program at one level; return what a user can observe."""
    exe_ext = ".exe" if os.name == "nt" else ""
    tag = "_".join(flag.lstrip("-").replace("-", "") for flag in level)
    out = out_dir / f"{src.stem}.{tag}{exe_ext}"
    flags = _strip_opt_flags(MARKERS._flags_markers(src))
    # Repo-relative source paths avoid space-splitting in toolchain command lines.
    compile_proc = subprocess.run(
        [str(baa), *level, *flags, str(src.relative_to(ROOT)), "-o", str(out)],
        cwd=str(ROOT),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=COMPILE_TIMEOUT_S,
    )
    if compile_proc.returncode != 0 or not out.exists():
        return ("compile-failed", compile_proc.returncode, compile_proc.stderr.strip())

    try:
        run_proc = subprocess.run(
            [str(out), *MARKERS._args_markers(src)],
            cwd=str(ROOT),
            input=MARKERS._stdin_markers(src),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=RUN_TIMEOUT_S,
        )
    except subprocess.TimeoutExpired:
        return ("timeout", RUN_TIMEOUT_S)
    return ("ran", run_proc.returncode, run_proc.stdout, run_proc.stderr)


def _first_difference(a: str, b: str) -> str:
    a_lines, b_lines = a.splitlines(), b.splitlines()
    for i, (x, y) in enumerate(zip(a_lines, b_lines), start=1):
        if x != y:
            return f"line {i}: -O0 {x!r} vs {y!r}"
    return f"line count: -O0 {len(a_lines)} vs {len(b_lines)}"


def compare_program(baa: Path, src: Path, out_dir: Path) -> list[str]:
    """Return human-readable failures for one program (empty when it passes)."""
    rel = src.relative_to(ROOT).as_posix()
    base = _observe(baa, src, BASELINE, out_dir)
    if base[0] != "ran":
        return [f"{rel} -O0: {base}"]

    failures: list[str] = []
    if src.parent == DIFFERENTIAL_DIR:
        if base[1] != 0:
            failures.append(f"{rel} -O0: differential program exited {base[1]}: {base[3][-300:]}")
        if not base[2].strip():
            failures.append(f"{rel} -O0: differential program printed nothing")

    for level in VARIANTS:
        name = " ".join(level)
        got = _observe(baa, src, level, out_dir)
        if got[0] != "ran":
            failures.append(f"{rel} {name}: {got}")
            continue
        if got[1] != base[1]:
            failures.append(f"{rel} {name}: exit {got[1]} but -O0 exit {base[1]}")
        if got[2] != base[2]:
            failures.append(f"{rel} {name}: stdout differs, {_first_difference(base[2], got[2])}")
        if got[3] != base[3]:
            failures.append(f"{rel} {name}: stderr differs, {_first_difference(base[3], got[3])}")
    return failures


class OptimizationDifferentialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = MARKERS._find_baa()
        cls.programs, cls.skipped = collect_corpus()

    def test_corpus_is_present(self) -> None:
        own = [p for p in self.programs if p.parent == DIFFERENTIAL_DIR]
        self.assertGreaterEqual(len(own), 6, "tests/differential lost programs")
        self.assertGreaterEqual(len(self.programs), 60, "backend runtime corpus shrank")

    def test_skips_carry_reasons(self) -> None:
        for src, reason in self.skipped:
            with self.subTest(program=src.name):
                self.assertTrue(reason, f"{src.name}: {DIFF_SKIP_MARKER} needs a reason")

    def test_optimized_builds_match_o0(self) -> None:
        jobs = max(1, int(os.environ.get("BAA_DIFF_JOBS", "2")))
        with tempfile.TemporaryDirectory(prefix="baa_opt_diff_") as temp:
            out_root = Path(temp)

            def check(src: Path) -> list[str]:
                out_dir = out_root / src.stem
                out_dir.mkdir()
                return compare_program(self.baa, src, out_dir)

            with ThreadPoolExecutor(max_workers=jobs) as pool:
                results = list(pool.map(check, self.programs))

        failures = [f for per_program in results for f in per_program]
        print(
            f"opt-differential: {len(self.programs)} programs x {1 + len(VARIANTS)} levels, "
            f"{len(self.skipped)} skipped, {len(failures)} failures"
        )
        self.assertEqual(failures, [], "\n" + "\n".join(failures))


if __name__ == "__main__":
    unittest.main()
