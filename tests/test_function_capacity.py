#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_FUNCTION_CAPACITY = 192


def _find_baa() -> Path:
    configured = os.environ.get("BAA")
    if configured:
        candidate = Path(configured)
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


class FunctionCapacityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def test_reference_compiler_accepts_large_tooling_module(self) -> None:
        declarations = [
            f"صحيح دالة_{index}() {{ إرجع ٠. }}"
            for index in range(EXPECTED_FUNCTION_CAPACITY)
        ]
        source_text = "\n".join(
            [*declarations, "صحيح الرئيسية() { إرجع دالة_191(). }", ""]
        )

        with tempfile.TemporaryDirectory(prefix="baa_function_capacity_") as temp:
            work = Path(temp)
            source = work / "tooling_module.baa"
            source.write_text(source_text, encoding="utf-8")
            proc = subprocess.run(
                [str(self.baa), "--check", str(source)],
                cwd=str(work),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=60,
            )

        self.assertEqual(proc.returncode, 0, f"{proc.stdout}\n{proc.stderr}")


if __name__ == "__main__":
    unittest.main()
