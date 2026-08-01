#!/usr/bin/env python3

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

REQUIRED_DOCS = {
    "docs/ECOSYSTEM_BOUNDARIES.md": ("# Baa Ecosystem Boundaries", "draft-0.1"),
    "docs/COMPATIBILITY_MATRIX.md": ("# Baa Ecosystem Compatibility Matrix", "draft-0.1"),
    "docs/TOOLING_CONTRACTS.md": ("# Baa Tooling Contracts", "draft-0.1"),
    "docs/DIAGNOSTICS_JSON_SCHEMA.md": ("# Baa Diagnostics JSON Schema", "draft-0.1"),
    "docs/FORMAT_JSON_SCHEMA.md": ("# Baa Formatting JSON Schema", "draft-0.1"),
    "docs/STRUCTURE_JSON_SCHEMA.md": ("# Baa Structure JSON Schema", "draft-0.1"),
    "docs/CONFORMANCE_SUITE.md": ("# Baa Conformance Suite", "draft-0.1"),
    "docs/SDK_RELEASE_PLAN.md": ("# Baa SDK Release Plan", "draft-0.1"),
    "docs/NAZM_PRODUCTION_ADMISSION.md": (
        "# Baa/Nazm Production Admission and Rollback",
        "1.0",
    ),
}


def _read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


