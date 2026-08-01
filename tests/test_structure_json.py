#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import tempfile
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


def _is_utf8_boundary(source: bytes, offset: int) -> bool:
    return offset == len(source) or offset == 0 or source[offset] & 0xC0 != 0x80


class StructureJsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def dump(self, logical: Path, source: bytes) -> tuple[subprocess.CompletedProcess[bytes], dict]:
        proc = subprocess.run(
            [
                str(self.baa),
                "--dump-structure=json",
                f"--source-stdin={logical}",
            ],
            input=source,
            capture_output=True,
            timeout=30,
        )
        result = json.loads(proc.stdout.decode("utf-8")) if proc.stdout else {}
        return proc, result

    def test_emits_tolerant_nested_editor_ranges(self) -> None:
        text = (
            "صحيح احسب(صحيح س) {\n"
            "    /* تعليق\n"
            "       { ليس كتلة }\n"
            "    */\n"
            "    // أول\n"
            "    // ثان\n"
            "    إذا (س > ٠) {\n"
            '        اطبع "{ليست كتلة}".\n'
            "    }\n"
            "    إرجع س.\n"
            "}\n"
        )
        source = text.encode("utf-8")
        with tempfile.TemporaryDirectory(prefix="baa_structure_مسار_") as temp:
            logical = Path(temp) / "مصدر عربي.baa"
            first, result = self.dump(logical, source)
            second, duplicate = self.dump(logical, source)

        self.assertEqual(first.returncode, 0, first.stderr.decode("utf-8", "replace"))
        self.assertEqual(second.returncode, 0)
        self.assertEqual(result, duplicate)
        self.assertEqual(result["schema_version"], "structure-json-v1")
        self.assertEqual(result["language"], "baa")
        self.assertEqual(result["position_encoding"], "utf-8-bytes")
        self.assertEqual(result["source_bytes"], len(source))
        self.assertTrue(result["complete"])

        folds = result["folding_ranges"]
        self.assertEqual(len(folds), 4, folds)
        self.assertEqual(
            sorted(item["kind"] for item in folds),
            ["comment", "comment", "region", "region"],
        )
        selections = result["selection_ranges"]
        self.assertTrue(any(item["kind"] == "document" for item in selections))
        self.assertTrue(any(item["kind"] == "construct" for item in selections))
        self.assertTrue(any(item["kind"] == "content" for item in selections))
        self.assertTrue(any(item["kind"] == "token" for item in selections))

        for item in [*folds, *selections]:
            start = item["span"]["start"]["byte"]
            end = item["span"]["end"]["byte"]
            self.assertLess(start, end)
            self.assertLessEqual(end, len(source))
            self.assertTrue(_is_utf8_boundary(source, start))
            self.assertTrue(_is_utf8_boundary(source, end))

    def test_incomplete_source_returns_partial_structure(self) -> None:
        source = 'صحيح الرئيسية() {\n    نص قيمة = "ناقص'.encode("utf-8")
        with tempfile.TemporaryDirectory(prefix="baa_structure_incomplete_") as temp:
            proc, result = self.dump(Path(temp) / "غير مكتمل.baa", source)
            escaped_proc, escaped_result = self.dump(
                Path(temp) / "نص مهروب.baa", b'"abc' + b'\\' + b'"'
            )

        self.assertEqual(proc.returncode, 0, proc.stderr.decode("utf-8", "replace"))
        self.assertFalse(result["complete"])
        self.assertGreater(len(result["selection_ranges"]), 0)
        self.assertEqual(escaped_proc.returncode, 0)
        self.assertFalse(escaped_result["complete"])

    def test_rejects_invalid_utf8_and_invalid_cli_combinations(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_structure_invalid_") as temp:
            logical = Path(temp) / "تالف.baa"
            invalid, _ = self.dump(logical, b"\xff")
            unsupported = subprocess.run(
                [str(self.baa), "--dump-structure=xml", str(logical)],
                capture_output=True,
                timeout=30,
            )
            combined = subprocess.run(
                [
                    str(self.baa),
                    "--dump-structure=json",
                    "--dump-tokens=json",
                    f"--source-stdin={logical}",
                ],
                input=b"",
                capture_output=True,
                timeout=30,
            )
            missing = subprocess.run(
                [str(self.baa), "--dump-structure=json", str(logical)],
                capture_output=True,
                timeout=30,
            )

        self.assertEqual(invalid.returncode, 1)
        self.assertEqual(unsupported.returncode, 2)
        self.assertEqual(combined.returncode, 2)
        self.assertEqual(missing.returncode, 1)


if __name__ == "__main__":
    unittest.main()
