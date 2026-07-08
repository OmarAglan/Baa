#!/usr/bin/env python3

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs" / "MODULES_AND_VISIBILITY.md"
README = ROOT / "README.md"


class ModuleVisibilityDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = DOC.read_text(encoding="utf-8")

    def test_module_visibility_doc_exists_with_expected_scope(self) -> None:
        self.assertTrue(DOC.is_file())
        self.assertIn("# Baa Module and Visibility Contract", self.text)
        self.assertIn("> **Version:** draft-0.1 | **Applies to:** Baa v0.7.1+", self.text)

    def test_file_role_contract_covers_baa_and_baahd(self) -> None:
        for marker in (
            "| `.baa` | implementation/source unit | yes |",
            "| `.baahd` | header/declaration unit | no, except future self-check mode |",
            "Compile implementation units (`.baa`) as roots.",
            "Include headers (`.baahd`) from implementation units with `#تضمين`.",
            "New public headers should be declaration-only by convention.",
            "strict header-body",
            "rejection is reserved for the future header self-check mode.",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, self.text)

    def test_visibility_rules_cover_current_modifiers(self) -> None:
        for marker in (
            "| top-level function definition | exported/default-visible symbol |",
            "| `خارجي` function prototype | declaration only; definition must be provided elsewhere |",
            "| `ساكن` global or array | file-local/internal storage |",
            "| local `ساكن` variable or array | static-duration local storage |",
            "| `ساكن` function definition | not supported; rejected by the parser |",
            "| `ثابت` | immutability/storage policy, not a visibility modifier |",
            "There are no `public`/`internal` keywords yet.",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, self.text)

    def test_extern_and_one_definition_policy_are_explicit(self) -> None:
        for marker in (
            "`خارجي` is allowed only at top level.",
            "A `خارجي` function declaration ends with `.` and has no body.",
            "Repeated `خارجي` declarations must match in type, constness, and array shape.",
            "`خارجي` cannot be combined with `ساكن`.",
            "one exported function definition per symbol",
            "file-local `ساكن` globals/arrays do not conflict across files.",
            "Better front-end duplicate-symbol diagnostics remain a v0.7.1 follow-up.",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, self.text)

    def test_takween_migration_path_keeps_baa_compiler_boundary(self) -> None:
        for marker in (
            "Treat `.baa` files as implementation roots.",
            "Treat `.baahd` files as include/declaration surfaces.",
            "Pass include directories with `-I` in project-config order.",
            "Consume `--emit-build-manifest` for dependency hashes and invalidation.",
            "`baa build`, `baa run`, and `baa clean` are not compiler",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, self.text)

    def test_readme_links_module_visibility_contract(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("docs/MODULES_AND_VISIBILITY.md", readme)


if __name__ == "__main__":
    unittest.main()
