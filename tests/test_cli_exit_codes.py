#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DRIVER_HEADER = ROOT / "src" / "driver" / "driver.h"


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


class CompilerCliExitCodeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()
        cls.cross_target = "x86_64-linux" if os.name == "nt" else "x86_64-windows"

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
    def write_valid_source(work: Path) -> Path:
        source = work / "main.baa"
        source.write_text("صحيح الرئيسية() {\n    إرجع ٠.\n}\n", encoding="utf-8")
        return source

    def test_success_is_zero(self) -> None:
        proc = self.run_baa(ROOT, "--version")
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_source_diagnostic_is_one_and_remains_structured(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_exit_source_") as temp:
            work = Path(temp)
            (work / "bad.baa").write_text(
                "صحيح الرئيسية() {\n    مفقود = ١.\n    إرجع ٠.\n}\n",
                encoding="utf-8",
            )
            proc = self.run_baa(work, "--check", "--diagnostics=json", "bad.baa")

        self.assertEqual(proc.returncode, 1, proc.stderr)
        data = json.loads(proc.stdout)
        self.assertEqual(data["schema_version"], "diagnostics-json-v1")
        self.assertTrue(data["diagnostics"])

    def test_invalid_invocations_are_two(self) -> None:
        no_args = self.run_baa(ROOT)
        unknown = self.run_baa(ROOT, "--not-a-baa-option")
        self.assertEqual(no_args.returncode, 2, no_args.stderr)
        self.assertEqual(unknown.returncode, 2, unknown.stderr)

    def test_unsupported_cross_target_link_is_three(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_exit_unsupported_") as temp:
            work = Path(temp)
            self.write_valid_source(work)
            proc = self.run_baa(work, f"--target={self.cross_target}", "main.baa")

        self.assertEqual(proc.returncode, 3, f"{proc.stdout}\n{proc.stderr}")

    def test_toolchain_output_failure_is_four(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_exit_toolchain_") as temp:
            work = Path(temp)
            self.write_valid_source(work)
            suffix = ".exe" if os.name == "nt" else ""
            impossible_output = work / "missing-parent" / f"program{suffix}"
            proc = self.run_baa(work, "main.baa", "-o", str(impossible_output))

        self.assertEqual(proc.returncode, 4, f"{proc.stdout}\n{proc.stderr}")

    def test_internal_failure_code_is_fixed_at_five(self) -> None:
        header = DRIVER_HEADER.read_text(encoding="utf-8")
        match = re.search(r"BAA_COMPILER_EXIT_INTERNAL_ERROR\s*=\s*(\d+)", header)
        self.assertIsNotNone(match, "driver exit-code enum is missing the internal failure member")
        self.assertEqual(int(match.group(1)), 5)


if __name__ == "__main__":
    unittest.main()
