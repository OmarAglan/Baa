#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "qa_run.py"
SPEC = importlib.util.spec_from_file_location("baa_qa_run", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
QA = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = QA
SPEC.loader.exec_module(QA)


class QaCompilerPreflightTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.previous_root = QA.ROOT
        QA.ROOT = self.root

    def tearDown(self) -> None:
        QA.ROOT = self.previous_root
        self.temp_dir.cleanup()

    def test_invalid_baa_override_does_not_fall_back(self) -> None:
        default_compiler = self.root / "build" / ("baa.exe" if os.name == "nt" else "baa")
        default_compiler.parent.mkdir(parents=True)
        default_compiler.write_bytes(b"placeholder")
        missing = self.root / "missing-baa"

        with mock.patch.dict(os.environ, {"BAA": str(missing)}):
            with self.assertRaisesRegex(FileNotFoundError, "BAA points to a missing"):
                QA._find_baa()

    def test_finds_verify_preset_compiler(self) -> None:
        if os.name == "nt":
            compiler = self.root / "build" / "presets" / "windows-verify" / "baa.exe"
        else:
            compiler = self.root / "build-linux" / "presets" / "verify" / "baa"
        compiler.parent.mkdir(parents=True)
        compiler.write_bytes(b"placeholder")

        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(QA._find_baa(), compiler)

    def test_missing_compiler_is_a_structured_failed_step(self) -> None:
        log_dir = self.root / ".baa_qa_logs"
        log_dir.mkdir()

        with mock.patch.dict(os.environ, {}, clear=True):
            compiler, result = QA._run_compiler_preflight(log_dir)

        self.assertIsNone(compiler)
        self.assertFalse(result.passed)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Could not find compiler binary", result.detail)
        stderr_log = self.root / result.stderr_log
        self.assertIn("Could not find compiler binary", stderr_log.read_text(encoding="utf-8"))

    def test_failed_preflight_is_written_to_summary_json(self) -> None:
        log_dir = self.root / ".baa_qa_logs"
        log_dir.mkdir()
        with mock.patch.dict(os.environ, {}, clear=True):
            _, result = QA._run_compiler_preflight(log_dir)
        summary_path = self.root / "qa-summary.json"

        with contextlib.redirect_stdout(io.StringIO()):
            returncode = QA._write_summary(
                "full",
                None,
                False,
                [result],
                log_dir,
                str(summary_path),
            )

        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        self.assertEqual(returncode, 1)
        self.assertFalse(summary["overall_passed"])
        self.assertEqual(summary["results"][0]["name"], "compiler-preflight")
        self.assertEqual(summary["results"][0]["returncode"], 2)


if __name__ == "__main__":
    unittest.main()
