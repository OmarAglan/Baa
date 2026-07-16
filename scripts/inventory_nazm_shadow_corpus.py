#!/usr/bin/env python3
"""Classify every inventoried Baa source against the opt-in Nazm emitter."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "baa-nazm-shadow-corpus-v1"
INVENTORY_SCHEMA = "baa-assembly-surface-v1"
DEFAULT_INVENTORY = ROOT / "docs" / "generated" / "assembly_surface_v1.json"
DEFAULT_OUTPUT = ROOT / "docs" / "generated" / "baa_nazm_shadow_corpus_v1.json"
TARGETS = ("x86_64-linux", "x86_64-windows")
LATIN_LETTER_RE = re.compile(r"[A-Za-z]")
LOCATION_RE = re.compile(r"\s+\((.+):(\d+):(\d+)\)\s*$")
BLOCKER_RE = re.compile(
    r"\s+\[عائق_نظم=([^؛\]]+)(?:؛تفصيل=([^\]]+))?\]\s*$"
)
ARABIC_DIGIT_TRANSLATION = str.maketrans("0123456789", "٠١٢٣٤٥٦٧٨٩")
INVENTORY_OUTPUT_FLAGS = {"-S", "-c", "--check", "--check-header", "--emit-ir"}


def _read_inventory(path: Path) -> tuple[dict[str, Any], bytes]:
    try:
        raw = path.read_bytes()
        payload = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read {path}: {exc}") from exc
    if not isinstance(payload, dict) or payload.get("schema") != INVENTORY_SCHEMA:
        actual = payload.get("schema") if isinstance(payload, dict) else None
        raise ValueError(
            f"{path} has schema {actual!r}; expected {INVENTORY_SCHEMA!r}"
        )
    return payload, raw


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


def _normalize_path(text: str) -> str:
    normalized = text.replace(str(ROOT.resolve()), ".")
    return normalized.replace("\\", "/")


def _diagnostic_and_blocker(stderr: str) -> tuple[str, dict[str, str] | None]:
    lines = [line.strip() for line in stderr.splitlines() if line.strip()]
    message = _normalize_path(lines[-1] if lines else "missing compiler diagnostic")
    blocker: dict[str, str] | None = None
    blocker_match = BLOCKER_RE.search(message)
    if blocker_match:
        blocker = {"kind": blocker_match.group(1)}
        if blocker_match.group(2):
            blocker["detail"] = blocker_match.group(2)
        message = message[: blocker_match.start()].rstrip()
    match = LOCATION_RE.search(message)
    if match:
        message = message[: match.start()].rstrip()
    return message, blocker


def _diagnostic(stderr: str) -> str:
    return _diagnostic_and_blocker(stderr)[0]


def _source_flag_index(target: dict[str, Any]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for entry in target.get("source_flags", []):
        source = entry.get("source")
        flags = entry.get("flags")
        if isinstance(source, str) and isinstance(flags, list):
            result[source] = [
                str(flag)
                for flag in flags
                if str(flag) not in INVENTORY_OUTPUT_FLAGS
                and not str(flag).startswith("--target=")
            ]
    return result


def _arabic_output_name(index: int) -> str:
    digits = f"{index:04d}".translate(ARABIC_DIGIT_TRANSLATION)
    return f"خرج-{digits}.نظم"


def _classify_source(
    compiler: Path,
    target: str,
    source: str,
    flags: list[str],
    directory: Path,
    index: int,
) -> dict[str, Any]:
    output = directory / _arabic_output_name(index)
    result = subprocess.run(
        [
            str(compiler),
            *flags,
            "--emit-nazm",
            f"--target={target}",
            source,
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
    row: dict[str, Any] = {
        "source": source,
        "status": "error",
        "exit_code": result.returncode,
    }
    if flags:
        row["flags"] = flags

    if result.returncode == 0:
        if not output.is_file():
            row["reason"] = "The compiler reported success without a Nazm source."
            return row
        try:
            text = output.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            row["reason"] = f"The emitted Nazm source is unreadable: {exc}"
            return row
        latin_match = LATIN_LETTER_RE.search(text)
        if latin_match:
            row["reason"] = "The emitted Nazm source contains a Latin letter."
            row["latin_letter"] = latin_match.group(0)
            return row
        row["status"] = "emitted"
        row["sha256"] = hashlib.sha256(text.encode("utf-8")).hexdigest()
        return row

    row["reason"], blocker = _diagnostic_and_blocker(result.stderr)
    if blocker:
        row["blocker"] = blocker
    if result.returncode == 3 and not output.exists() and blocker:
        row["status"] = "unsupported"
    elif result.returncode == 3 and not blocker:
        row["reason"] = "Unsupported emission did not report a structured Nazm blocker."
    elif result.returncode == 3:
        row["reason"] = "Unsupported emission left a partial Nazm source."
    return row


def _target_matrix(
    compiler: Path,
    target_name: str,
    inventory_target: dict[str, Any],
    directory: Path,
) -> dict[str, Any]:
    sources = inventory_target.get("sources", [])
    if not isinstance(sources, list) or not all(isinstance(item, str) for item in sources):
        raise ValueError(f"inventory target {target_name!r} has invalid sources")
    if inventory_target.get("compile_failures"):
        raise ValueError(f"inventory target {target_name!r} has compile failures")
    if inventory_target.get("compiled_source_count") != len(sources):
        raise ValueError(f"inventory target {target_name!r} omits corpus sources")

    flag_index = _source_flag_index(inventory_target)
    rows = [
        _classify_source(
            compiler,
            target_name,
            source,
            flag_index.get(source, []),
            directory,
            index,
        )
        for index, source in enumerate(sources)
    ]
    counts = Counter(row["status"] for row in rows)
    reason_counts = Counter(
        row["reason"] for row in rows if row["status"] == "unsupported"
    )
    blocker_counts = Counter(
        (row["blocker"]["kind"], row["blocker"].get("detail", ""))
        for row in rows
        if row["status"] == "unsupported"
    )
    return {
        "source_count": len(rows),
        "summary": {
            status: counts[status] for status in ("emitted", "unsupported", "error")
        },
        "unsupported_reasons": [
            {"reason": reason, "count": count}
            for reason, count in sorted(
                reason_counts.items(), key=lambda item: (-item[1], item[0])
            )
        ],
        "unsupported_blockers": [
            {
                **{"kind": kind},
                **({"detail": detail} if detail else {}),
                "count": count,
            }
            for (kind, detail), count in sorted(
                blocker_counts.items(),
                key=lambda item: (-item[1], item[0][0], item[0][1]),
            )
        ],
        "sources": rows,
    }


def build_matrix(
    compiler: Path,
    inventory: dict[str, Any],
    inventory_sha256: str,
) -> dict[str, Any]:
    targets = inventory.get("targets")
    if not isinstance(targets, dict) or set(targets) != set(TARGETS):
        raise ValueError(f"inventory must contain exactly these targets: {TARGETS}")

    rendered_targets: dict[str, Any] = {}
    with tempfile.TemporaryDirectory(prefix="baa-nazm-shadow-corpus-") as temp:
        temp_root = Path(temp)
        for target_name in TARGETS:
            target_dir = temp_root / target_name
            target_dir.mkdir()
            rendered_targets[target_name] = _target_matrix(
                compiler, target_name, targets[target_name], target_dir
            )

    return {
        "schema": SCHEMA,
        "compiler": _compiler_version(compiler),
        "source_inventory": {
            "schema": INVENTORY_SCHEMA,
            "sha256": inventory_sha256,
        },
        "status_contract": {
            "emitted": "Baa emitted canonical Arabic Nazm without Latin letters.",
            "unsupported": "Baa returned compiler-cli-v1 code 3 and left no output.",
            "error": "The classification gate failed; this is never a GAS fallback.",
            "blocker": "Every unsupported row carries a stable Arabic kind and optional detail.",
        },
        "targets": rendered_targets,
    }


def _short_value(value: Any) -> str:
    rendered = repr(value)
    return rendered if len(rendered) <= 240 else rendered[:237] + "..."


def _first_difference(current: Any, generated: Any, path: str = "$") -> str | None:
    if type(current) is not type(generated):
        return (
            f"{path}: type {type(current).__name__} != "
            f"{type(generated).__name__}"
        )
    if isinstance(current, dict):
        current_keys = set(current)
        generated_keys = set(generated)
        if current_keys != generated_keys:
            return (
                f"{path}: keys {_short_value(sorted(current_keys))} != "
                f"{_short_value(sorted(generated_keys))}"
            )
        for key in current:
            difference = _first_difference(
                current[key], generated[key], f"{path}.{key}"
            )
            if difference:
                return difference
        return None
    if isinstance(current, list):
        if len(current) != len(generated):
            return f"{path}: length {len(current)} != {len(generated)}"
        for index, (current_item, generated_item) in enumerate(
            zip(current, generated)
        ):
            difference = _first_difference(
                current_item, generated_item, f"{path}[{index}]"
            )
            if difference:
                return difference
        return None
    if current != generated:
        return f"{path}: {_short_value(current)} != {_short_value(generated)}"
    return None


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify that --output already contains the generated matrix.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv or sys.argv[1:])
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        print(f"error: Baa compiler not found: {compiler}", file=sys.stderr)
        return 2
    try:
        inventory, inventory_raw = _read_inventory(args.inventory.resolve())
        document = build_matrix(
            compiler,
            inventory,
            hashlib.sha256(inventory_raw).hexdigest(),
        )
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    has_errors = any(
        target["summary"]["error"] != 0 for target in document["targets"].values()
    )
    generated = json.dumps(document, ensure_ascii=False, indent=2) + "\n"
    output = args.output.resolve()
    if args.check:
        try:
            current = output.read_text(encoding="utf-8")
        except OSError as exc:
            print(f"error: cannot read checked matrix {output}: {exc}", file=sys.stderr)
            return 1
        if current != generated:
            try:
                current_payload = json.loads(current)
            except json.JSONDecodeError as exc:
                difference = f"$: checked matrix is invalid JSON: {exc}"
            else:
                difference = _first_difference(current_payload, document)
                if difference is None:
                    difference = "$: semantic content matches; JSON formatting differs"
            print(
                "error: checked Baa-to-Nazm shadow matrix is stale; regenerate it without --check",
                file=sys.stderr,
            )
            print(f"error: first difference: {difference}", file=sys.stderr)
            return 1
        print(f"verified {output}")
        return 1 if has_errors else 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding="utf-8", newline="\n")
    print(f"wrote {output}")
    return 1 if has_errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
