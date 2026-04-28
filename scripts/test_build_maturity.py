#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env and Path(env).exists():
        return Path(env)
    if os.name == "nt":
        for p in (ROOT / "build" / "baa.exe", ROOT / "build" / "baa"):
            if p.exists():
                return p
    for p in (ROOT / "build-linux" / "baa", ROOT / "build" / "baa"):
        if p.exists():
            return p
    raise FileNotFoundError("build-maturity: compiler binary not found")


def _run(cmd: list[str], cwd: Path) -> None:
    p = subprocess.run(cmd, cwd=str(cwd), text=True, encoding="utf-8", errors="replace",
                       capture_output=True)
    if p.returncode != 0:
        print(p.stdout)
        print(p.stderr, file=sys.stderr)
        raise RuntimeError(f"command failed ({p.returncode}): {' '.join(cmd)}")


def _build(baa: Path, work: Path, manifest: Path) -> dict:
    suffix = ".exe" if os.name == "nt" else ""
    exe = work / f"app_{manifest.stem}{suffix}"
    cmd = [
        str(baa),
        "--incremental",
        "--cache-dir",
        str(work / "cache"),
        "--emit-build-manifest",
        str(manifest),
        str(work / "main.baa"),
        str(work / "lib.baa"),
        "-o",
        str(exe),
    ]
    _run(cmd, ROOT)
    data = json.loads(manifest.read_text(encoding="utf-8"))
    _run([str(exe)], ROOT)
    return data


def _hits(data: dict) -> dict[str, bool]:
    result: dict[str, bool] = {}
    for unit in data.get("units", []):
        source = Path(unit.get("source", "")).name
        result[source] = bool(unit.get("cache", {}).get("hit"))
    return result


def main() -> int:
    baa = _find_baa()
    work = ROOT / f".baa_build_maturity_out_{os.getpid()}"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    try:
        (work / "lib.baahd").write_text("صحيح قيمة().\n", encoding="utf-8")
        (work / "lib.baa").write_text("صحيح قيمة() { إرجع ٧. }\n", encoding="utf-8")
        (work / "main.baa").write_text(
            "#تضمين \"lib.baahd\"\n"
            "صحيح الرئيسية() {\n"
            "    إذا (قيمة() != ٧) { إرجع ١. }\n"
            "    إرجع ٠.\n"
            "}\n",
            encoding="utf-8",
        )

        first = _build(baa, work, work / "m1.json")
        if any(_hits(first).values()):
            print("build-maturity: first build should miss cache")
            return 1

        second = _build(baa, work, work / "m2.json")
        if _hits(second) != {"main.baa": True, "lib.baa": True}:
            print(f"build-maturity: second build should hit cache, got {_hits(second)}")
            return 1

        third = _build(baa, work, work / "m3.json")
        if (work / "m2.json").read_bytes() != (work / "m3.json").read_bytes():
            print("build-maturity: identical cached manifests are not byte-stable")
            return 1

        (work / "lib.baahd").write_text("// touch dependency\nصحيح قيمة().\n", encoding="utf-8")
        fourth = _build(baa, work, work / "m4.json")
        expected = {"main.baa": False, "lib.baa": True}
        if _hits(fourth) != expected:
            print(f"build-maturity: header invalidation mismatch, got {_hits(fourth)}")
            return 1

        print("build-maturity: PASS")
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
