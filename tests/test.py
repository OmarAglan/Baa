#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import shutil
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"


def _run(cmd: list[str], cwd: Path | None = None) -> int:
    p = subprocess.run(cmd, cwd=str(cwd) if cwd else None)
    return int(p.returncode)


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = [
        ROOT / "build-linux" / "baa",
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "baa",
    ]

    for c in candidates:
        if c.exists():
            return c

    raise FileNotFoundError(
        "Could not find compiler binary. Build it first (build-linux/baa or build/baa.exe), "
        "or set env var BAA to its path."
    )


def main() -> int:
    baa_real = _find_baa()
    baa_files = sorted(TESTS_DIR.glob("*.baa"))
    if not baa_files:
        print("No .baa tests found.")
        return 1

    failures: list[str] = []

    exe_ext = ".exe" if os.name == "nt" else ""
    out_dir = Path(f".baa_test_out_{os.getpid()}")
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Avoid "Text file busy" if a rebuild replaces the compiler while tests run.
    baa = baa_real
    try:
        baa_copy = out_dir / (baa_real.name + ".copy")
        shutil.copy2(baa_real, baa_copy)
        if os.name != "nt":
            st = baa_copy.stat()
            baa_copy.chmod(st.st_mode | 0o111)
        baa = baa_copy
    except Exception:
        baa = baa_real

    try:
        for src in baa_files:
            # IMPORTANT: use repo-relative paths to avoid space-splitting bugs
            # in toolchain command strings when the repo root contains spaces.
            src_rel = src.relative_to(ROOT)
            out = out_dir / f"{src.stem}{exe_ext}"

            # -O2 is required because --verify runs SSA verification (Mem2Reg) via the optimizer.
            rc = _run([str(baa), "-O2", "--verify", str(src_rel), "-o", str(out)], cwd=ROOT)
            if rc != 0:
                failures.append(f"compile/verify failed: {src.name}")
                continue

            # Only execute runtime integration tests that are designed to return 0 on PASS.
            if not src.stem.startswith("backend_"):
                continue

            rc = _run([str(out)], cwd=ROOT)
            if rc != 0:
                failures.append(f"run failed (exit={rc}): {src.name}")
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    if failures:
        print("tests: FAIL")
        for f in failures:
            print(f"  - {f}")
        return 1

    print("tests: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