class IntegrationArtifactDocumentationTests(unittest.TestCase):
    def test_required_integration_docs_exist_with_expected_titles(self) -> None:
        for relative, (title, version) in REQUIRED_DOCS.items():
            with self.subTest(doc=relative):
                path = ROOT / relative
                self.assertTrue(path.is_file(), f"missing required integration doc: {relative}")
                text = path.read_text(encoding="utf-8")
                self.assertIn(title, text)
                self.assertIn(f"> **Version:** {version}", text)

    def test_ecosystem_boundaries_lock_project_ownership(self) -> None:
        text = _read("docs/ECOSYSTEM_BOUNDARIES.md")
        for marker in (
            "| Baa | Arabic-first systems language and reference compiler |",
            "| Takween | Arabic-first build workflow for Baa projects |",
            "| Qalam-IDE | Arabic/RTL IDE and editor experience |",
            "| PyramidOS | Future freestanding/OS-development consumer and testbed |",
            "Do not add `baa build` while Takween owns project build UX.",
            "Do not add editor/IDE roadmap items to Baa while Qalam owns IDE UX.",
            "Qalam-facing roadmap entries in Baa must stay limited to compiler/data contracts",
            "Baa does not own editor widgets, IDE commands, extension packaging, themes, layout, or UI flows.",
            "Do not add PyramidOS kernel migration tasks to Baa before the freestanding profile exists.",
            "Do not add production self-hosting work before v0.9 stable beta freeze.",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)

    def test_compatibility_matrix_names_shared_contracts(self) -> None:
        text = _read("docs/COMPATIBILITY_MATRIX.md")
        for contract in (
            "compiler-cli-v1",
            "build-manifest-v1",
            "diagnostics-json-v1",
            "symbols-json-v1",
            "format-json-v1",
            "tokens-json-v1",
            "structure-json-v1",
            "target-spec-v1",
            "conformance-v1",
            "freestanding-v0",
        ):
            with self.subTest(contract=contract):
                self.assertIn(f"`{contract}`", text)

        for release_row in ("| v0.7.0 |", "| v0.7.2 |", "| v0.8.x |", "| v0.9.0 |"):
            with self.subTest(release_row=release_row):
                self.assertIn(release_row, text)

    def test_tooling_contracts_cover_external_cli_surfaces(self) -> None:
        text = _read("docs/TOOLING_CONTRACTS.md")
        for surface in (
            "`--check`",
            "`--emit-build-manifest <file>`",
            "`--incremental`",
            "`--cache-dir <dir>`",
            "`--diagnostics=json`",
            "`--dump-tokens=json`",
            "`--dump-structure=json`",
            "`--dump-symbols=json`",
            "`--format=json`",
            "`--target=<target>`",
            "`--target-info=json`",
            "`-I <dir>` / `-I<dir>`",
            "`schema_version`",
            "Exit-code meanings are part of `compiler-cli-v1`.",
            "must preserve the numeric status",
        ):
            with self.subTest(surface=surface):
                self.assertIn(surface, text)

        for code in ("| 0 | success |", "| 1 | user/source error:", "| 5 | internal compiler error |"):
            with self.subTest(code=code):
                self.assertIn(code, text)

    def test_takween_contracts_cover_invocation_manifest_and_dependency_invalidation(self) -> None:
        text = _read("docs/TOOLING_CONTRACTS.md")
        for marker in (
            "## 3. Takween Invocation Contract",
            "baa --check [-I <dir>...] [--target=<target>] <inputs...>",
            "baa --incremental --cache-dir <dir> --emit-build-manifest <file> <inputs...> -o <output>",
            "baa [--target=<target>] --target-info=json",
            "`compiler-cli-v1` does not include `baa build`, `baa run`, or `baa clean`.",
            '"schema": 1',
            '"compiler_version": "0.6.0"',
            '"assembler": "nazm"',
            '"source_kind": "baa"',
            '"runtime_check_mask": 0',
            '"units": [',
            '"cache": {',
            '"dependencies": [',
            "## 6. Include and Dependency Contract",
            "The manifest `units[].dependencies[]` list is the canonical invalidation surface for Takween.",
            "ordered `-I` include directory list",
            "output kind when switching between link, `-c`, and `-S`",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)

    def test_diagnostics_json_schema_covers_tooling_fields_and_codes(self) -> None:
        text = _read("docs/DIAGNOSTICS_JSON_SCHEMA.md")
        for marker in (
            '"schema_version": "diagnostics-json-v1"',
            '"code": "B0001"',
            '"severity": "error"',
            '"category": "syntax"',
            '"span": {',
            "| `B0001`-`B0999` | lexer/syntax/parser |",
            "| `B1000`-`B1999` | semantic/type/scope |",
            "| `B9000`-`B9999` | internal compiler errors |",
            "`--diagnostics=json` emits the same codes",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)

    def test_format_json_schema_covers_canonical_editor_contract(self) -> None:
        text = _read("docs/FORMAT_JSON_SCHEMA.md")
        for marker in (
            '"schema_version": "format-json-v1"',
            '"position_encoding": "utf-8-bytes"',
            '"line_ending": "lf"',
            '"indent_width": 4',
            '"insert_spaces": true',
            '"formatted_text":',
            "accepts incomplete editor buffers",
            "is idempotent",
            "full-document UTF-16 `TextEdit`",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)

    def test_structure_json_schema_covers_structural_editor_contract(self) -> None:
        text = _read("docs/STRUCTURE_JSON_SCHEMA.md")
        for marker in (
            '"schema_version": "structure-json-v1"',
            '"position_encoding": "utf-8-bytes"',
            '"complete": true',
            '"folding_ranges": [',
            '"selection_ranges": [',
            "`region`: a matched multiline",
            "`token`: one raw source token.",
            "textDocument/selectionRange",
            "must not parse Baa source",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)

    def test_conformance_suite_plan_covers_categories_and_release_gate(self) -> None:
        text = _read("docs/CONFORMANCE_SUITE.md")
        for category in (
            "| syntax | accepted/rejected grammar |",
            "| semantics | type, scope, const, pointer, aggregate rules |",
            "| diagnostics | stable diagnostic IDs, spans, hints, cascade control |",
            "| stdlib | string, memory, file, math, error behavior |",
            "| abi | calls, returns, varargs, stack args, struct layout |",
            "| targets | hosted/freestanding target behavior |",
            "| runtime | optional runtime checks and panic behavior |",
            "| negative | intentionally invalid programs |",
        ):
            with self.subTest(category=category):
                self.assertIn(category, text)

        for gate in (
            "conformance tests pass on Windows and Linux",
            "`-O0` and `-O2` agree where applicable",
            "diagnostic tests pass with stable codes",
            "ABI tests pass for supported targets",
        ):
            with self.subTest(gate=gate):
                self.assertIn(gate, text)

    def test_sdk_release_plan_covers_bundle_layout_and_profiles(self) -> None:
        text = _read("docs/SDK_RELEASE_PLAN.md")
        for marker in (
            "baa-sdk-0.9.0/",
            "targets/",
            "schemas/",
            "diagnostics-json-v1.schema.json",
            "| minimal | compiler + stdlib + target specs |",
            "| default | minimal + docs + examples + Takween |",
            "| full | default + Qalam metadata and optional IDE link |",
            "| ci | compiler + stdlib + target specs, no local docs |",
            "Do not build a public package registry until:",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)

    def test_nazm_production_admission_records_receipts_and_rollback(self) -> None:
        text = _read("docs/NAZM_PRODUCTION_ADMISSION.md")
        for marker in (
            "`baa-nazm-production-admission-v1`",
            "**Decision:** APPROVED",
            "## 3. Parity Surface",
            "## 4. Candidate Receipts",
            "## 6. Rollback Procedure",
            "no failed Nazm invocation retries through GAS",
            "`--assembler=gas`",
            "- [x] Green Nazm exact-revision CI.",
            "- [x] Terminal green Baa exact-revision CI receipt.",
            "- [x] Hosted quick/full/stress/release receipts on Windows and Linux.",
            "29685356936",
            "29685512987",
            "29687846586",
            "29689709002",
            "| Linux PIC/PIE producer contract | PASS |",
            "| Direct Unicode artifact pipeline | PASS |",
            "| Explicit GAS rollback drill | PASS |",
            "| Baa compiler | approved |",
            "| Nazm assembler | approved |",
            "| Takween consumer | approved |",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, text)


if __name__ == "__main__":
    unittest.main()
