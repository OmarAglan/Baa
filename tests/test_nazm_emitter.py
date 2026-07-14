#!/usr/bin/env python3

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        candidate = Path(env)
        if candidate.is_file():
            return candidate

    candidates = [
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build" / "baa.exe",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


def _find_nazm() -> Path | None:
    env = os.environ.get("NAZM")
    if env:
        candidate = Path(env)
        if candidate.is_file():
            return candidate

    candidates = [
        ROOT.parent / "Nazm" / "build-e31" / "nazm.exe",
        ROOT.parent / "Nazm" / "build-e31" / "nazm",
        ROOT.parent / "Nazm" / "build" / "nazm.exe",
        ROOT.parent / "Nazm" / "build" / "nazm",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate

    command = shutil.which("nazm")
    return Path(command) if command else None


class NazmEmitterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def run_baa(self, work: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.baa), *args],
            cwd=str(work),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )

    @staticmethod
    def write_minimal_source(work: Path) -> Path:
        source = work / "برنامج.باء"
        source.write_text(
            "صحيح الرئيسية() {\n"
            "    إرجع ٠.\n"
            "}\n",
            encoding="utf-8",
        )
        return source

    def test_minimal_source_is_canonical_arabic_for_both_targets(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_emit_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)

            for target in ("x86_64-windows", "x86_64-linux"):
                with self.subTest(target=target):
                    output = work / f"خرج-{target}.نظم"
                    proc = self.run_baa(
                        work,
                        "--emit-nazm",
                        f"--target={target}",
                        str(source),
                        "-o",
                        str(output),
                    )
                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    text = output.read_text(encoding="utf-8")
                    self.assertIn(".عام الرئيسية", text)
                    self.assertIn("الرئيسية:", text)
                    self.assertIn("انقل سجل_المركم، ٠", text)
                    self.assertNotIn("main", text)
                    self.assertIsNone(
                        re.search(r"[A-Za-z]", text),
                        "Nazm source contains a Latin letter",
                    )

    def test_unsupported_form_is_visible_and_leaves_no_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_unsupported_") as temp:
            work = Path(temp)
            output = work / "غير-مدعوم.نظم"
            proc = self.run_baa(
                work,
                "--emit-nazm",
                str(ROOT / "examples" / "hello_world.baa"),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 3, proc.stderr)
            self.assertIn("غير مدعومة", proc.stderr)
            self.assertFalse(output.exists())

    def test_conflicting_output_modes_are_invalid_invocation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_conflict_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            proc = self.run_baa(work, "--emit-nazm", "-S", str(source))
            self.assertEqual(proc.returncode, 2, proc.stderr)

    def test_missing_shadow_assembler_is_visible_without_gas_fallback(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_missing_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            output = work / ("برنامج.exe" if os.name == "nt" else "برنامج")
            missing = work / "مجمّع-مفقود"
            proc = self.run_baa(
                work,
                f"--nazm-shadow={missing}",
                str(source),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 4, proc.stderr)
            self.assertIn("فشل مجمّع نظم", proc.stderr)
            self.assertFalse(output.exists(), "GAS output must not hide a shadow failure")

    def test_explicit_shadow_links_and_matches_minimal_runtime(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_shadow_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            exe_suffix = ".exe" if os.name == "nt" else ""
            object_suffix = ".obj" if os.name == "nt" else ".o"
            output = work / f"برنامج{exe_suffix}"
            proc = self.run_baa(
                work,
                f"--nazm-shadow={nazm}",
                str(source),
                "-o",
                str(output),
            )
            self.assertEqual(proc.returncode, 0, proc.stderr)

            shadow_source = Path(f"{output}.ظل-نظم.نظم")
            shadow_object = Path(f"{output}.ظل-نظم{object_suffix}")
            shadow_executable = Path(f"{output}.ظل-نظم{exe_suffix}")
            self.assertTrue(shadow_source.is_file())
            self.assertTrue(shadow_object.is_file())
            self.assertTrue(shadow_executable.is_file())

            nazm_text = shadow_source.read_text(encoding="utf-8")
            self.assertIsNone(re.search(r"[A-Za-z]", nazm_text))
            self.assertIn(".عام الرئيسية", nazm_text)

            production_assembly = work / "إنتاج.s"
            assembly_proc = self.run_baa(
                work, "-S", str(source), "-o", str(production_assembly)
            )
            self.assertEqual(assembly_proc.returncode, 0, assembly_proc.stderr)
            gas_text = production_assembly.read_text(encoding="utf-8")
            self.assertIn(".globl main", gas_text)
            self.assertRegex(gas_text, r"mov[q]?\s+\$0,\s*%rax")

            production_run = subprocess.run(
                [str(output)], capture_output=True, timeout=30
            )
            shadow_run = subprocess.run(
                [str(shadow_executable)], capture_output=True, timeout=30
            )
            self.assertEqual(shadow_run.returncode, production_run.returncode)
            self.assertEqual(shadow_run.stdout, production_run.stdout)
            self.assertEqual(shadow_run.stderr, production_run.stderr)

            nm = shutil.which("nm")
            objdump = shutil.which("objdump")
            if nm and objdump:
                production_object = work / f"إنتاج{object_suffix}"
                compile_object = self.run_baa(
                    work, "-c", str(source), "-o", str(production_object)
                )
                self.assertEqual(compile_object.returncode, 0, compile_object.stderr)

                for artifact in (production_object, shadow_object):
                    symbols = subprocess.run(
                        [nm, "-g", "--defined-only", str(artifact)],
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                        check=True,
                    ).stdout
                    self.assertRegex(symbols, r"\bmain$")

                    sections = subprocess.run(
                        [objdump, "-h", str(artifact)],
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                        check=True,
                    ).stdout
                    self.assertRegex(sections, r"\s\.text\s")

                    relocations = subprocess.run(
                        [objdump, "-r", str(artifact)],
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                        check=True,
                    ).stdout
                    self.assertNotRegex(relocations, r"R_X86_64_|IMAGE_REL_AMD64_")


if __name__ == "__main__":
    unittest.main()
