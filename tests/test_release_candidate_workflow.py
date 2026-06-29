#!/usr/bin/env python3

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "release-candidate.yml"


class ReleaseCandidateWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = WORKFLOW.read_text(encoding="utf-8")

    def test_is_manual_and_read_only(self) -> None:
        self.assertIn("workflow_dispatch:", self.text)
        self.assertIn("permissions:\n  contents: read", self.text)

    def test_uses_strict_c_reference_presets_on_both_hosts(self) -> None:
        self.assertIn("cmake --preset windows-verify", self.text)
        self.assertIn("cmake --build --preset windows-verify --clean-first", self.text)
        self.assertIn("cmake --preset linux-verify", self.text)
        self.assertIn("cmake --build --preset linux-verify --clean-first -j", self.text)

    def test_runs_release_mode_and_preserves_both_receipts(self) -> None:
        self.assertEqual(self.text.count("--mode release"), 2)
        self.assertIn("qa-summary-release-windows.json", self.text)
        self.assertIn("qa-summary-release-linux.json", self.text)
        self.assertEqual(self.text.count("if: always()"), 2)
        self.assertEqual(self.text.count("uses: actions/upload-artifact@v4"), 2)
        self.assertEqual(self.text.count("if-no-files-found: error"), 2)


if __name__ == "__main__":
    unittest.main()
