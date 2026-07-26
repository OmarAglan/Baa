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

    def test_invalid_utf8_from_source_stdin_remains_structured(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_utf8_stdin_") as temp:
            work = Path(temp)
            logical = work / "مشروع عربي" / "رئيسي.baa"
            logical.parent.mkdir()
            source = (
                "صحيح ".encode("utf-8")
                + b"\xff"
                + " = ١.\nصحيح الرئيسية() { إرجع ٠. }\n".encode("utf-8")
            )
            proc = subprocess.run(
                [
                    str(self.baa),
                    "--check",
                    "--diagnostics=json",
                    f"--source-stdin={logical}",
                ],
                cwd=str(work),
                input=source,
                capture_output=True,
                timeout=20,
            )

        self.assertEqual(proc.returncode, 1, proc.stderr.decode("utf-8", errors="replace"))
        data = json.loads(proc.stdout.decode("utf-8"))
        self.assertEqual(data["schema_version"], "diagnostics-json-v1")
        self.assertEqual(Path(data["diagnostics"][0]["file"]), logical)
        self.assertIn("0xFF", data["diagnostics"][0]["message"])

    def test_generated_program_accepts_arabic_argv_without_main_alias(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_arabic_entry_") as temp:
            work = Path(temp)
            source = work / "الرئيسية.baa"
            output = work / ("برنامج.exe" if os.name == "nt" else "برنامج")
            assembly = work / "برنامج.نظم"
            source.write_text(
                '#تضمين "baalib.baahd"\n'
                "صحيح الرئيسية(صحيح عدد، نص[] معاملات) {\n"
                "    إذا (عدد != ٢) { إرجع ١. }\n"
                '    إذا (قارن_نص(معاملات[١]، "تحقق") != ٠) { إرجع ٢. }\n'
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )
            compile_proc = subprocess.run(
                [str(self.baa), "-I", str(ROOT / "stdlib"), source.name, "-o", str(output)],
                cwd=str(work),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(
                compile_proc.returncode,
                0,
                f"{compile_proc.stdout}\n{compile_proc.stderr}",
            )
            run_proc = subprocess.run(
                [str(output), "تحقق"],
                cwd=str(work),
                capture_output=True,
                timeout=20,
            )
            self.assertEqual(run_proc.returncode, 0, run_proc.stderr)

            assembly_proc = subprocess.run(
                [str(self.baa), "-I", str(ROOT / "stdlib"), "-S", source.name, "-o", str(assembly)],
                cwd=str(work),
                capture_output=True,
                timeout=20,
            )
            self.assertEqual(assembly_proc.returncode, 0, assembly_proc.stderr)
            assembly_text = assembly.read_text(encoding="utf-8")
            self.assertIn(".عام الرئيسية", assembly_text)
            self.assertNotIn(".globl", assembly_text)
            self.assertIn("الرئيسية_المستخدم", assembly_text)

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

    def test_assembly_only_writes_unicode_output_directly(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_direct_assembly_") as temp:
            work = Path(temp) / "مشروع عربي مباشر"
            work.mkdir()
            source = work / "مدخل.baa"
            output = work / "ناتج نهائي.نظم"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            proc = subprocess.run(
                [
                    str(self.baa),
                    "-v",
                    "-S",
                    source.name,
                    "-o",
                    output.name,
                ],
                cwd=str(work),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=20,
            )

            combined = f"{proc.stdout}\n{proc.stderr}"
            self.assertEqual(proc.returncode, 0, combined)
            self.assertTrue(output.is_file())
            self.assertIn(".عام الرئيسية", output.read_text(encoding="utf-8"))
            self.assertEqual(list(work.glob(".baa_asm_*.s")), [])

    @unittest.skipUnless(os.name == "nt", "Windows Unicode cache-path regression")
    def test_windows_incremental_cache_accepts_unicode_directory(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_unicode_cache_") as temp:
            work = Path(temp)
            source = work / "main.baa"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            cache = work / "بناء" / ".baa-cache"
            manifest = work / "بناء" / "build-manifest.json"
            output = work / "out.exe"
            (work / "بناء").mkdir()
            command = [
                str(self.baa),
                "--assembler=gas",
                "--incremental",
                "--cache-dir",
                str(cache.relative_to(work)),
                "--emit-build-manifest",
                str(manifest.relative_to(work)),
                source.name,
                "-o",
                output.name,
            ]

            first = subprocess.run(
                command,
                cwd=str(work),
                timeout=20,
            )
            second = subprocess.run(
                command,
                cwd=str(work),
                timeout=20,
            )
            self.assertEqual(first.returncode, 0)
            self.assertEqual(second.returncode, 0)
            data = json.loads(manifest.read_text(encoding="utf-8"))

        self.assertEqual(data["schema"], 1)
        self.assertTrue(data["incremental"])
        self.assertTrue(data["units"])
        self.assertTrue(data["units"][0]["cache"]["hit"])


if __name__ == "__main__":
    unittest.main()
