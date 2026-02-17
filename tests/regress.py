#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import sys
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

    candidates = [
        ROOT / "build" / "baa.exe",
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


def main() -> int:
    baa = _find_baa()

    # 1) Always run IR integration tests (host)
    rc = _run([sys.executable, str(TESTS_DIR / "test.py")], cwd=ROOT)
    if rc != 0:
        print("regress: FAIL (tests/test.py)")
        return rc

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

    print("regress: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
