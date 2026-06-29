#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import io
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "check_version_sync.py"
SPEC = importlib.util.spec_from_file_location("baa_check_version_sync", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
VERSION_SYNC = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VERSION_SYNC
SPEC.loader.exec_module(VERSION_SYNC)

FIXTURE_FILES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "README.md",
    "setup.iss",
    "src/baa.h",
    "src/baa.rc",
    "src/support/version.h",
    "docs/API_REFERENCE.md",
    "docs/BAA_BOOK_AR.md",
    "docs/BAA_IR_SPECIFICATION.md",
    "docs/BOOTSTRAP_CONTRACT.md",
    "docs/INTERNALS.md",
    "docs/IR_DEVELOPER_GUIDE.md",
    "docs/LANGUAGE.md",
    "docs/STYLE_GUIDE.md",
    "docs/USER_GUIDE.md",
)


class VersionSyncTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        for relative in FIXTURE_FILES:
            source = REPO_ROOT / relative
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        self.previous_root = VERSION_SYNC.ROOT
        VERSION_SYNC.ROOT = self.root

    def tearDown(self) -> None:
        VERSION_SYNC.ROOT = self.previous_root
        self.temp_dir.cleanup()

    def run_guard(self) -> tuple[int, str, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = VERSION_SYNC.main()
        return result, stdout.getvalue(), stderr.getvalue()

    def test_accepts_synchronized_release_metadata(self) -> None:
        result, stdout, stderr = self.run_guard()

        self.assertEqual(result, 0)
        self.assertIn("version-sync: PASS (version=0.5.9", stdout)
        self.assertEqual(stderr, "")

    def test_rejects_compiler_macro_drift(self) -> None:
        header = self.root / "src" / "support" / "version.h"
        text = header.read_text(encoding="utf-8").replace(
            '#define BAA_VERSION "0.5.9"',
            '#define BAA_VERSION "0.5.8"',
        )
        header.write_text(text, encoding="utf-8")

        result, _, stderr = self.run_guard()

        self.assertEqual(result, 1)
        self.assertIn("src/support/version.h", stderr)
        self.assertIn('#define BAA_VERSION "0.5.9"', stderr)


if __name__ == "__main__":
    unittest.main()
