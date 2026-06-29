#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "check_reference_compiler_policy.py"
SPEC = importlib.util.spec_from_file_location("check_reference_compiler_policy", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)


VALID_CMAKE = """\
cmake_minimum_required(VERSION 3.10)
project(baa VERSION 0.5.8 LANGUAGES C)
set(SOURCES
    src/driver/main.c
)
add_executable(baa ${SOURCES})
"""


class ReferenceCompilerPolicyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        (self.root / "CMakeLists.txt").write_text(VALID_CMAKE, encoding="utf-8")
        self.previous_root = POLICY.ROOT
        POLICY.ROOT = self.root

    def tearDown(self) -> None:
        POLICY.ROOT = self.previous_root
        self.temp_dir.cleanup()

    def run_policy(self) -> tuple[int, str, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = POLICY.main()
        return result, stdout.getvalue(), stderr.getvalue()

    def test_accepts_c_only_reference_build_and_packaged_baa_associations(self) -> None:
        (self.root / "setup.iss").write_text(
            'Name: "assoc_baa"; Description: "Associate .baa/.baahd files"\n',
            encoding="utf-8",
        )

        result, stdout, stderr = self.run_policy()

        self.assertEqual(result, 0)
        self.assertIn("reference-compiler-policy: PASS", stdout)
        self.assertEqual(stderr, "")

    def test_rejects_bootstrap_compiler_setting_in_workflow(self) -> None:
        workflows = self.root / ".github" / "workflows"
        workflows.mkdir(parents=True)
        (workflows / "ci.yml").write_text(
            "env:\n  BAA_BOOTSTRAP_COMPILER: build/baa\n",
            encoding="utf-8",
        )

        result, _, stderr = self.run_policy()

        self.assertEqual(result, 1)
        self.assertIn("forbidden bootstrap requirement", stderr)

    def test_rejects_baa_source_in_compiler_target(self) -> None:
        (self.root / "CMakeLists.txt").write_text(
            VALID_CMAKE.replace("src/driver/main.c", "src/compiler/main.baa"),
            encoding="utf-8",
        )

        result, _, stderr = self.run_policy()

        self.assertEqual(result, 1)
        self.assertIn("non-C inputs", stderr)


if __name__ == "__main__":
    unittest.main()
