#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT_DIR = ROOT / f".baa_asm_legality_out_{os.getpid()}"

TWO_ADDRESS_OPS = {"add", "sub", "and", "or", "xor"}
IMUL_OP = "imul"
INST_RE = re.compile(r"^\s*([A-Za-z]+)[bwlq]?\s+([^,]+),\s*(.+?)\s*$")


def _find_compiler(explicit: str | None) -> Path:
    if explicit:
        compiler = Path(explicit)
        if compiler.exists():
            return compiler
        raise FileNotFoundError(f"compiler not found: {compiler}")

    env = os.environ.get("BAA")
    if env:
        compiler = Path(env)
        if compiler.exists():
            return compiler

    candidates = [
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for compiler in candidates:
        if compiler.exists():
            return compiler

    raise FileNotFoundError("compiler not found; pass --compiler or set BAA")


def _is_memory_operand(operand: str) -> bool:
    text = operand.strip()
    if text.startswith("$"):
        return False
    return "(" in text and ")" in text


def _scan_illegal_asm(asm_text: str) -> list[tuple[int, str]]:
    illegal: list[tuple[int, str]] = []
    for lineno, line in enumerate(asm_text.splitlines(), start=1):
        match = INST_RE.match(line)
        if not match:
            continue

        op = match.group(1).lower()
        src = match.group(2)
        dst = match.group(3)

        if op in TWO_ADDRESS_OPS and _is_memory_operand(src) and _is_memory_operand(dst):
            illegal.append((lineno, line.strip()))
        elif op == IMUL_OP and _is_memory_operand(dst):
            illegal.append((lineno, line.strip()))

    return illegal


def main() -> int:
    parser = argparse.ArgumentParser(description="Check emitted assembly for illegal x86 memory operands")
    parser.add_argument("--compiler", default="")
    parser.add_argument("--source", default="src/frontend/lexer.baa")
    parser.add_argument("--target", default="x86_64-linux")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    parser.add_argument("--keep-output", action="store_true")
    args = parser.parse_args()

    compiler = _find_compiler(args.compiler or None)
    source = Path(args.source)
    if not source.is_absolute():
        source = ROOT / source
    if not source.exists():
        print(f"asm-legality: missing source: {source}", file=sys.stderr)
        return 1

    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    if out_dir.exists():
        shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    asm_path = out_dir / "asm_legality.s"
    source_arg = str(source.relative_to(ROOT)) if source.is_relative_to(ROOT) else str(source)
    cmd = [
        str(compiler),
        "--target",
        args.target,
        "-S",
        "-O2",
        "--verify-ir",
        "--verify-ssa",
        source_arg,
        "-o",
        str(asm_path.relative_to(ROOT) if asm_path.is_relative_to(ROOT) else asm_path),
    ]

    result = subprocess.run(
        cmd,
        cwd=ROOT,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        print(f"asm-legality: compiler failed with rc={result.returncode}", file=sys.stderr)
        return int(result.returncode)

    asm_text = asm_path.read_text(encoding="utf-8", errors="replace")
    illegal = _scan_illegal_asm(asm_text)
    if illegal:
        print("asm-legality: FAIL illegal memory operands:")
        for lineno, inst in illegal:
            print(f"  {asm_path}:{lineno}: {inst}")
        return 1

    print(f"asm-legality: PASS target={args.target} source={source_arg}")
    if not args.keep_output:
        shutil.rmtree(out_dir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
