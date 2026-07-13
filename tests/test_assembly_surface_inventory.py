from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "inventory_assembly_surface.py"
SPEC = importlib.util.spec_from_file_location("inventory_assembly_surface", SCRIPT)
assert SPEC and SPEC.loader
INVENTORY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INVENTORY)


class AssemblySurfaceInventoryTests(unittest.TestCase):
    def test_extracts_deterministic_surface_forms(self) -> None:
        fixture = """\
.text
.globl main
main:
    movq $42, %rax
    movq item(%rip), %rcx
    call printf
    ret
.data
item: .quad main
.section .note.GNU-stack,"",@progbits
"""
        with tempfile.TemporaryDirectory(prefix="baa-inventory-test-") as temp:
            path = Path(temp) / "fixture.s"
            path.write_text(fixture, encoding="utf-8")
            result = INVENTORY.inspect_assembly([path])

        forms = {
            (item["mnemonic"], tuple(item["operands"])) for item in result["instructions"]
        }
        self.assertIn(("movq", ("immediate-integer", "register")), forms)
        self.assertIn(("movq", ("memory-rip-relative", "register")), forms)
        self.assertIn(("call", ("symbol",)), forms)
        self.assertEqual([item["name"] for item in result["sections"]], [
            ".data",
            ".note.GNU-stack",
            ".text",
        ])
        self.assertEqual(result["symbols"]["defined"], 2)
        self.assertTrue(any(item["form"].startswith("instruction:call") for item in result["relocation_candidates"]))
        self.assertTrue(any(item["form"] == "data:.quad" for item in result["relocation_candidates"]))

    def test_cli_writes_utf8_lf_json(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa-inventory-cli-") as temp:
            root = Path(temp)
            source = root / "عينة.s"
            output = root / "inventory.json"
            source.write_text(".text\nوسم:\n    ret\n", encoding="utf-8")
            exit_code = INVENTORY.main([
                "--assembly",
                str(source),
                "--output",
                str(output),
            ])
            self.assertEqual(exit_code, 0)
            raw = output.read_bytes()
            self.assertNotIn(b"\r\n", raw)
            payload = json.loads(raw.decode("utf-8"))
            self.assertEqual(payload["schema"], "baa-assembly-surface-v1")
            self.assertEqual(payload["assembly"]["instructions"][0]["mnemonic"], "ret")

    def test_reads_quoted_corpus_flags(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa-inventory-flags-") as temp:
            source = Path(temp) / "fixture.baa"
            source.write_text(
                '// RUN: expect-pass runtime\n// FLAGS: -I "fixtures/path with spaces" -O1\n\n',
                encoding="utf-8",
            )
            self.assertEqual(
                INVENTORY._source_flags(source),
                ["-I", "fixtures/path with spaces", "-O1"],
            )

    def test_check_rejects_stale_inventory(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa-inventory-check-") as temp:
            root = Path(temp)
            source = root / "fixture.s"
            output = root / "inventory.json"
            source.write_text(".text\n    ret\n", encoding="utf-8")
            args = ["--assembly", str(source), "--output", str(output)]
            self.assertEqual(INVENTORY.main(args), 0)
            self.assertEqual(INVENTORY.main([*args, "--check"]), 0)
            output.write_text("{}\n", encoding="utf-8")
            self.assertEqual(INVENTORY.main([*args, "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
