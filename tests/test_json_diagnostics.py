#!/usr/bin/env python3

from __future__ import annotations

import json
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


class JsonDiagnosticsTests(unittest.TestCase):
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

    def test_clean_check_emits_empty_diagnostics_object(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_json_diag_clean_") as temp:
            work = Path(temp)
            (work / "main.baa").write_text(
                "صحيح الرئيسية() {\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "--check", "--diagnostics=json", "main.baa")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        data = json.loads(proc.stdout)
        self.assertEqual(data["schema_version"], "diagnostics-json-v1")
        self.assertEqual(data["invocation"]["mode"], "check")
        self.assertEqual(data["summary"], {"errors": 0, "warnings": 0, "notes": 0})
        self.assertEqual(data["diagnostics"], [])

    def test_semantic_error_reports_json_span_code_and_hint(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_json_diag_semantic_") as temp:
            work = Path(temp)
            (work / "bad.baa").write_text(
                "صحيح الرئيسية() {\n"
                "    س = ١.\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "--check", "--diagnostics=json", "bad.baa")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertNotEqual(proc.returncode, 0, combined)
        data = json.loads(proc.stdout)
        self.assertEqual(data["summary"]["errors"], 1)
        diag = data["diagnostics"][0]
        self.assertEqual(diag["code"], "B1000")
        self.assertEqual(diag["severity"], "error")
        self.assertEqual(diag["category"], "semantic")
        self.assertTrue(diag["file"].endswith("bad.baa"), diag)
        self.assertGreaterEqual(diag["line"], 1)
        self.assertGreaterEqual(diag["column"], 1)
        self.assertIn("span", diag)
        self.assertIn("start", diag["span"])
        self.assertIn("end", diag["span"])
        self.assertIsInstance(diag.get("hint"), str)
        self.assertGreater(len(diag.get("hints", [])), 0)
        self.assertNotIn("[Error]", proc.stderr)

    def test_warning_reports_json_without_failing_check(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_json_diag_warning_") as temp:
            work = Path(temp)
            (work / "warn.baa").write_text(
                "صحيح الرئيسية() {\n"
                "    صحيح س = ١.\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "--check", "--diagnostics=json", "-Wall", "warn.baa")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        data = json.loads(proc.stdout)
        self.assertEqual(data["summary"]["warnings"], 1)
        diag = data["diagnostics"][0]
        self.assertEqual(diag["code"], "B1100")
        self.assertEqual(diag["severity"], "warning")
        self.assertEqual(diag["category"], "warning")
        self.assertEqual(diag["hint"], None)

    def test_unexpected_identifier_statement_recovers_without_looping(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_json_diag_recovery_") as temp:
            work = Path(temp)
            (work / "bad.baa").write_text(
                "صحيح الرئيسية() {\n"
                "    أرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "--check", "--diagnostics=json", "bad.baa")

        self.assertNotEqual(proc.returncode, 0, proc.stdout)
        data = json.loads(proc.stdout)
        self.assertEqual(data["summary"]["errors"], 1)
        self.assertEqual(len(data["diagnostics"]), 1)
        self.assertEqual(data["diagnostics"][0]["code"], "B0001")


if __name__ == "__main__":
    unittest.main()
