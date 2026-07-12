#!/usr/bin/env python3

from __future__ import annotations

import json
import unittest
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
TARGETS = ROOT / "targets"
TARGET_SPEC_DOC = ROOT / "docs" / "TARGET_SPECIFICATION.md"

SCHEMA_VERSION = "target-spec-v1"

REQUIRED_TARGET_FILES = {
    "x86_64-linux.json",
    "x86_64-windows.json",
    "i386-elf.experimental.json",
    "i386-pyramidos.experimental.json",
}

COMMON_REQUIRED_FIELDS = {
    "schema_version",
    "name",
    "arch",
    "bits",
    "environment",
    "object_format",
    "executable_suffix",
    "assembly_syntax",
    "pointer_width",
    "endianness",
    "calling_convention",
    "stack_alignment",
    "supports_libc",
    "supports_stdlib",
    "default_linker",
    "features",
}


def _load_target_specs() -> dict[str, dict[str, Any]]:
    specs: dict[str, dict[str, Any]] = {}
    for path in sorted(TARGETS.glob("*.json")):
        with path.open("r", encoding="utf-8") as f:
            specs[path.name] = json.load(f)
    return specs


def _is_positive_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and value > 0


class TargetSpecificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.specs = _load_target_specs()

    def test_required_target_descriptor_files_exist(self) -> None:
        missing = REQUIRED_TARGET_FILES.difference(self.specs)
        detail = "missing required target descriptor(s): " + ", ".join(sorted(missing))
        self.assertFalse(missing, detail)

    def test_all_descriptors_follow_common_schema(self) -> None:
        self.assertTrue(self.specs, "no target descriptors found under targets/*.json")
        for file_name, spec in self.specs.items():
            with self.subTest(target=file_name):
                missing = COMMON_REQUIRED_FIELDS.difference(spec)
                self.assertFalse(missing, f"{file_name} missing field(s): {sorted(missing)}")
                self.assertEqual(spec["schema_version"], SCHEMA_VERSION)

                expected_name = file_name.removesuffix(".json").removesuffix(".experimental")
                self.assertEqual(spec["name"], expected_name)

                self.assertIsInstance(spec["name"], str)
                self.assertIsInstance(spec["arch"], str)
                self.assertTrue(_is_positive_int(spec["bits"]))
                self.assertIn(spec["environment"], {"hosted", "freestanding"})
                self.assertIn(spec["object_format"], {"coff", "elf"})
                self.assertIsInstance(spec["executable_suffix"], str)
                self.assertEqual(spec["assembly_syntax"], "gas-att")
                self.assertTrue(_is_positive_int(spec["pointer_width"]))
                self.assertIn(spec["endianness"], {"little", "big"})
                self.assertIsInstance(spec["calling_convention"], str)
                self.assertTrue(_is_positive_int(spec["stack_alignment"]))
                self.assertIsInstance(spec["supports_libc"], bool)
                self.assertIsInstance(spec["supports_stdlib"], bool)
                self.assertIsInstance(spec["default_linker"], str)
                self.assertIsInstance(spec["features"], dict)
                self.assertIn("freestanding", spec["features"])
                self.assertIsInstance(spec["features"]["freestanding"], bool)

    def test_hosted_x86_64_targets_match_current_backend_contract(self) -> None:
        expected = {
            "x86_64-linux.json": {
                "object_format": "elf",
                "calling_convention": "sysv-amd64",
                "default_linker": "host-gcc",
            },
            "x86_64-windows.json": {
                "object_format": "coff",
                "calling_convention": "windows-x64",
                "default_linker": "mingw-gcc",
            },
        }

        for file_name, required in expected.items():
            spec = self.specs[file_name]
            with self.subTest(target=file_name):
                self.assertEqual(spec["arch"], "x86_64")
                self.assertEqual(spec["bits"], 64)
                self.assertEqual(spec["pointer_width"], 64)
                self.assertEqual(spec["environment"], "hosted")
                self.assertTrue(spec["supports_libc"])
                self.assertTrue(spec["supports_stdlib"])
                self.assertFalse(spec["features"]["freestanding"])
                self.assertEqual(spec["stack_alignment"], 16)
                for key, value in required.items():
                    self.assertEqual(spec[key], value)

        windows_features = self.specs["x86_64-windows.json"]["features"]
        self.assertEqual(windows_features["shadow_space_bytes"], 32)
        self.assertEqual(self.specs["x86_64-windows.json"]["executable_suffix"], ".exe")
        self.assertEqual(self.specs["x86_64-linux.json"]["executable_suffix"], "")

    def test_i386_targets_remain_experimental_and_freestanding(self) -> None:
        for file_name in ("i386-elf.experimental.json", "i386-pyramidos.experimental.json"):
            spec = self.specs[file_name]
            with self.subTest(target=file_name):
                self.assertEqual(spec["status"], "experimental-post-v0.9")
                self.assertEqual(spec["arch"], "i386")
                self.assertEqual(spec["bits"], 32)
                self.assertEqual(spec["pointer_width"], 32)
                self.assertEqual(spec["environment"], "freestanding")
                self.assertEqual(spec["object_format"], "elf")
                self.assertEqual(spec["calling_convention"], "cdecl-i386")
                self.assertFalse(spec["supports_libc"])
                self.assertFalse(spec["supports_stdlib"])
                self.assertTrue(spec["features"]["freestanding"])
                self.assertTrue(spec["features"]["volatile"])
                self.assertTrue(spec["features"]["packed_structs"])
                self.assertTrue(spec["features"]["custom_sections"])

    def test_target_spec_document_covers_schema_and_seed_targets(self) -> None:
        text = TARGET_SPEC_DOC.read_text(encoding="utf-8")
        self.assertIn(SCHEMA_VERSION, text)
        for target_name in ("x86_64-linux", "x86_64-windows", "i386-elf", "i386-pyramidos"):
            with self.subTest(target=target_name):
                self.assertIn(target_name, text)
        for field in COMMON_REQUIRED_FIELDS:
            with self.subTest(field=field):
                self.assertIn(f"`{field}`", text)


if __name__ == "__main__":
    unittest.main()
