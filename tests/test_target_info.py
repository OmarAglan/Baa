#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        candidate = Path(env)
        if candidate.is_file():
            return candidate

    candidates = [
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build" / "baa.exe",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class TargetInfoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()
        cls.host_name = "x86_64-windows" if os.name == "nt" else "x86_64-linux"

    def run_baa(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.baa), *args],
            cwd=str(ROOT),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )

    def test_default_query_describes_host_and_all_supported_targets(self) -> None:
        proc = self.run_baa("--target-info=json")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stderr, "")

        data = json.loads(proc.stdout)
        self.assertEqual(data["schema_version"], "target-info-v1")
        self.assertIsInstance(data["compiler_version"], str)
        self.assertEqual(data["host_target"], self.host_name)
        self.assertEqual(data["selected_target"], self.host_name)

        targets = {target["name"]: target for target in data["targets"]}
        self.assertEqual(set(targets), {"x86_64-windows", "x86_64-linux"})
        self.assertEqual(targets["x86_64-windows"]["object_format"], "coff")
        self.assertEqual(targets["x86_64-windows"]["executable_suffix"], ".exe")
        self.assertEqual(targets["x86_64-linux"]["object_format"], "elf")
        self.assertEqual(targets["x86_64-linux"]["executable_suffix"], "")

        host = targets[self.host_name]
        self.assertTrue(host["is_host"])
        self.assertTrue(host["capabilities"]["object"])
        self.assertTrue(host["capabilities"]["link"])

    def test_target_selection_does_not_change_host_capabilities(self) -> None:
        selected = "x86_64-linux" if self.host_name.endswith("windows") else "x86_64-windows"
        proc = self.run_baa(f"--target={selected}", "--target-info=json")
        self.assertEqual(proc.returncode, 0, proc.stderr)

        data = json.loads(proc.stdout)
        self.assertEqual(data["host_target"], self.host_name)
        self.assertEqual(data["selected_target"], selected)
        target = next(item for item in data["targets"] if item["name"] == selected)
        self.assertFalse(target["is_host"])
        self.assertFalse(target["capabilities"]["object"])
        self.assertFalse(target["capabilities"]["link"])
        self.assertTrue(target["capabilities"]["assembly"])

    def test_unknown_target_info_format_is_rejected(self) -> None:
        proc = self.run_baa("--target-info=text")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("expected json", proc.stderr)


if __name__ == "__main__":
    unittest.main()
