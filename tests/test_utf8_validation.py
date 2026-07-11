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
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = [
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build-linux" / "baa",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class Utf8ValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def _compile_bytes(
        self,
        files: dict[str, bytes],
        main_name: str = "main.baa",
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory(prefix="baa_utf8_validation_") as temp:
            root = Path(temp)
            for name, data in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)

            out = root / ("out.exe" if os.name == "nt" else "out")
            return subprocess.run(
                [str(self.baa), "-O1", main_name, "-o", str(out)],
                cwd=str(root),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=20,
            )

    def assert_compile_fails_with(self, files: dict[str, bytes], *markers: str) -> None:
        proc = self._compile_bytes(files)
        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertNotEqual(proc.returncode, 0, combined)
        for marker in markers:
            self.assertIn(marker, combined)

    def test_invalid_utf8_in_identifier_reports_byte_diagnostic(self) -> None:
        src = (
            "صحيح ".encode("utf-8")
            + b"\xff"
            + " = ١.\nصحيح الرئيسية() { إرجع ٠. }\n".encode("utf-8")
        )
        self.assert_compile_fails_with({"main.baa": src}, "0xFF", "خطأ")

    def test_invalid_utf8_in_string_literal_reports_literal_context(self) -> None:
        src = (
            "صحيح الرئيسية() {\n    نص س = \"".encode("utf-8")
            + b"\xc3("
            + "\".\n    إرجع ٠.\n}\n".encode("utf-8")
        )
        self.assert_compile_fails_with({"main.baa": src}, "UTF-8", "النص")

    def test_invalid_utf8_in_char_literal_reports_literal_context(self) -> None:
        src = (
            "صحيح الرئيسية() {\n    حرف ح = '".encode("utf-8")
            + b"\xc3("
            + "'.\n    إرجع ٠.\n}\n".encode("utf-8")
        )
        self.assert_compile_fails_with({"main.baa": src}, "UTF-8", "الحرف")

    def test_invalid_utf8_in_included_file_reports_included_path(self) -> None:
        main = '#تضمين "bad.baahd"\nصحيح الرئيسية() { إرجع ٠. }\n'.encode("utf-8")
        self.assert_compile_fails_with(
            {"main.baa": main, "bad.baahd": b"\xff\n"},
            "bad.baahd",
            "0xFF",
        )

    @unittest.skipUnless(os.name == "nt", "Windows Unicode argv regression")
    def test_windows_unicode_source_and_include_paths_reach_driver_as_utf8(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_unicode_argv_") as temp:
            work = Path(temp) / "مشروع عربي"
            work.mkdir()
            header = work / "تكوين.baahd"
            source = work / "الرئيسية.baa"
            header.write_text(
                "خارجي صحيح قيمة().\n",
                encoding="utf-8",
            )
            source.write_text(
                '#تضمين "تكوين.baahd"\n'
                "صحيح قيمة() { إرجع ٧. }\n"
                "صحيح الرئيسية() { إرجع قيمة() - ٧. }\n",
                encoding="utf-8",
            )
            proc = subprocess.run(
                [str(self.baa), "--check", "--diagnostics=json", source.name],
                cwd=str(work),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=20,
            )

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        data = json.loads(proc.stdout)
        self.assertEqual(data["schema_version"], "diagnostics-json-v1")
        self.assertEqual(data["diagnostics"], [])


if __name__ == "__main__":
    unittest.main()
