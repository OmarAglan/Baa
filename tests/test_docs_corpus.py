#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "extract_docs_corpus.py"
SPEC = importlib.util.spec_from_file_location("baa_extract_docs_corpus", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
EXTRACTOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = EXTRACTOR
SPEC.loader.exec_module(EXTRACTOR)


ARABIC_README = """\
# مثال

```baa
صحيح الرئيسية() {
    إرجع ٠.
}
```
"""


class HistoricalDocsCorpusTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.previous_root = EXTRACTOR.ROOT
        EXTRACTOR.ROOT = self.root

        self._git("init", "-q")
        self._git("config", "user.name", "Baa QA")
        self._git("config", "user.email", "qa@example.invalid")
        (self.root / "README.md").write_text(ARABIC_README, encoding="utf-8")
        self._git("add", "README.md")
        self._git("-c", "commit.gpgsign=false", "commit", "-q", "-m", "fixture")

    def tearDown(self) -> None:
        EXTRACTOR.ROOT = self.previous_root
        self.temp_dir.cleanup()

    def _git(self, *args: str) -> None:
        subprocess.run(
            ["git", *args],
            cwd=str(self.root),
            check=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
        )

    def test_git_snapshot_preserves_arabic_program_text(self) -> None:
        self.assertEqual(EXTRACTOR._git_list_md_files("HEAD"), ["README.md"])

        text = EXTRACTOR._git_show_text("HEAD", "README.md")

        self.assertIsNotNone(text)
        assert text is not None
        self.assertIn("صحيح الرئيسية", text)
        blocks = EXTRACTOR._extract_baa_blocks_from_text(text)
        self.assertEqual(len(blocks), 1)
        self.assertTrue(EXTRACTOR._is_candidate_program(blocks[0], allow_io=False))


if __name__ == "__main__":
    unittest.main()
