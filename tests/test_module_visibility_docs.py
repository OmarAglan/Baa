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

    def test_file_role_contract_covers_canonical_arabic_extensions(self) -> None:
        for marker in (
            "| `.باء` | implementation/source unit | yes |",
            "| `.رأسباء` | header/declaration unit | only with `--check-header` |",
            "Compile implementation units (`.باء`) as roots.",
            "Include headers (`.رأسباء`) from implementation units with `#تضمين`.",
            "`.baa` and `.baahd` remain accepted compatibility spellings",
            "Use `--check-header` to parse and semantically validate a header",
            "New public headers should be declaration-only by convention.",
            "`--check-header`",
            "does not yet enforce a header-only grammar.",
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
            "Treat `.baahd` files as include/declaration surfaces, optionally checked with `--check-header`.",
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
