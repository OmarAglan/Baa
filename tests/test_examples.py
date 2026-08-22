#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples"


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


class PublicExamplesCompileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()
        cls.examples = sorted(EXAMPLES.glob("*.باء"))
        if not cls.examples:
            raise AssertionError("No public examples found under examples/*.باء")

    def test_public_examples_compile_with_verify(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_examples_") as temp:
            out_root = Path(temp)
            for example in self.examples:
                ext = ".exe" if os.name == "nt" else ""
                out = out_root / f"{example.stem}{ext}"
                with self.subTest(example=example.name):
                    proc = subprocess.run(
                        [str(self.baa), "-O2", "--verify", str(example), "-o", str(out)],
                        cwd=str(ROOT),
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                    )
                    combined = f"{proc.stdout}\n{proc.stderr}"
                    self.assertEqual(proc.returncode, 0, combined)
                    self.assertTrue(out.exists(), f"missing output for {example.name}")


if __name__ == "__main__":
    unittest.main()
