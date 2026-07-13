#!/usr/bin/env python3
"""Generate a deterministic inventory of the assembly surface emitted by Baa."""

from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "baa-assembly-surface-v1"
DEFAULT_TARGETS = ("x86_64-linux", "x86_64-windows")
DEFAULT_SOURCE_ROOTS = (
    ROOT / "tests" / "integration",
    ROOT / "tests" / "stress",
    ROOT / "examples",
)

LABEL_RE = re.compile(r"^([^\s:]+):\s*(.*)$")
TOKEN_RE = re.compile(r"^([^\s]+)(?:\s+(.*))?$")
INTEGER_RE = re.compile(r"^[+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+)$")
REGISTER_RE = re.compile(r"%[A-Za-z][A-Za-z0-9]*")
SYMBOL_RE = re.compile(r"^[.$A-Za-z_][.$A-Za-z0-9_@]*$")


def _relative(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path.resolve().as_posix()


def _strip_comment(line: str) -> str:
    quoted = False
    escaped = False
    for index, char in enumerate(line):
        if escaped:
            escaped = False
            continue
        if char == "\\" and quoted:
            escaped = True
            continue
        if char == '"':
            quoted = not quoted
            continue
        if char == "#" and not quoted:
            return line[:index]
    return line


def _split_operands(text: str) -> list[str]:
    if not text.strip():
        return []
    operands: list[str] = []
    current: list[str] = []
    depth = 0
    quoted = False
    escaped = False
    for char in text:
        if escaped:
            current.append(char)
            escaped = False
            continue
        if char == "\\" and quoted:
            current.append(char)
            escaped = True
            continue
        if char == '"':
            current.append(char)
            quoted = not quoted
            continue
        if not quoted:
            if char == "(":
                depth += 1
            elif char == ")" and depth:
                depth -= 1
            elif char == "," and depth == 0:
                operands.append("".join(current).strip())
                current = []
                continue
        current.append(char)
    operands.append("".join(current).strip())
    return [operand for operand in operands if operand]


def _operand_kind(operand: str) -> str:
    value = operand.strip()
    if not value:
        return "empty"
    if value.startswith("$"):
        payload = value[1:]
        return "immediate-integer" if INTEGER_RE.fullmatch(payload) else "immediate-symbol"
    if value.startswith('"'):
        return "string"
    if "(" in value and value.endswith(")"):
        if "%rip" in value:
            return "memory-rip-relative"
        registers = REGISTER_RE.findall(value)
        if len(registers) > 1:
            return "memory-base-index"
        return "memory-base-displacement"
    if value.startswith("%") and REGISTER_RE.fullmatch(value):
        return "register"
    if INTEGER_RE.fullmatch(value):
        return "integer"
    if SYMBOL_RE.fullmatch(value):
        return "local-symbol" if value.startswith(".L") else "symbol"
    if REGISTER_RE.search(value):
        return "register-expression"
    return "expression"


def _record(counter: Counter[str], samples: dict[str, list[str]], key: str, sample: str) -> None:
    counter[key] += 1
    if sample not in samples[key] and len(samples[key]) < 3:
        samples[key].append(sample)


def _is_relocation_candidate(mnemonic: str, operands: list[str]) -> bool:
    if not operands:
        return False
    if any("%rip" in operand for operand in operands):
        return True
    if mnemonic in {"call", "callq", "jmp", "jmpq"}:
        kind = _operand_kind(operands[-1])
        return kind == "symbol"
    return False


def inspect_assembly(
    paths: Iterable[Path], display_names: dict[Path, str] | None = None
) -> dict[str, object]:
    instruction_counts: Counter[str] = Counter()
    instruction_samples: dict[str, list[str]] = defaultdict(list)
    directive_counts: Counter[str] = Counter()
    directive_samples: dict[str, list[str]] = defaultdict(list)
    section_counts: Counter[str] = Counter()
    symbol_counts: Counter[str] = Counter()
    relocation_counts: Counter[str] = Counter()
    registers: Counter[str] = Counter()
    inspected_files: list[str] = []
    display_names = display_names or {}

    for path in sorted((Path(item) for item in paths), key=lambda item: item.as_posix()):
        display_name = display_names.get(path.resolve(), _relative(path))
        inspected_files.append(display_name)
        for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            line = _strip_comment(raw_line).strip()
            if not line:
                continue
            sample = f"{display_name}:{line_number}"

            label_match = LABEL_RE.match(line)
            if label_match:
                label, line = label_match.groups()
                symbol_counts["local" if label.startswith(".L") else "defined"] += 1
                line = line.strip()
                if not line:
                    continue

            token_match = TOKEN_RE.match(line)
            if not token_match:
                continue
            token, operand_text = token_match.groups()
            operand_text = operand_text or ""
            operands = _split_operands(operand_text)

            if token.startswith("."):
                kinds = ",".join(_operand_kind(operand) for operand in operands) or "none"
                form = f"{token}|{kinds}"
                _record(directive_counts, directive_samples, form, sample)
                if token in {".text", ".data", ".bss"}:
                    section_counts[token] += 1
                elif token == ".section" and operands:
                    section_counts[operands[0]] += 1
                if token == ".globl":
                    symbol_counts["global-declaration"] += len(operands)
                elif token == ".local":
                    symbol_counts["local-declaration"] += len(operands)
                elif token in {".quad", ".long", ".word", ".byte"}:
                    for operand in operands:
                        if _operand_kind(operand) in {"symbol", "local-symbol", "expression"}:
                            relocation_counts[f"data:{token}"] += 1
                continue

            kinds = ",".join(_operand_kind(operand) for operand in operands) or "none"
            form = f"{token}|{kinds}"
            _record(instruction_counts, instruction_samples, form, sample)
            for operand in operands:
                for register in REGISTER_RE.findall(operand):
                    registers[register] += 1
            if _is_relocation_candidate(token, operands):
                relocation_counts[f"instruction:{token}:{kinds}"] += 1

    def render_forms(
        counts: Counter[str], samples: dict[str, list[str]], key_name: str
    ) -> list[dict[str, object]]:
        rendered: list[dict[str, object]] = []
        for form in sorted(counts):
            token, operand_form = form.split("|", 1)
            rendered.append(
                {
                    key_name: token,
                    "operands": operand_form.split(",") if operand_form != "none" else [],
                    "count": counts[form],
                    "samples": samples[form],
                }
            )
        return rendered

    return {
        "assembly_files": inspected_files,
        "instructions": render_forms(instruction_counts, instruction_samples, "mnemonic"),
        "directives": render_forms(directive_counts, directive_samples, "directive"),
        "sections": [
            {"name": name, "count": section_counts[name]} for name in sorted(section_counts)
        ],
        "symbols": {name: symbol_counts[name] for name in sorted(symbol_counts)},
        "registers": [
            {"name": name, "count": registers[name]} for name in sorted(registers)
        ],
        "relocation_candidates": [
            {"form": form, "count": relocation_counts[form]}
            for form in sorted(relocation_counts)
        ],
    }


def _discover_sources(roots: Iterable[Path]) -> list[Path]:
    sources: set[Path] = set()
    for root in roots:
        candidate = Path(root)
        if candidate.is_file() and candidate.suffix == ".baa":
            sources.add(candidate.resolve())
        elif candidate.is_dir():
            sources.update(path.resolve() for path in candidate.rglob("*.baa"))
    return sorted(sources, key=lambda path: path.as_posix())


def _source_flags(source: Path) -> list[str]:
    for line in source.read_text(encoding="utf-8").splitlines()[:32]:
        stripped = line.strip()
        if stripped.startswith("// FLAGS:"):
            return shlex.split(stripped.removeprefix("// FLAGS:").strip(), posix=True)
        if stripped and not stripped.startswith("//"):
            break
    return []


def _compiler_version(compiler: Path) -> str:
    result = subprocess.run(
        [str(compiler), "--version"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        return "unknown"
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    return lines[0] if lines else "unknown"


def _compile_target(compiler: Path, target: str, sources: list[Path], directory: Path) -> dict[str, object]:
    assembly_files: list[Path] = []
    display_names: dict[Path, str] = {}
    failures: list[dict[str, object]] = []
    source_flags: list[dict[str, object]] = []
    for index, source in enumerate(sources):
        output = directory / f"{index:04d}_{source.stem}.s"
        flags = _source_flags(source)
        if flags:
            source_flags.append({"source": _relative(source), "flags": flags})
        result = subprocess.run(
            [
                str(compiler),
                *flags,
                f"--target={target}",
                "-S",
                str(source),
                "-o",
                str(output),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode != 0 or not output.is_file():
            failures.append(
                {
                    "source": _relative(source),
                    "exit_code": result.returncode,
                    "stderr": result.stderr.strip()[-1000:],
                }
            )
            continue
        assembly_files.append(output)
        display_names[output.resolve()] = _relative(source)

    inventory = inspect_assembly(assembly_files, display_names)
    inventory["sources"] = [_relative(source) for source in sources]
    inventory["source_flags"] = source_flags
    inventory["compiled_source_count"] = len(assembly_files)
    inventory["compile_failures"] = failures
    return inventory


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="Inventory JSON path")
    parser.add_argument("--assembly", type=Path, action="append", default=[], help="Existing .s file")
    parser.add_argument("--compiler", type=Path, help="Baa compiler used to generate the corpus")
    parser.add_argument("--target", action="append", choices=DEFAULT_TARGETS, default=[])
    parser.add_argument("--source-root", type=Path, action="append", default=[])
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify that --output already contains the generated inventory",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv or sys.argv[1:])
    output = args.output.resolve()
    document: dict[str, object] = {"schema": SCHEMA}
    has_failures = False

    if args.assembly:
        document["assembly"] = inspect_assembly(args.assembly)

    if args.compiler:
        compiler = args.compiler.resolve()
        if not compiler.is_file():
            print(f"error: Baa compiler not found: {compiler}", file=sys.stderr)
            return 2
        roots = args.source_root or list(DEFAULT_SOURCE_ROOTS)
        sources = _discover_sources(roots)
        if not sources:
            print("error: no Baa corpus sources found", file=sys.stderr)
            return 2
        document["compiler"] = _compiler_version(compiler)
        targets: dict[str, object] = {}
        with tempfile.TemporaryDirectory(prefix="baa-assembly-surface-") as temp:
            temp_root = Path(temp)
            for target in args.target or list(DEFAULT_TARGETS):
                target_dir = temp_root / target
                target_dir.mkdir(parents=True)
                target_inventory = _compile_target(compiler, target, sources, target_dir)
                targets[target] = target_inventory
                if target_inventory["compile_failures"]:
                    has_failures = True
        document["targets"] = targets

    if not args.assembly and not args.compiler:
        print("error: pass --assembly or --compiler", file=sys.stderr)
        return 2

    if has_failures:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        print(f"assembly inventory is incomplete; see compile_failures in {output}", file=sys.stderr)
        return 1
    generated = json.dumps(document, ensure_ascii=False, indent=2) + "\n"
    if args.check:
        if not output.is_file():
            print(f"error: checked inventory is missing: {output}", file=sys.stderr)
            return 1
        if output.read_text(encoding="utf-8") != generated:
            print(
                "error: checked inventory is stale; regenerate it with the same command without --check",
                file=sys.stderr,
            )
            return 1
        print(f"verified {output}")
        return 0
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding="utf-8", newline="\n")
    print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
