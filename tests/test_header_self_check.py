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
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build" / "baa.exe",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class HeaderSelfCheckTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def run_baa(self, work: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.baa), *args],
            cwd=str(work),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )

    def test_valid_header_checks_without_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_header_check_ok_") as temp:
            work = Path(temp)
            (work / "api.baahd").write_text(
                "خارجي صحيح قيمة_مشتركة.\n"
                "خارجي صحيح اجمع(صحيح أ، صحيح ب).\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "--check-header", "api.baahd")
            outputs = list(work.glob("out*")) + list(work.glob("*.o")) + list(work.glob("*.s"))

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        self.assertEqual(outputs, [])

    def test_invalid_header_reports_existing_parser_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_header_check_bad_") as temp:
            work = Path(temp)
            (work / "bad.baahd").write_text("خارجي صحيح قيمة = ١.\n", encoding="utf-8")

            proc = self.run_baa(work, "--check-header", "bad.baahd")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertNotEqual(proc.returncode, 0, combined)
        self.assertIn("لا يقبل التصريح 'خارجي' تهيئة", combined)


if __name__ == "__main__":
    unittest.main()
