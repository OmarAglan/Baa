#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "test_determinism.py"
SPEC = importlib.util.spec_from_file_location("baa_test_determinism", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
DETERMINISM = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = DETERMINISM
SPEC.loader.exec_module(DETERMINISM)


def _process(returncode: int, stdout: str = "", stderr: str = "") -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(
        args=["baa"],
        returncode=returncode,
        stdout=stdout,
        stderr=stderr,
    )


class DiagnosticDeterminismTests(unittest.TestCase):
    def test_accepts_identical_failure_output_with_normalized_line_endings(self) -> None:
        runs = [
            _process(1, stderr="خطأ\r\nسطر\r\n"),
            _process(1, stderr="خطأ\nسطر\n"),
        ]

        with mock.patch.object(DETERMINISM, "_run", side_effect=runs):
            result = DETERMINISM._compare_failure_output("diagnostic", ["baa", "bad.baa"])

        self.assertTrue(result.passed)
        self.assertIn("stable failure", result.detail)

    def test_rejects_diagnostic_text_drift(self) -> None:
        runs = [
            _process(1, stderr="الخطأ الأول\n"),
            _process(1, stderr="الخطأ الثاني\n"),
        ]

        with mock.patch.object(DETERMINISM, "_run", side_effect=runs):
            result = DETERMINISM._compare_failure_output("diagnostic", ["baa", "bad.baa"])

        self.assertFalse(result.passed)
        self.assertIn("diagnostic output differed", result.detail)

    def test_rejects_unexpected_success(self) -> None:
        with mock.patch.object(DETERMINISM, "_run", return_value=_process(0)):
            result = DETERMINISM._compare_failure_output("diagnostic", ["baa", "bad.baa"])

        self.assertFalse(result.passed)
        self.assertIn("unexpectedly succeeded", result.detail)

    def test_rejects_exit_code_drift(self) -> None:
        runs = [
            _process(1, stderr="خطأ\n"),
            _process(2, stderr="خطأ\n"),
        ]

        with mock.patch.object(DETERMINISM, "_run", side_effect=runs):
            result = DETERMINISM._compare_failure_output("diagnostic", ["baa", "bad.baa"])

        self.assertFalse(result.passed)
        self.assertIn("exit code differed", result.detail)

    def test_manifest_receipt_keeps_stability_and_shape_results(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            out_dir = Path(temp_dir)
            manifest = out_dir / "manifest.json"
            manifest.write_text(
                json.dumps(
                    {
                        "compiler_version": "0.5.9",
                        "target": "x86_64-linux",
                        "mode": "link",
                        "opt_level": 2,
                        "runtime_checks": False,
                        "units": [],
                    }
                ),
                encoding="utf-8",
            )
            stability = DETERMINISM.CheckResult(
                name="manifest-byte-stability",
                passed=True,
                detail="stable output file",
                duration_s=0.0,
            )

            with mock.patch.object(
                DETERMINISM,
                "_compare_output_file",
                return_value=stability,
            ):
                results = DETERMINISM._check_manifest(Path("baa"), out_dir)

        self.assertEqual([result.name for result in results], [
            "manifest-byte-stability",
            "manifest-shape",
        ])
        self.assertTrue(all(result.passed for result in results))


if __name__ == "__main__":
    unittest.main()
