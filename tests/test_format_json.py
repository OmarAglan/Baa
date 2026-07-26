#!/usr/bin/env python3
"""Contract tests for Baa-owned format-json-v1 source formatting."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _compiler() -> Path:
    configured = os.environ.get("BAA")
    if configured:
        return Path(configured).resolve()
    suffix = ".exe" if os.name == "nt" else ""
    candidates = (
        ROOT / "build" / "codex-navigation" / f"baa{suffix}",
        ROOT / "build" / f"baa{suffix}",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise unittest.SkipTest("Set BAA to a built Baa compiler")


class FormatJsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _compiler()

    def run_format(
        self, source: bytes, logical_path: Path | str = "رئيسي.baa"
    ) -> subprocess.CompletedProcess[bytes]:
        return subprocess.run(
            [
                str(self.baa),
                "--format=json",
                f"--source-stdin={logical_path}",
            ],
            cwd=ROOT,
            input=source,
            capture_output=True,
            timeout=30,
        )

    def test_formats_arabic_source_without_losing_literals_or_comments(self) -> None:
        source = (
            '#تضمين   "stdlib/baalib.baahd"\r\n'
            "\r\n"
            "صحيح  اجمع ( صحيح أ،صحيح ب){ // جمع القيم\r\n"
            'إرجع أ+ب.}\r\n'
            "صحيح الرئيسية(){\r\n"
            "عشري س=٠.٥.صحيح* م=عدم.\r\n"
            'نص وصف="// { أ+ب }".\r\n'
            "هيكل نقطة ن={س:١،ص:٢}.\r\n"
            "إذا(!م){إرجع اجمع(١،٢).}وإلا{إرجع ٠.}}\r\n"
        ).encode("utf-8")
        logical = Path("مسار عربي") / "رئيسي.baa"
        proc = self.run_format(source, logical)
        self.assertEqual(
            proc.returncode,
            0,
            proc.stderr.decode("utf-8", errors="replace"),
        )
        data = json.loads(proc.stdout.decode("utf-8"))
        self.assertEqual(data["schema_version"], "format-json-v1")
        self.assertEqual(data["language"], "baa")
        self.assertEqual(Path(data["file"]), logical)
        self.assertEqual(data["position_encoding"], "utf-8-bytes")
        self.assertEqual(data["line_ending"], "lf")
        self.assertEqual(data["indent_width"], 4)
        self.assertTrue(data["insert_spaces"])
        self.assertEqual(data["source_bytes"], len(source))
        self.assertTrue(data["changed"])
        expected = (
            '#تضمين "stdlib/baalib.baahd"\n'
            "\n"
            "صحيح اجمع(صحيح أ، صحيح ب) {\n"
            "    // جمع القيم\n"
            "    إرجع أ + ب.\n"
            "}\n"
            "صحيح الرئيسية() {\n"
            "    عشري س = ٠.٥.\n"
            "    صحيح* م = عدم.\n"
            '    نص وصف = "// { أ+ب }".\n'
            "    هيكل نقطة ن = { س: ١، ص: ٢ }.\n"
            "    إذا (!م) {\n"
            "        إرجع اجمع(١، ٢).\n"
            "    } وإلا {\n"
            "        إرجع ٠.\n"
            "    }\n"
            "}\n"
        )
        self.assertEqual(data["formatted_text"], expected)
        self.assertEqual(data["formatted_bytes"], len(expected.encode("utf-8")))

        second = self.run_format(expected.encode("utf-8"), logical)
        self.assertEqual(second.returncode, 0)
        second_data = json.loads(second.stdout.decode("utf-8"))
        self.assertFalse(second_data["changed"])
        self.assertEqual(second_data["formatted_text"], expected)

    def test_formats_incomplete_editor_buffer_without_parsing(self) -> None:
        source = "صحيح الرئيسية(){إذا(صواب){إرجع".encode("utf-8")
        proc = self.run_format(source)
        self.assertEqual(proc.returncode, 0)
        data = json.loads(proc.stdout.decode("utf-8"))
        self.assertEqual(
            data["formatted_text"],
            (
                "صحيح الرئيسية() {\n"
                "    إذا (صواب) {\n"
                "        إرجع\n"
            ),
        )

    def test_formats_an_unsaved_baa_header_buffer(self) -> None:
        source = "خارجي  صحيح جمع(صحيح أ،صحيح ب).\n".encode("utf-8")
        logical = Path("تضمينات") / "حساب.baahd"
        proc = self.run_format(source, logical)
        self.assertEqual(proc.returncode, 0)
        data = json.loads(proc.stdout.decode("utf-8"))
        self.assertEqual(Path(data["file"]), logical)
        self.assertEqual(
            data["formatted_text"],
            "خارجي صحيح جمع(صحيح أ، صحيح ب).\n",
        )

    def test_rejects_invalid_utf8_and_invalid_cli_combinations(self) -> None:
        invalid = self.run_format(b"\xff")
        self.assertEqual(invalid.returncode, 1)
        self.assertEqual(invalid.stdout, b"")
        self.assertIn(
            "UTF-8",
            invalid.stderr.decode("utf-8", errors="replace"),
        )

        unsupported = subprocess.run(
            [str(self.baa), "--format=text", "رئيسي.baa"],
            cwd=ROOT,
            capture_output=True,
            timeout=30,
        )
        self.assertEqual(unsupported.returncode, 2)

        conflicting = subprocess.run(
            [str(self.baa), "--format=json", "--check", "رئيسي.baa"],
            cwd=ROOT,
            capture_output=True,
            timeout=30,
        )
        self.assertEqual(conflicting.returncode, 2)

    def test_reads_an_arabic_path_with_spaces(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa-format-") as temporary:
            path = Path(temporary) / "مسار عربي" / "غير منسق.baa"
            path.parent.mkdir()
            path.write_text(
                "صحيح الرئيسية(){إرجع ٠.}\n",
                encoding="utf-8",
            )
            proc = subprocess.run(
                [str(self.baa), "--format=json", str(path)],
                cwd=ROOT,
                capture_output=True,
                timeout=30,
            )
        self.assertEqual(proc.returncode, 0)
        data = json.loads(proc.stdout.decode("utf-8"))
        self.assertEqual(Path(data["file"]), path)
        self.assertEqual(
            data["formatted_text"],
            "صحيح الرئيسية() {\n    إرجع ٠.\n}\n",
        )


if __name__ == "__main__":
    unittest.main()
