#!/usr/bin/env python3

from __future__ import annotations

import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _read_c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    if end < 0:
        end = len(data)
    return data[offset:end].decode("utf-8", errors="strict")


def _inspect_coff_object(data: bytes) -> tuple[set[str], set[str], int]:
    if len(data) < 20:
        raise ValueError("truncated COFF header")

    machine, section_count, _, symbol_offset, symbol_count, optional_size, _ = (
        struct.unpack_from("<HHLLLHH", data, 0)
    )
    if machine != 0x8664:
        raise ValueError(f"unexpected COFF machine: 0x{machine:04x}")

    sections: set[str] = set()
    relocation_count = 0
    section_offset = 20 + optional_size
    for index in range(section_count):
        offset = section_offset + (index * 40)
        if offset + 40 > len(data):
            raise ValueError("truncated COFF section table")
        raw_name = data[offset : offset + 8]
        sections.add(raw_name.rstrip(b"\0").decode("ascii", errors="replace"))
        relocation_count += struct.unpack_from("<H", data, offset + 32)[0]

    symbol_table_end = symbol_offset + (symbol_count * 18)
    if symbol_table_end + 4 > len(data):
        raise ValueError("truncated COFF symbol table")
    string_table = data[symbol_table_end:]

    global_symbols: set[str] = set()
    index = 0
    while index < symbol_count:
        offset = symbol_offset + (index * 18)
        raw_name = data[offset : offset + 8]
        zeroes, string_offset = struct.unpack_from("<LL", raw_name, 0)
        if zeroes == 0:
            name = _read_c_string(string_table, string_offset)
        else:
            name = raw_name.rstrip(b"\0").decode("ascii", errors="replace")
        section_number = struct.unpack_from("<h", data, offset + 12)[0]
        storage_class = data[offset + 16]
        auxiliary_count = data[offset + 17]
        if storage_class == 2 and section_number > 0:
            global_symbols.add(name)
        index += 1 + auxiliary_count

    return sections, global_symbols, relocation_count


def _inspect_elf64_object(data: bytes) -> tuple[set[str], set[str], int]:
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("not a little-endian ELF64 object")

    section_offset = struct.unpack_from("<Q", data, 40)[0]
    section_entry_size = struct.unpack_from("<H", data, 58)[0]
    section_count = struct.unpack_from("<H", data, 60)[0]
    section_names_index = struct.unpack_from("<H", data, 62)[0]
    if section_entry_size < 64 or section_names_index >= section_count:
        raise ValueError("invalid ELF64 section table")

    section_headers: list[tuple[int, int, int, int, int, int, int]] = []
    for index in range(section_count):
        offset = section_offset + (index * section_entry_size)
        if offset + 64 > len(data):
            raise ValueError("truncated ELF64 section table")
        name, section_type = struct.unpack_from("<II", data, offset)
        file_offset, size = struct.unpack_from("<QQ", data, offset + 24)
        link, info = struct.unpack_from("<II", data, offset + 40)
        entry_size = struct.unpack_from("<Q", data, offset + 56)[0]
        section_headers.append(
            (name, section_type, file_offset, size, link, info, entry_size)
        )

    _, _, names_offset, names_size, _, _, _ = section_headers[section_names_index]
    section_names = data[names_offset : names_offset + names_size]
    sections = {_read_c_string(section_names, header[0]) for header in section_headers}

    global_symbols: set[str] = set()
    relocation_count = 0
    for _, section_type, file_offset, size, link, _, entry_size in section_headers:
        if section_type in (4, 9):
            if entry_size == 0 or size % entry_size != 0:
                raise ValueError("invalid ELF64 relocation table")
            relocation_count += size // entry_size
        if section_type != 2:
            continue
        if link >= section_count:
            raise ValueError("invalid ELF64 symbol string table")
        _, _, strings_offset, strings_size, _, _, _ = section_headers[link]
        strings = data[strings_offset : strings_offset + strings_size]
        if entry_size < 24 or size % entry_size != 0:
            raise ValueError("invalid ELF64 symbol table")
        for symbol_offset in range(file_offset, file_offset + size, entry_size):
            if symbol_offset + entry_size > len(data):
                raise ValueError("truncated ELF64 symbol table")
            name = struct.unpack_from("<I", data, symbol_offset)[0]
            info = data[symbol_offset + 4]
            section_index = struct.unpack_from("<H", data, symbol_offset + 6)[0]
            if (info >> 4) == 1 and section_index != 0:
                global_symbols.add(_read_c_string(strings, name))

    return sections, global_symbols, relocation_count


def _inspect_object(path: Path) -> tuple[set[str], set[str], int]:
    data = path.read_bytes()
    if data.startswith(b"\x7fELF"):
        return _inspect_elf64_object(data)
    return _inspect_coff_object(data)


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

    def test_unsupported_include_source_has_stable_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_include_unsupported_") as temp:
            work = Path(temp)
            output = work / "غير-مدعوم-تضمين.نظم"
            source = (
                ROOT
                / "tests"
                / "integration"
                / "backend"
                / "backend_include_relative_alias_path_test.baa"
            )
            proc = self.run_baa(
                work,
                "--emit-nazm",
                "-I",
                str(ROOT),
                str(source),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 3, proc.stderr)
            self.assertIn("غير مدعومة", proc.stderr)
            self.assertNotIn("\x06", proc.stderr)
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
            self.assertIn(".globl الرئيسية", gas_text)
            self.assertNotRegex(gas_text, r"(?m)^\.globl main$")
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

            production_object = work / f"إنتاج{object_suffix}"
            compile_object = self.run_baa(
                work, "-c", str(source), "-o", str(production_object)
            )
            self.assertEqual(compile_object.returncode, 0, compile_object.stderr)

            for artifact in (production_object, shadow_object):
                sections, global_symbols, relocation_count = _inspect_object(artifact)
                self.assertIn(".text", sections)
                self.assertIn("الرئيسية", global_symbols)
                self.assertNotIn("main", global_symbols)
                self.assertEqual(relocation_count, 0)

    def test_supported_corpus_source_links_and_matches_runtime(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        source = (
            ROOT / "tests" / "integration" / "backend" / "backend_pp_nested_test.baa"
        )
        with tempfile.TemporaryDirectory(prefix="baa_nazm_corpus_shadow_") as temp:
            work = Path(temp)
            exe_suffix = ".exe" if os.name == "nt" else ""
            object_suffix = ".obj" if os.name == "nt" else ".o"
            output = work / f"برنامج-المصفوفة{exe_suffix}"
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
            nazm_text = shadow_source.read_text(encoding="utf-8")
            self.assertIsNone(re.search(r"[A-Za-z]", nazm_text))
            self.assertTrue(shadow_object.is_file())
            self.assertTrue(shadow_executable.is_file())

            production_run = subprocess.run(
                [str(output)], capture_output=True, timeout=30
            )
            shadow_run = subprocess.run(
                [str(shadow_executable)], capture_output=True, timeout=30
            )
            self.assertEqual(shadow_run.returncode, production_run.returncode)
            self.assertEqual(shadow_run.stdout, production_run.stdout)
            self.assertEqual(shadow_run.stderr, production_run.stderr)

            sections, global_symbols, relocation_count = _inspect_object(shadow_object)
            self.assertIn(".text", sections)
            self.assertIn("الرئيسية", global_symbols)
            self.assertNotIn("main", global_symbols)
            self.assertEqual(relocation_count, 0)


if __name__ == "__main__":
    unittest.main()
