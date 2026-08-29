#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _find_baa() -> Path:
    configured = os.environ.get("BAA")
    if configured and Path(configured).is_file():
        return Path(configured)
    for candidate in (
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "baa",
        ROOT / "build-linux" / "baa",
    ):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class CompletionDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def run_baa(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        proc = subprocess.run(
            [str(self.baa), *arguments],
            cwd=str(ROOT),
            capture_output=True,
            timeout=30,
        )
        return subprocess.CompletedProcess(
            proc.args,
            proc.returncode,
            proc.stdout.decode("utf-8", errors="replace"),
            proc.stderr.decode("utf-8", errors="replace"),
        )

    def test_exports_versioned_arabic_language_metadata(self) -> None:
        proc = self.run_baa("--completion-data=json")
        self.assertEqual(proc.returncode, 0, f"{proc.stdout}\n{proc.stderr}")
        data = json.loads(proc.stdout)
        self.assertEqual(data["schema_version"], "completion-data-json-v1")
        self.assertEqual(data["language"], "baa")
        self.assertTrue(data["compiler_version"])

        items = data["items"]
        self.assertGreater(len(items), 50)
        keywords = {
            item["label"]
            for item in items
            if item["kind"] in {"keyword", "type", "value"}
        }
        self.assertEqual(
            keywords,
            {
                "إرجع", "اطبع", "اقرأ", "مجمع", "صحيح", "ص٨", "ص١٦",
                "ص٣٢", "ص٦٤", "ط٨", "ط١٦", "ط٣٢", "ط٦٤", "نص",
                "منطقي", "حرف", "عشري", "عشري٣٢", "عدم", "دالة", "كـ", "حجم",
                "نوع", "ثابت", "ساكن", "خارجي", "إذا", "وإلا", "طالما",
                "لكل", "توقف", "استمر", "اختر", "حالة", "افتراضي",
                "صواب", "خطأ", "تعداد", "هيكل", "اتحاد",
            },
        )

        by_filter = {item["filter_text"]: item for item in items}
        self.assertTrue(by_filter["نوع"]["contextual"])
        function_pointer_type = next(
            item for item in items
            if item["label"] == "دالة" and item["kind"] == "type"
        )
        self.assertTrue(function_pointer_type["contextual"])
        self.assertEqual(function_pointer_type["relevance"], 40)
        self.assertEqual(by_filter["الرئيسية"]["insert_text_format"], "snippet")
        self.assertEqual(by_filter["الرئيسية"]["relevance"], 20)
        self.assertIn("${0}", by_filter["الرئيسية"]["insert_text"])
        self.assertIn("#تضمين", by_filter)
        self.assertNotIn("main", by_filter)
        self.assertNotIn("function", by_filter)
        self.assertNotIn("ifelse", by_filter)

    def test_exports_compiler_owned_builtin_signatures_and_documentation(self) -> None:
        proc = self.run_baa("--completion-data=json")
        self.assertEqual(proc.returncode, 0, f"{proc.stdout}\n{proc.stderr}")
        data = json.loads(proc.stdout)
        builtins = {
            item["label"]: item
            for item in data["items"]
            if item["kind"] == "function"
        }

        self.assertIn("ابدأ_عملية", builtins)
        self.assertIn("اطبع_منسق", builtins)
        self.assertIn("اقرأ_رقم", builtins)
        self.assertIn("←", builtins["ابدأ_عملية"]["detail"])
        self.assertIn("دالة مدمجة", builtins["ابدأ_عملية"]["documentation"])
        self.assertEqual(
            builtins["ابدأ_عملية"]["insert_text_format"],
            "plain",
        )
        self.assertEqual(builtins["ابدأ_عملية"]["relevance"], 30)

    def test_rejects_unknown_format_and_positional_source(self) -> None:
        bad_format = self.run_baa("--completion-data=text")
        self.assertEqual(bad_format.returncode, 2)
        with_source = self.run_baa("--completion-data=json", "مثال.baa")
        self.assertEqual(with_source.returncode, 2)


if __name__ == "__main__":
    unittest.main()
