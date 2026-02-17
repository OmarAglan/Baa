#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"


def _run(cmd: list[str], cwd: Path) -> int:
    p = subprocess.run(cmd, cwd=str(cwd))
    return int(p.returncode)


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


def main() -> int:
    baa = _find_baa()

    # 1) Always run IR integration tests (host)
    rc = _run([sys.executable, str(TESTS_DIR / "test.py")], cwd=ROOT)
    if rc != 0:
        print("regress: FAIL (tests/test.py)")
        return rc

    # 1.5) Docs-derived v0.2.x corpus (compile+run with IR backend)
    corpus_v2x = sorted((TESTS_DIR / "corpus_v2x_docs").glob("**/*.baa"))
    if corpus_v2x:
        exe_ext = ".exe" if os.name == "nt" else ""
        with tempfile.TemporaryDirectory(prefix="baa_regress_corpus_") as td:
            out_dir = Path(td)
            for src in corpus_v2x:
                src_rel = src.relative_to(ROOT)
                out = out_dir / f"{src.stem}{exe_ext}"
                # Use repo-relative paths to avoid space-in-path toolchain issues.
                cmd = [str(baa), "-O1", "--verify", "--backend=ir", str(src_rel), "-o", str(out)]
                p = subprocess.run(cmd, cwd=str(ROOT), text=True, capture_output=True)
                if p.returncode != 0:
                    print(f"regress: FAIL (corpus_v2x compile): {src_rel}")
                    sys.stderr.write(p.stderr)
                    return int(p.returncode)

                r = subprocess.run([str(out)], cwd=str(ROOT), text=True, capture_output=True)
                if r.returncode != 0:
                    print(f"regress: FAIL (corpus_v2x run exit={r.returncode}): {src_rel}")
                    return int(r.returncode)
    else:
        print("regress: WARN (no tests/corpus_v2x_docs/**/*.baa)")

    # 2) Windows-only: legacy-vs-IR output comparisons
    if os.name == "nt":
        corpus = sorted((TESTS_DIR / "corpus_compare").glob("*.baa"))
        if not corpus:
            print("regress: WARN (no tests/corpus_compare/*.baa)")
        else:
            for src in corpus:
                src_rel = src.relative_to(ROOT)
                rc = _run([str(baa), "-O1", "--compare-backends", str(src_rel)], cwd=ROOT)
                if rc != 0:
                    print(f"regress: FAIL (compare-backends): {src.name}")
                    return rc

    # 3) Negative tests: error-message anchors
    neg = sorted((TESTS_DIR / "neg").glob("*.baa"))
    if neg:
        for src in neg:
            src_rel = src.relative_to(ROOT)
            markers = _expected_markers(src)
            # Use a unique dummy output to avoid collisions.
            exe_ext = ".exe" if os.name == "nt" else ""
            with tempfile.TemporaryDirectory(prefix="baa_regress_neg_") as td:
                dummy = Path(td) / f"neg_out{exe_ext}"
                p = subprocess.run(
                    [str(baa), "-O1", "--backend=ir", str(src_rel), "-o", str(dummy)],
                    cwd=str(ROOT),
                    text=True,
                    capture_output=True,
                )
            if p.returncode == 0:
                print(f"regress: FAIL (neg should fail): {src_rel}")
                return 1
            se = p.stderr or ""
            for m in markers:
                if m and m not in se:
                    print(f"regress: FAIL (neg missing marker '{m}'): {src_rel}")
                    return 1
    else:
        print("regress: WARN (no tests/neg/*.baa)")

    print("regress: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
