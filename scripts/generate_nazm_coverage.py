#!/usr/bin/env python3
"""Generate the deterministic Baa-to-Nazm assembly coverage contract."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "baa-nazm-coverage-v1"
INVENTORY_SCHEMA = "baa-assembly-surface-v1"
CAPABILITIES_SCHEMA = "nazm-capabilities-v1"
DEFAULT_INVENTORY = ROOT / "docs" / "generated" / "assembly_surface_v1.json"
DEFAULT_CAPABILITIES = (
    ROOT.parent / "Nazm" / "Docs" / "generated" / "nazm_capabilities_v1.json"
)
DEFAULT_OUTPUT = ROOT / "docs" / "generated" / "baa_nazm_coverage_v1.json"


SSE_MNEMONICS = {
    "addsd",
    "cvtsi2sd",
    "cvttsd2si",
    "divsd",
    "mulsd",
    "subsd",
    "ucomisd",
    "xorpd",
}


def _read_json(path: Path, expected_schema: str) -> tuple[dict[str, Any], bytes]:
    try:
        raw = path.read_bytes()
        payload = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read {path}: {exc}") from exc
    if not isinstance(payload, dict) or payload.get("schema") != expected_schema:
        actual = payload.get("schema") if isinstance(payload, dict) else None
        raise ValueError(
            f"{path} has schema {actual!r}; expected {expected_schema!r}"
        )
    return payload, raw


def _instruction_index(
    capabilities: dict[str, Any],
) -> tuple[dict[tuple[str, tuple[str, ...]], dict[str, Any]], set[str]]:
    forms: dict[tuple[str, tuple[str, ...]], dict[str, Any]] = {}
    known_mnemonics: set[str] = set()
    for instruction in capabilities.get("instructions", []):
        for mnemonic in instruction.get("gas_mnemonics", []):
            known_mnemonics.add(mnemonic)
            for form in instruction.get("forms", []):
                key = (mnemonic, tuple(form.get("gas", [])))
                if key in forms:
                    raise ValueError(f"duplicate Nazm instruction capability: {key}")
                forms[key] = {
                    "arabic": instruction["arabic"],
                    "width_bits": instruction["width_bits"],
                    "nazm_operands": form.get("nazm", []),
                    "status": form.get("coverage_status", "supported"),
                    "reason": form.get("reason"),
                    "constraint": form.get("constraint"),
                    "lowering": form.get("lowering"),
                }
    return forms, known_mnemonics


def _unsupported_instruction_reason(mnemonic: str, operands: list[str]) -> str:
    if "memory-rip-relative" in operands:
        return "Nazm does not implement RIP-relative memory operands."
    if "memory-base-index" in operands:
        return "Nazm does not implement base-index-scale memory operands."
    if mnemonic in SSE_MNEMONICS:
        return "Nazm does not implement scalar SSE2 registers or instructions."
    if mnemonic.startswith("set"):
        return "Nazm does not implement setcc instructions or 8-bit destinations."
    if mnemonic in {"cqo", "leave", "rdtsc"}:
        return f"Nazm does not implement the {mnemonic} instruction."
    if mnemonic in {"movsbl", "movsbq", "movslq", "movzbl", "movzbq"}:
        return "Nazm does not implement sign- or zero-extension instructions."
    if mnemonic in {"mov", "sub"}:
        return "The unsuffixed GAS form has no width in the inventory and cannot be mapped safely."
    if mnemonic.endswith(("b", "w", "l")):
        return "Nazm exposes only 64-bit general-purpose register instruction forms."
    return "The mnemonic is not implemented by Nazm."


def _classify_instructions(
    inventory_forms: list[dict[str, Any]],
    capabilities: dict[str, Any],
) -> list[dict[str, Any]]:
    index, known_mnemonics = _instruction_index(capabilities)
    fixture_index = capabilities.get("baa_acceptance_fixtures", {}).get(
        "instructions", {}
    )
    rendered: list[dict[str, Any]] = []
    for item in inventory_forms:
        mnemonic = item["mnemonic"]
        operands = item.get("operands", [])
        capability = index.get((mnemonic, tuple(operands)))
        row = {
            "mnemonic": mnemonic,
            "operands": operands,
            "count": item["count"],
            "samples": item.get("samples", []),
        }
        if capability is not None:
            row.update(
                {
                    "status": capability["status"],
                    "nazm": {
                        "mnemonic": capability["arabic"],
                        "operands": capability["nazm_operands"],
                        "width_bits": capability["width_bits"],
                    },
                }
            )
            for field in ("reason", "constraint", "lowering"):
                if capability.get(field):
                    row[field] = capability[field]
            if capability["status"] == "supported":
                fixture_key = f"{mnemonic}|{','.join(operands)}"
                fixture = fixture_index.get(fixture_key)
                if not fixture:
                    raise ValueError(
                        f"supported Baa instruction form has no acceptance fixture: {fixture_key}"
                    )
                row["acceptance_fixture"] = fixture
        else:
            row["status"] = "unsupported"
            if mnemonic in known_mnemonics:
                row["reason"] = (
                    "Nazm implements this mnemonic, but not this operand shape or width."
                )
            else:
                row["reason"] = _unsupported_instruction_reason(mnemonic, operands)
        rendered.append(row)
    return rendered


def _directive_index(
    capabilities: dict[str, Any],
) -> tuple[dict[tuple[str, tuple[str, ...]], dict[str, Any]], set[str]]:
    forms: dict[tuple[str, tuple[str, ...]], dict[str, Any]] = {}
    known: set[str] = set()
    for directive in capabilities.get("directives", []):
        name = directive["gas"]
        known.add(name)
        key = (name, tuple(directive.get("gas_operands", [])))
        if key in forms:
            raise ValueError(f"duplicate Nazm directive capability: {key}")
        forms[key] = directive
    return forms, known


def _directive_reason(name: str) -> str:
    if name == ".section":
        return "Nazm supports only its canonical .text and .data section directives."
    if name == ".p2align":
        return "Nazm does not implement an alignment directive."
    if name in {".file", ".loc"}:
        return "Nazm does not implement GAS debug/source mapping directives."
    if name == ".quad":
        return "Nazm integer data directives accept immediate values, not symbol relocations."
    return "The directive is not implemented by Nazm."


def _classify_directives(
    inventory_forms: list[dict[str, Any]],
    capabilities: dict[str, Any],
) -> list[dict[str, Any]]:
    index, known = _directive_index(capabilities)
    fixture_index = capabilities.get("baa_acceptance_fixtures", {}).get(
        "directives", {}
    )
    rendered: list[dict[str, Any]] = []
    for item in inventory_forms:
        name = item["directive"]
        operands = item.get("operands", [])
        capability = index.get((name, tuple(operands)))
        row = {
            "directive": name,
            "operands": operands,
            "count": item["count"],
            "samples": item.get("samples", []),
        }
        if capability is not None:
            status = capability.get("coverage_status", "supported")
            row.update(
                {
                    "status": status,
                    "nazm": {
                        "directive": capability["arabic"],
                        "operands": capability.get("nazm_operands", []),
                    },
                }
            )
            if capability.get("reason"):
                row["reason"] = capability["reason"]
            fixture_key = f"{name}|{','.join(operands)}"
            fixture = fixture_index.get(fixture_key)
            if status == "supported" and not fixture:
                raise ValueError(
                    f"supported Baa directive form has no acceptance fixture: {fixture_key}"
                )
            if fixture:
                row["acceptance_fixture"] = fixture
        else:
            row["status"] = "unsupported"
            row["reason"] = (
                "Nazm implements this directive, but not this operand shape."
                if name in known
                else _directive_reason(name)
            )
        rendered.append(row)
    return rendered


def _classify_sections(
    inventory_sections: list[dict[str, Any]], capabilities: dict[str, Any]
) -> list[dict[str, Any]]:
    index = {item["gas"]: item for item in capabilities.get("sections", [])}
    fixture_index = capabilities.get("baa_acceptance_fixtures", {}).get(
        "sections", {}
    )
    rendered: list[dict[str, Any]] = []
    for item in inventory_sections:
        name = item["name"]
        row = {"name": name, "count": item["count"]}
        capability = index.get(name)
        if capability:
            row.update(
                {
                    "status": "supported",
                    "nazm": capability["nazm"],
                    "object": capability["object"],
                }
            )
            fixture = fixture_index.get(name)
            if not fixture:
                raise ValueError(
                    f"supported Baa section has no acceptance fixture: {name}"
                )
            row["acceptance_fixture"] = fixture
        else:
            row.update(
                {
                    "status": "unsupported",
                    "reason": (
                        "Nazm has no read-only data section."
                        if name in {".rodata", ".rdata"}
                        else "Nazm does not emit this object section."
                    ),
                }
            )
        rendered.append(row)
    return rendered


def _classify_relocations(
    candidates: list[dict[str, Any]], capabilities: dict[str, Any]
) -> list[dict[str, Any]]:
    rendered: list[dict[str, Any]] = []
    for item in candidates:
        form = item["form"]
        row = {"form": form, "count": item["count"]}
        if form == "instruction:call:symbol":
            row.update(
                {
                    "status": "partial",
                    "reason": (
                        "Calls to labels defined in the same .text input are resolved, but "
                        "external-symbol call relocations are not implemented."
                    ),
                }
            )
        elif form == "data:.quad":
            row.update(
                {
                    "status": "unsupported",
                    "reason": "Nazm does not implement data-section symbol relocations.",
                }
            )
        elif "memory-rip-relative" in form:
            row.update(
                {
                    "status": "unsupported",
                    "reason": "Nazm does not implement RIP-relative relocations.",
                }
            )
        else:
            row.update(
                {
                    "status": "unsupported",
                    "reason": "This relocation-producing form is not implemented end to end.",
                }
            )
        rendered.append(row)
    return rendered


def _summary(*groups: list[dict[str, Any]]) -> dict[str, Any]:
    form_counts: Counter[str] = Counter()
    emission_counts: Counter[str] = Counter()
    for group in groups:
        for item in group:
            status = item["status"]
            form_counts[status] += 1
            emission_counts[status] += item.get("count", 0)
    return {
        "forms": {key: form_counts[key] for key in ("supported", "partial", "unsupported")},
        "emissions": {
            key: emission_counts[key]
            for key in ("supported", "partial", "unsupported")
        },
    }


def build_coverage(
    inventory: dict[str, Any],
    capabilities: dict[str, Any],
    capabilities_sha256: str,
) -> dict[str, Any]:
    targets: dict[str, Any] = {}
    inventory_targets = inventory.get("targets")
    if not isinstance(inventory_targets, dict) or not inventory_targets:
        raise ValueError("assembly inventory has no targets")

    for target_name in sorted(inventory_targets):
        source = inventory_targets[target_name]
        instructions = _classify_instructions(
            source.get("instructions", []), capabilities
        )
        directives = _classify_directives(source.get("directives", []), capabilities)
        sections = _classify_sections(source.get("sections", []), capabilities)
        relocations = _classify_relocations(
            source.get("relocation_candidates", []), capabilities
        )
        sources = source.get("sources", [])
        compiled_count = source.get("compiled_source_count", 0)
        failures = source.get("compile_failures", [])
        targets[target_name] = {
            "corpus": {
                "source_count": len(sources),
                "compiled_source_count": compiled_count,
                "omitted_source_count": len(sources) - compiled_count,
                "compile_failures": failures,
                "sources": sources,
            },
            "instruction_forms": instructions,
            "directive_forms": directives,
            "sections": sections,
            "symbols": source.get("symbols", {}),
            "relocation_candidates": relocations,
            "summary": _summary(instructions, directives, sections, relocations),
        }

    return {
        "schema": SCHEMA,
        "source_inventory": {
            "schema": inventory["schema"],
            "compiler": inventory.get("compiler", "unknown"),
        },
        "nazm_capabilities": {
            "schema": capabilities["schema"],
            "assembler": capabilities.get("assembler", "unknown"),
            "sha256": capabilities_sha256,
        },
        "status_contract": {
            "supported": "The complete inventory form is implemented by Nazm.",
            "partial": "Only a stated subset is implemented; this is not shadow-ready.",
            "unsupported": "Nazm must reject or Baa must report the form; no fallback is allowed.",
        },
        "acceptance_fixtures": capabilities.get("acceptance_fixtures", []),
        "targets": targets,
    }


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--nazm-capabilities", type=Path, default=DEFAULT_CAPABILITIES)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify that --output already contains the generated contract.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv or sys.argv[1:])
    try:
        inventory, _ = _read_json(args.inventory.resolve(), INVENTORY_SCHEMA)
        capabilities, capabilities_raw = _read_json(
            args.nazm_capabilities.resolve(), CAPABILITIES_SCHEMA
        )
        document = build_coverage(
            inventory,
            capabilities,
            hashlib.sha256(capabilities_raw).hexdigest(),
        )
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    generated = json.dumps(document, ensure_ascii=False, indent=2) + "\n"
    output = args.output.resolve()
    if args.check:
        try:
            current = output.read_text(encoding="utf-8")
        except OSError as exc:
            print(f"error: cannot read checked coverage {output}: {exc}", file=sys.stderr)
            return 1
        if current != generated:
            print(
                "error: checked Baa-to-Nazm coverage is stale; regenerate it without --check",
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
