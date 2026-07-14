#!/usr/bin/env python3

from __future__ import annotations

import os
import re
import subprocess
import tempfile
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


class NazmEmitterTests(unittest.TestCase):
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

    @staticmethod
    def write_minimal_source(work: Path) -> Path:
        source = work / "برنامج.باء"
        source.write_text(
            "صحيح الرئيسية() {\n"
            "    إرجع ٠.\n"
            "}\n",
            encoding="utf-8",
        )
        return source

    def test_minimal_source_is_canonical_arabic_for_both_targets(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_emit_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)

            for target in ("x86_64-windows", "x86_64-linux"):
                with self.subTest(target=target):
                    output = work / f"خرج-{target}.نظم"
                    proc = self.run_baa(
                        work,
                        "--emit-nazm",
                        f"--target={target}",
                        str(source),
                        "-o",
                        str(output),
                    )
                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    text = output.read_text(encoding="utf-8")
                    self.assertIn(".عام الرئيسية", text)
                    self.assertIn("الرئيسية:", text)
                    self.assertIn("انقل سجل_المركم، ٠", text)
                    self.assertNotIn("main", text)
                    self.assertIsNone(
                        re.search(r"[A-Za-z]", text),
                        "Nazm source contains a Latin letter",
                    )

    def test_unsupported_form_is_visible_and_leaves_no_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_unsupported_") as temp:
            work = Path(temp)
            output = work / "غير-مدعوم.نظم"
            proc = self.run_baa(
                work,
                "--emit-nazm",
                str(ROOT / "examples" / "hello_world.baa"),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 3, proc.stderr)
            self.assertIn("غير مدعومة", proc.stderr)
            self.assertFalse(output.exists())

    def test_conflicting_output_modes_are_invalid_invocation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_conflict_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            proc = self.run_baa(work, "--emit-nazm", "-S", str(source))
            self.assertEqual(proc.returncode, 2, proc.stderr)


if __name__ == "__main__":
    unittest.main()
