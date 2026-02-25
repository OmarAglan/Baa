#!/usr/bin/env python3

from __future__ import annotations

import os
import shlex
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"

CORPUS_COMPILE_TIMEOUT_S = 20.0
CORPUS_RUN_TIMEOUT_S = 8.0


def _run(cmd: list[str], cwd: Path) -> int:
    p = subprocess.run(cmd, cwd=str(cwd))
    return int(p.returncode)


def _run_capture(cmd: list[str], cwd: Path, timeout_s: float) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=timeout_s,
    )


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
        "Could not find compiler binary. Build it first (build/baa.exe or build-linux/baa), "
        "or set env var BAA to its path."
    )


def _expected_markers(src: Path) -> list[str]:
    markers: list[str] = []
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if s.startswith("// EXPECT:"):
                markers.append(s.split(":", 1)[1].strip())
    except Exception:
        return []
    return markers


def _flags_markers(src: Path) -> list[str]:
    flags: list[str] = []
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if s.startswith("// FLAGS:"):
                raw = s.split(":", 1)[1].strip()
                if raw:
                    flags.extend(shlex.split(raw))
    except Exception:
        return []
    return flags


def _run_markers(src: Path) -> set[str]:
    markers: set[str] = set()
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if not s.startswith("// RUN:"):
                continue
            raw = s.split(":", 1)[1].strip()
            if not raw:
                continue
            for part in raw.replace(",", " ").split():
                v = part.strip().lower()
                if v:
                    markers.add(v)
    except Exception:
        return set()
    return markers


def main() -> int:
    baa = _find_baa()

    # 1) Always run integration suite first (host)
    rc = _run([sys.executable, str(TESTS_DIR / "test.py")], cwd=ROOT)
    if rc != 0:
        print("regress: FAIL (tests/test.py)")
        return rc

    # 1.5) Docs-derived v0.2.x corpus (compile+run with IR backend)
    corpus_root = TESTS_DIR / "corpus_v2x_docs"
    corpus_v2x = sorted(corpus_root.glob("**/*.baa"))
    if corpus_v2x:
        exe_ext = ".exe" if os.name == "nt" else ""
        out_dir = ROOT / f".baa_regress_corpus_out_{os.getpid()}"
        if out_dir.exists():
            import shutil
            shutil.rmtree(out_dir, ignore_errors=True)
        out_dir.mkdir(parents=True, exist_ok=True)
        try:
            for src in corpus_v2x:
                src_rel = src.relative_to(ROOT)
                out = out_dir / f"{src.stem}{exe_ext}"
                # Use repo-relative paths to avoid space-in-path toolchain issues.
                cmd = [str(baa), "-O1", "--verify", str(src_rel), "-o", str(out)]
                try:
                    p = _run_capture(cmd, ROOT, CORPUS_COMPILE_TIMEOUT_S)
                except subprocess.TimeoutExpired:
                    print(
                        f"regress: FAIL (corpus_v2x compile timeout>{CORPUS_COMPILE_TIMEOUT_S}s): "
                        f"{src_rel}"
                    )
                    return 1
                if p.returncode != 0:
                    print(f"regress: FAIL (corpus_v2x compile): {src_rel}")
                    sys.stderr.write(p.stdout)
                    sys.stderr.write(p.stderr)
                    return int(p.returncode)

                try:
                    r = _run_capture([str(out)], ROOT, CORPUS_RUN_TIMEOUT_S)
                except subprocess.TimeoutExpired:
                    print(f"regress: FAIL (corpus_v2x run timeout>{CORPUS_RUN_TIMEOUT_S}s): {src_rel}")
                    return 1
                if r.returncode != 0:
                    print(f"regress: FAIL (corpus_v2x run exit={r.returncode}): {src_rel}")
                    sys.stderr.write(r.stdout)
                    sys.stderr.write(r.stderr)
                    return int(r.returncode)
        finally:
            import shutil
            shutil.rmtree(out_dir, ignore_errors=True)
    else:
        # Auto-generate from git tags if missing.
        extractor = ROOT / "scripts" / "extract_docs_corpus.py"
        if extractor.exists():
            rc2 = _run(
                [
                    sys.executable,
                    str(extractor),
                    "--git-ref-pattern",
                    "0.2.*",
                    "--out-dir",
                    str(corpus_root.relative_to(ROOT)),
                    "--opt",
                    "O1",
                    "--verify",
                    "--run",
                    "--max",
                    "80",
                    "--compile-timeout",
                    str(CORPUS_COMPILE_TIMEOUT_S),
                    "--run-timeout",
                    str(CORPUS_RUN_TIMEOUT_S),
                ],
                cwd=ROOT,
            )
            if rc2 != 0:
                print("regress: FAIL (corpus_v2x generate)")
                return rc2
            corpus_v2x = sorted(corpus_root.glob("**/*.baa"))
            if not corpus_v2x:
                print("regress: FAIL (corpus_v2x empty after generate)")
                return 1
            return main()

        print("regress: WARN (no tests/corpus_v2x_docs/**/*.baa)")

     # 2) Negative tests: error-message anchors
    neg = sorted((TESTS_DIR / "neg").glob("*.baa"))
    if neg:
        for src in neg:
            src_rel = src.relative_to(ROOT)
            markers = _expected_markers(src)
            flags = _flags_markers(src)
            run_markers = _run_markers(src)
            if "skip" in run_markers:
                continue
            expect_fail = "expect-pass" not in run_markers
            # Use a unique dummy output to avoid collisions.
            exe_ext = ".exe" if os.name == "nt" else ""
            out_dir = ROOT / f".baa_regress_neg_out_{os.getpid()}"
            if out_dir.exists():
                import shutil
                shutil.rmtree(out_dir, ignore_errors=True)
            out_dir.mkdir(parents=True, exist_ok=True)
            try:
                dummy = out_dir / f"neg_out{exe_ext}"
                try:
                    p = _run_capture(
                        [str(baa), "-O1", *flags, str(src_rel), "-o", str(dummy)],
                        ROOT,
                        CORPUS_COMPILE_TIMEOUT_S,
                    )
                except subprocess.TimeoutExpired:
                    print(
                        f"regress: FAIL (neg compile timeout>{CORPUS_COMPILE_TIMEOUT_S}s): "
                        f"{src_rel}"
                    )
                    return 1
            finally:
                import shutil
                shutil.rmtree(out_dir, ignore_errors=True)
            if expect_fail:
                if p.returncode == 0:
                    print(f"regress: FAIL (neg should fail): {src_rel}")
                    return 1
                se = p.stderr or ""
                for m in markers:
                    if m and m not in se:
                        print(f"regress: FAIL (neg missing marker '{m}'): {src_rel}")
                        return 1
            else:
                if p.returncode != 0:
                    print(f"regress: FAIL (neg marked expect-pass): {src_rel}")
                    return 1
    else:
        print("regress: WARN (no tests/neg/*.baa)")

    print("regress: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
