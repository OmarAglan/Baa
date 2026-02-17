#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BENCH_DIR = ROOT / "bench"
RESULTS_DIR = BENCH_DIR / "results"


TIME_RE = re.compile(r"^\[TIME\]\s+(.*)$")
MEM_RE = re.compile(r"^\[MEM\]\s+(.*)$")


def _run_capture(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=True)


def _run_no_capture(cmd: list[str], cwd: Path) -> int:
    p = subprocess.run(cmd, cwd=str(cwd))
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


def _git_rev() -> str | None:
    try:
        p = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(ROOT),
            text=True,
            capture_output=True,
            check=False,
        )
        if p.returncode != 0:
            return None
        return p.stdout.strip() or None
    except Exception:
        return None


def _parse_kv_tail(line: str) -> dict[str, str]:
    out: dict[str, str] = {}
    parts = [p for p in line.strip().split() if p]
    for p in parts:
        if "=" not in p:
            continue
        k, v = p.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def _extract_compiler_stats(stderr: str) -> dict[str, dict[str, str]]:
    time_kv: dict[str, str] = {}
    mem_kv: dict[str, str] = {}
    for raw in stderr.splitlines():
        m = TIME_RE.match(raw)
        if m:
            time_kv = _parse_kv_tail(m.group(1))
            continue
        m = MEM_RE.match(raw)
        if m:
            mem_kv = _parse_kv_tail(m.group(1))
            continue
    out: dict[str, dict[str, str]] = {}
    if time_kv:
        out["time_phases"] = time_kv
    if mem_kv:
        out["mem"] = mem_kv
    return out


def _is_linux() -> bool:
    return sys.platform.startswith("linux")


def _time_v_rss_kb(stderr: str) -> int | None:
    # /usr/bin/time -v prints: "Maximum resident set size (kbytes): 12345"
    for line in stderr.splitlines():
        if "Maximum resident set size" in line:
            try:
                return int(line.split(":", 1)[1].strip())
            except Exception:
                return None
    return None


@dataclass
class BenchResult:
    name: str
    kind: str
    opt: str
    metrics: dict


def _bench_files() -> list[Path]:
    if not BENCH_DIR.exists():
        return []
    return sorted([p for p in BENCH_DIR.glob("*.baa") if p.is_file()])


def _classify(p: Path) -> str:
    stem = p.stem
    if stem.startswith("runtime_"):
        return "runtime"
    if stem.startswith("compile_"):
        return "compile"
    return "unknown"


def _compile_cmd(
    baa: Path,
    src_rel: Path,
    out_path: Path,
    opt: str,
    target: str | None,
    verify: bool,
    time_phases: bool,
    to_asm_only: bool,
) -> list[str]:
    cmd: list[str] = [str(baa), f"-{opt}"]
    if target:
        cmd.append(f"--target={target}")
    if verify:
        cmd.append("--verify")
    if time_phases:
        cmd.append("--time-phases")
    if to_asm_only:
        cmd.append("-S")
    cmd.extend([str(src_rel), "-o", str(out_path)])
    return cmd


def _measure_compile_wall(
    cmd: list[str],
    use_time_v: bool,
) -> tuple[float, int, str, str, int | None, dict]:
    if use_time_v:
        p_cmd = ["/usr/bin/time", "-v"] + cmd
        t0 = time.perf_counter()
        p = subprocess.run(p_cmd, cwd=str(ROOT), text=True, capture_output=True)
        dt = time.perf_counter() - t0
        rss_kb = _time_v_rss_kb(p.stderr)
        stats = _extract_compiler_stats(p.stderr)
        return dt, int(p.returncode), p.stdout, p.stderr, rss_kb, stats

    t0 = time.perf_counter()
    p = subprocess.run(cmd, cwd=str(ROOT), text=True, capture_output=True)
    dt = time.perf_counter() - t0
    stats = _extract_compiler_stats(p.stderr)
    return dt, int(p.returncode), p.stdout, p.stderr, None, stats


def _median(xs: list[float]) -> float:
    ys = sorted(xs)
    n = len(ys)
    if n == 0:
        return 0.0
    mid = n // 2
    if n % 2 == 1:
        return ys[mid]
    return 0.5 * (ys[mid - 1] + ys[mid])


