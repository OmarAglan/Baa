#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = [
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build-linux" / "baa",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class ArabicNumeralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def test_dump_ir_uses_arabic_numerals_for_registers_and_constants(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_arabic_numerals_") as temp:
            root = Path(temp)
            src = root / "main.baa"
            src.write_text(
                "صحيح الرئيسية() {\n"
                "    صحيح س = ١٢ + ٣.\n"
                "    إرجع س.\n"
                "}\n",
                encoding="utf-8",
            )
            out = root / ("out.exe" if os.name == "nt" else "out")
            proc = subprocess.run(
                [str(self.baa), "--dump-ir", "main.baa", "-o", str(out)],
                cwd=str(root),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=20,
            )

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        self.assertIn("%م٠", proc.stdout)
        self.assertIn("%م١", proc.stdout)
        self.assertIn("خزن ص٦٤ ١٥", proc.stdout)
        self.assertNotIn("خزن ص٦٤ 15", proc.stdout)


if __name__ == "__main__":
    unittest.main()
