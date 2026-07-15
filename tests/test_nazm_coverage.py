from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "generate_nazm_coverage.py"
INVENTORY_PATH = ROOT / "docs" / "generated" / "assembly_surface_v1.json"
COVERAGE_PATH = ROOT / "docs" / "generated" / "baa_nazm_coverage_v1.json"
CAPABILITIES_PATH = (
    ROOT.parent / "Nazm" / "Docs" / "generated" / "nazm_capabilities_v1.json"
)
SPEC = importlib.util.spec_from_file_location("generate_nazm_coverage", SCRIPT)
assert SPEC and SPEC.loader
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


class NazmCoverageTests(unittest.TestCase):
    def setUp(self) -> None:
        self.inventory = json.loads(INVENTORY_PATH.read_text(encoding="utf-8"))
        self.coverage = json.loads(COVERAGE_PATH.read_text(encoding="utf-8"))

    def test_versioned_contract_covers_every_inventory_form(self) -> None:
        self.assertEqual(self.coverage["schema"], "baa-nazm-coverage-v1")
        self.assertEqual(
            set(self.coverage["targets"]),
            {"x86_64-linux", "x86_64-windows"},
        )

        for target, inventory_target in self.inventory["targets"].items():
            target_coverage = self.coverage["targets"][target]
            corpus = target_coverage["corpus"]
            self.assertEqual(corpus["source_count"], 100)
            self.assertEqual(corpus["compiled_source_count"], 100)
            self.assertEqual(corpus["omitted_source_count"], 0)
            self.assertEqual(corpus["compile_failures"], [])
            self.assertEqual(corpus["sources"], inventory_target["sources"])

            expected_instructions = {
                (item["mnemonic"], tuple(item["operands"]), item["count"])
                for item in inventory_target["instructions"]
            }
            actual_instructions = {
                (item["mnemonic"], tuple(item["operands"]), item["count"])
                for item in target_coverage["instruction_forms"]
            }
            self.assertEqual(actual_instructions, expected_instructions)

            expected_directives = {
                (item["directive"], tuple(item["operands"]), item["count"])
                for item in inventory_target["directives"]
            }
            actual_directives = {
                (item["directive"], tuple(item["operands"]), item["count"])
                for item in target_coverage["directive_forms"]
            }
            self.assertEqual(actual_directives, expected_directives)

            rows = (
                target_coverage["instruction_forms"]
                + target_coverage["directive_forms"]
                + target_coverage["sections"]
                + target_coverage["relocation_candidates"]
            )
            self.assertTrue(rows)
            self.assertTrue(all(
                item["status"] in {"supported", "partial", "unsupported"}
                for item in rows
            ))
            self.assertTrue(any(item["status"] == "supported" for item in rows))
            self.assertTrue(any(item["status"] == "unsupported" for item in rows))
            self.assertTrue(all(
                item.get("reason")
                for item in rows
                if item["status"] != "supported"
            ))
            self.assertTrue(all(
                item.get("acceptance_fixture")
                for item in rows
                if item["status"] == "supported"
            ))

    def test_contract_preserves_arabic_source_and_backend_limits(self) -> None:
        linux = self.coverage["targets"]["x86_64-linux"]
        instruction_index = {
            (item["mnemonic"], tuple(item["operands"])): item
            for item in linux["instruction_forms"]
        }
        self.assertEqual(
            instruction_index[("call", ("symbol",))]["status"],
            "partial",
        )
        self.assertEqual(
            instruction_index[("leaq", ("memory-rip-relative", "register"))][
                "status"
            ],
            "unsupported",
        )
        self.assertEqual(
            instruction_index[("movq", ("immediate-integer", "register"))][
                "status"
            ],
            "supported",
        )
        self.assertEqual(
            instruction_index[("sete", ("register",))]["status"],
            "supported",
        )
        directive_index = {
            (item["directive"], tuple(item["operands"])): item
            for item in linux["directive_forms"]
        }
        self.assertEqual(
            directive_index[(".globl", ("symbol",))]["status"],
            "partial",
        )
        self.assertIn(
            "الرئيسية",
            directive_index[(".globl", ("symbol",))]["reason"],
        )

    @unittest.skipUnless(
        CAPABILITIES_PATH.is_file(),
        "sibling Nazm capability contract is unavailable in standalone Baa checkout",
    )
    def test_versioned_contract_is_fresh_against_sibling_nazm(self) -> None:
        exit_code = COVERAGE.main([
            "--inventory",
            str(INVENTORY_PATH),
            "--nazm-capabilities",
            str(CAPABILITIES_PATH),
            "--output",
            str(COVERAGE_PATH),
            "--check",
        ])
        self.assertEqual(exit_code, 0)

    @unittest.skipUnless(
        CAPABILITIES_PATH.is_file(),
        "sibling Nazm capability contract is unavailable in standalone Baa checkout",
    )
    def test_cli_rejects_stale_contract(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa-nazm-coverage-") as temp:
            output = Path(temp) / "coverage.json"
            args = [
                "--inventory",
                str(INVENTORY_PATH),
                "--nazm-capabilities",
                str(CAPABILITIES_PATH),
                "--output",
                str(output),
            ]
            self.assertEqual(COVERAGE.main(args), 0)
            self.assertEqual(COVERAGE.main([*args, "--check"]), 0)
            output.write_text("{}\n", encoding="utf-8")
            self.assertEqual(COVERAGE.main([*args, "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
