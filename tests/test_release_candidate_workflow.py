#!/usr/bin/env python3

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "release-candidate.yml"
NAZM_ADMISSION_WORKFLOW = (
    ROOT / ".github" / "workflows" / "nazm-production-admission.yml"
)


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

    def test_runs_every_mode_and_preserves_both_receipt_sets(self) -> None:
        self.assertEqual(self.text.count("--mode quick"), 2)
        self.assertEqual(self.text.count("--mode full"), 2)
        self.assertEqual(self.text.count("--mode stress"), 2)
        self.assertEqual(self.text.count("--mode release"), 2)
        self.assertIn("qa-summary-quick-windows.json", self.text)
        self.assertIn("qa-summary-full-windows.json", self.text)
        self.assertIn("qa-summary-stress-windows.json", self.text)
        self.assertIn("qa-summary-release-windows.json", self.text)
        self.assertIn("qa-summary-quick-linux.json", self.text)
        self.assertIn("qa-summary-full-linux.json", self.text)
        self.assertIn("qa-summary-stress-linux.json", self.text)
        self.assertIn("qa-summary-release-linux.json", self.text)
        self.assertEqual(self.text.count("if: always()"), 2)
        self.assertEqual(self.text.count("uses: actions/upload-artifact@v4"), 2)
        self.assertEqual(self.text.count("if-no-files-found: error"), 2)
        self.assertEqual(self.text.count("include-hidden-files: true"), 2)

class NazmProductionAdmissionWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = NAZM_ADMISSION_WORKFLOW.read_text(encoding="utf-8")

    def test_requires_an_exact_nazm_revision_and_read_only_access(self) -> None:
        self.assertIn("workflow_dispatch:", self.text)
        self.assertIn("baa_ref:", self.text)
        self.assertIn("nazm_ref:", self.text)
        self.assertIn("required: true", self.text)
        self.assertIn("permissions:\n  contents: read", self.text)
        self.assertEqual(
            self.text.count("ref: ${{ inputs.baa_ref }}"),
            2,
        )
        self.assertEqual(
            self.text.count("ref: ${{ inputs.nazm_ref }}"),
            2,
        )
        self.assertEqual(
            self.text.count("^[0-9a-f]{40}$"),
            4,
        )
        self.assertEqual(self.text.count("git rev-parse HEAD"), 2)
        self.assertEqual(
            self.text.count("git -C Nazm rev-parse HEAD"),
            2,
        )

    def test_builds_baa_and_nazm_on_both_hosts(self) -> None:
        self.assertEqual(
            self.text.count("repository: OmarAglan/Nazm"),
            2,
        )
        self.assertIn("cmake --preset windows-verify", self.text)
        self.assertIn(
            "cmake --build --preset windows-verify --clean-first",
            self.text,
        )
        self.assertIn("cmake --preset linux-verify", self.text)
        self.assertIn(
            "cmake --build --preset linux-verify --clean-first -j",
            self.text,
        )
        self.assertEqual(
            self.text.count("cmake -S Nazm -B Nazm/build-admission"),
            2,
        )
        self.assertEqual(
            self.text.count("cmake --build Nazm/build-admission"),
            2,
        )

    def test_runs_and_preserves_every_cross_platform_admission_mode(self) -> None:
        self.assertEqual(self.text.count("--mode quick"), 2)
        self.assertEqual(self.text.count("--mode full"), 2)
        self.assertEqual(self.text.count("--mode stress"), 2)
        self.assertEqual(self.text.count("--mode release"), 2)
        self.assertEqual(self.text.count("--log-dir .baa_qa_logs/"), 8)
        self.assertIn("qa-summary-nazm-release-windows.json", self.text)
        self.assertIn("qa-summary-nazm-release-linux.json", self.text)
        self.assertEqual(
            self.text.count("baa-nazm-admission-revisions-v1"),
            2,
        )
        self.assertEqual(self.text.count("if: always()"), 2)
        self.assertEqual(
            self.text.count("uses: actions/upload-artifact@v4"),
            2,
        )
        self.assertEqual(self.text.count("if-no-files-found: error"), 2)


if __name__ == "__main__":
    unittest.main()