def main() -> int:
    ap = argparse.ArgumentParser(description="Baa benchmark runner")
    ap.add_argument("--mode", choices=["all", "compile_s", "compile_exe", "runtime", "mem"], default="all")
    ap.add_argument("--opt", nargs="+", default=["O2"], choices=["O0", "O1", "O2"])
    ap.add_argument("--runs", type=int, default=7)
    ap.add_argument("--compile-runs", type=int, default=5)
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--time-phases", action="store_true")
    ap.add_argument("--target", default=None)
    args = ap.parse_args()

    baa = _find_baa()
    bench_files = _bench_files()
    if not bench_files:
        print("No benchmark files found in bench/*.baa")
        return 1

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_dir = ROOT / ".bench_out"
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    host = {
        "platform": sys.platform,
        "python": sys.version.split()[0],
        "machine": platform.machine(),
        "system": platform.system(),
        "release": platform.release(),
    }
    rev = _git_rev()
    compiler_version = _run_capture([str(baa), "--version"], cwd=ROOT).stdout.strip()

    results: dict = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "git_rev": rev,
        "host": host,
        "compiler": {"path": str(baa), "version": compiler_version},
        "config": {
            "mode": args.mode,
            "opt": args.opt,
            "runs": args.runs,
            "compile_runs": args.compile_runs,
            "verify": bool(args.verify),
            "time_phases": bool(args.time_phases),
            "target": args.target,
        },
        "benchmarks": [],
    }

    use_time_v = (args.mode in ("all", "mem")) and _is_linux()

    exe_ext = ".exe" if os.name == "nt" else ""

    try:
        for src in bench_files:
            kind = _classify(src)
            if kind == "unknown":
                continue

            src_rel = src.relative_to(ROOT)
            name = src.stem

            for opt in args.opt:
                metrics: dict = {}

                if args.mode in ("all", "compile_s", "mem"):
                    asm_out = out_dir / f"{name}.{opt}.s"
                    cmd = _compile_cmd(
                        baa=baa,
                        src_rel=src_rel,
                        out_path=asm_out,
                        opt=opt,
                        target=args.target,
                        verify=args.verify,
                        time_phases=args.time_phases,
                        to_asm_only=True,
                    )

                    dts: list[float] = []
                    rss: list[int] = []
                    last_stats: dict = {}
                    for _ in range(args.compile_runs):
                        dt, rc, _so, se, rss_kb, stats = _measure_compile_wall(cmd, use_time_v=(args.mode == "mem"))
                        if rc != 0:
                            metrics["compile_s_error"] = se[-4000:]
                            break
                        dts.append(dt)
                        if rss_kb is not None:
                            rss.append(rss_kb)
                        if stats:
                            last_stats = stats

                    if dts:
                        metrics["compile_s_wall_s"] = {
                            "runs": dts,
                            "median": _median(dts),
                        }
                        if rss:
                            metrics["compile_s_rss_kb"] = {
                                "runs": rss,
                                "max": max(rss),
                            }
                        if last_stats:
                            metrics["compile_s_compiler_stats"] = last_stats

                if args.mode in ("all", "compile_exe") and kind in ("runtime", "compile"):
                    exe_out = out_dir / f"{name}.{opt}{exe_ext}"
                    cmd = _compile_cmd(
                        baa=baa,
                        src_rel=src_rel,
                        out_path=exe_out,
                        opt=opt,
                        target=args.target,
                        verify=args.verify,
                        time_phases=args.time_phases,
                        to_asm_only=False,
                    )
                    dts: list[float] = []
                    for _ in range(args.compile_runs):
                        t0 = time.perf_counter()
                        p = subprocess.run(cmd, cwd=str(ROOT), text=True, capture_output=True)
                        dt = time.perf_counter() - t0
                        if p.returncode != 0:
                            metrics["compile_exe_error"] = p.stderr[-4000:]
                            break
                        dts.append(dt)
                    if dts:
                        metrics["compile_exe_wall_s"] = {"runs": dts, "median": _median(dts)}

                if args.mode in ("all", "runtime") and kind == "runtime":
                    exe_out = out_dir / f"{name}.{opt}{exe_ext}"
                    cmd = _compile_cmd(
                        baa=baa,
                        src_rel=src_rel,
                        out_path=exe_out,
                        opt=opt,
                        target=args.target,
                        verify=args.verify,
                        time_phases=args.time_phases,
                        to_asm_only=False,
                    )
                    p = subprocess.run(cmd, cwd=str(ROOT), text=True, capture_output=True)
                    if p.returncode != 0:
                        metrics["runtime_compile_error"] = p.stderr[-4000:]
                    else:
                        dts: list[float] = []
                        for _ in range(args.runs):
                            t0 = time.perf_counter()
                            rc = _run_no_capture([str(exe_out)], cwd=ROOT)
                            dt = time.perf_counter() - t0
                            if rc != 0:
                                metrics["runtime_error"] = f"exit={rc}"
                                break
                            dts.append(dt)
                        if dts:
                            metrics["runtime_wall_s"] = {"runs": dts, "median": _median(dts)}

                results["benchmarks"].append(
                    BenchResult(name=name, kind=kind, opt=opt, metrics=metrics).__dict__
                )
                print(f"{name} {opt}: ok")

        out_path = RESULTS_DIR / f"bench_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        out_path.write_text(json.dumps(results, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        print(f"wrote: {out_path.relative_to(ROOT)}")
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
