#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SHADOW_MATRIX = ROOT / "docs" / "generated" / "baa_nazm_shadow_corpus_v1.json"


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

    raw_section_names: list[bytes] = []
    relocation_count = 0
    section_offset = 20 + optional_size
    for index in range(section_count):
        offset = section_offset + (index * 40)
        if offset + 40 > len(data):
            raise ValueError("truncated COFF section table")
        raw_name = data[offset : offset + 8]
        raw_section_names.append(raw_name)
        relocation_count += struct.unpack_from("<H", data, offset + 32)[0]

    symbol_table_end = symbol_offset + (symbol_count * 18)
    if symbol_table_end + 4 > len(data):
        raise ValueError("truncated COFF symbol table")
    string_table = data[symbol_table_end:]
    sections: set[str] = set()
    for raw_name in raw_section_names:
        short_name = raw_name.rstrip(b"\0").decode("ascii", errors="replace")
        if short_name.startswith("/") and short_name[1:].isdigit():
            sections.add(_read_c_string(string_table, int(short_name[1:])))
        else:
            sections.add(short_name)

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


def _debug_sections(sections: set[str]) -> set[str]:
    return {
        section
        for section in sections
        if section.startswith(".debug") or section.startswith(".zdebug")
    }


def _non_debug_sections(sections: set[str]) -> set[str]:
    return sections - _debug_sections(sections)


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
        ROOT.parent / "Nazm" / "build" / "eco-verify" / "nazm.exe",
        ROOT.parent / "Nazm" / "build" / "eco-verify" / "nazm",
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


def _find_c_compiler() -> Path | None:
    configured = os.environ.get("CC")
    if configured:
        candidate = Path(configured)
        if candidate.is_file():
            return candidate.resolve()
        command = shutil.which(configured)
        if command:
            return Path(command).resolve()

    for name in ("gcc", "cc", "clang"):
        command = shutil.which(name)
        if command:
            return Path(command).resolve()

    candidates: list[Path] = []
    if os.name == "nt":
        candidates.extend(
            [
                Path("C:/msys64/ucrt64/bin/gcc.exe"),
                Path("C:/msys64/mingw64/bin/gcc.exe"),
                Path("C:/msys64/clang64/bin/clang.exe"),
                Path(sys.executable).with_name("gcc.exe"),
                Path(sys.executable).with_name("clang.exe"),
            ]
        )
    else:
        candidates.extend((Path("/usr/bin/cc"), Path("/usr/bin/gcc")))

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    return None


class NazmEmitterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def run_baa(self, work: Path, *args: str) -> subprocess.CompletedProcess[str]:
        env = os.environ.copy()
        env["BAA_STDLIB"] = str(ROOT / "stdlib")
        compiler_driver = _find_c_compiler()
        if compiler_driver is not None:
            env["CC"] = str(compiler_driver)
            env["PATH"] = (
                str(compiler_driver.parent)
                + os.pathsep
                + env.get("PATH", "")
            )
        return subprocess.run(
            [str(self.baa), *args],
            cwd=str(work),
            env=env,
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
                    if target == "x86_64-linux":
                        self.assertIn("    ارجع\n", text)
                        self.assertNotIn("ناد_النظام", text)
                    self.assertIsNone(
                        re.search(r"[A-Za-z]", text),
                        "Nazm source contains a Latin letter",
                    )
                    source_map_path = Path(f"{output}.خريطة-باء.json")
                    source_map = json.loads(source_map_path.read_text(encoding="utf-8"))
                    self.assertEqual(source_map["schema"], "baa-nazm-source-map-v1")
                    self.assertEqual(
                        bytes.fromhex(source_map["generated_path_utf8_hex"]).decode("utf-8"),
                        str(output),
                    )
                    self.assertGreaterEqual(len(source_map["entries"]), 1)
                    mapped = source_map["entries"][0]
                    self.assertEqual(
                        bytes.fromhex(mapped["source_file_utf8_hex"]).decode("utf-8"),
                        str(source),
                    )
                    self.assertEqual(mapped["source_line"], 2)
                    generated_lines = text.splitlines()
                    for line_number in range(
                        mapped["generated_line_start"],
                        mapped["generated_line_end"] + 1,
                    ):
                        self.assertGreaterEqual(line_number, 1)
                        self.assertLessEqual(line_number, len(generated_lines))

    def test_nazm_is_a_normal_selectable_assembler(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_normal_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)

            emitted = work / "برنامج.نظم"
            emit = self.run_baa(
                work,
                "-S",
                "--assembler=nazm",
                f"--nazm-path={nazm}",
                str(source),
                "-o",
                str(emitted),
            )
            self.assertEqual(emit.returncode, 0, emit.stderr)
            text = emitted.read_text(encoding="utf-8")
            self.assertIn(".عام الرئيسية", text)
            self.assertIsNone(re.search(r"[A-Za-z]", text))

            object_suffix = ".obj" if os.name == "nt" else ".o"
            object_path = work / f"برنامج{object_suffix}"
            manifest_path = work / "بيان-البناء.json"
            arabic_bin = work / "أدوات"
            arabic_bin.mkdir()
            arabic_command = arabic_bin / (
                "نظم.exe" if os.name == "nt" else "نظم"
            )
            shutil.copy2(nazm, arabic_command)
            path_with_nazm = (
                str(arabic_bin)
                + os.pathsep
                + os.environ.get("PATH", "")
            )
            with mock.patch.dict(os.environ, {"PATH": path_with_nazm}):
                compile_object = self.run_baa(
                    work,
                    "-c",
                    "--assembler=nazm",
                    "--incremental",
                    "--emit-build-manifest",
                    str(manifest_path),
                    str(source),
                    "-o",
                    str(object_path),
                )
            self.assertEqual(compile_object.returncode, 0, compile_object.stderr)
            sections, global_symbols, _ = _inspect_object(object_path)
            self.assertIn(".text", sections)
            self.assertIn("الرئيسية", global_symbols)
            self.assertNotIn("main", global_symbols)
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["assembler"], "nazm")
            self.assertFalse(manifest["units"][0]["cache"]["enabled"])

            cross_target = (
                "x86_64-linux" if os.name == "nt" else "x86_64-windows"
            )
            cross_object = work / (
                "برنامج-عابر.o" if os.name == "nt" else "برنامج-عابر.obj"
            )
            cross_compile = self.run_baa(
                work,
                "-c",
                "--assembler=nazm",
                f"--nazm-path={nazm}",
                f"--target={cross_target}",
                str(source),
                "-o",
                str(cross_object),
            )
            self.assertEqual(cross_compile.returncode, 0, cross_compile.stderr)
            cross_sections, cross_symbols, _ = _inspect_object(cross_object)
            self.assertIn(".text", cross_sections)
            self.assertIn("الرئيسية", cross_symbols)

            exe_suffix = ".exe" if os.name == "nt" else ""
            executable = work / f"برنامج-تشغيل{exe_suffix}"
            link = self.run_baa(
                work,
                "--assembler=nazm",
                f"--nazm-path={nazm}",
                str(source),
                "-o",
                str(executable),
            )
            self.assertEqual(link.returncode, 0, link.stderr)
            run = subprocess.run(
                [str(executable)],
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(run.returncode, 0)

    def test_missing_normal_nazm_never_falls_back_to_gas(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_normal_missing_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            output = work / ("برنامج.obj" if os.name == "nt" else "برنامج.o")
            missing = work / "نظم-مفقود"
            proc = self.run_baa(
                work,
                "-c",
                "--assembler=nazm",
                f"--nazm-path={missing}",
                str(source),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 4, proc.stderr)
            self.assertIn("فشل مجمّع نظم", proc.stderr)
            self.assertFalse(output.exists())

    def test_unknown_assembler_is_invalid_invocation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_unknown_assembler_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            proc = self.run_baa(
                work,
                "--assembler=مجهول",
                str(source),
            )
            self.assertEqual(proc.returncode, 2, proc.stderr)

    def test_direct_nazm_source_compiles_and_links_with_baa(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_direct_nazm_") as temp:
            work = Path(temp)
            baa_source = work / "الرئيسية.baa"
            baa_source.write_text(
                "خارجي صحيح قيمة_نظم().\n"
                "صحيح الرئيسية() {\n"
                "    إرجع قيمة_نظم() - ٧.\n"
                "}\n",
                encoding="utf-8",
            )
            nazm_source = work / "مساعدة.نظم"
            nazm_source.write_text(
                ".نص\n"
                ".عام قيمة_نظم\n"
                "قيمة_نظم:\n"
                "    انقل سجل_المركم_٣٢، ٧\n"
                "    ارجع\n",
                encoding="utf-8",
            )

            object_path = work / (
                "مساعدة.obj" if os.name == "nt" else "مساعدة.o"
            )
            direct_object = self.run_baa(
                work,
                "-c",
                f"--nazm-path={nazm}",
                str(nazm_source),
                "-o",
                str(object_path),
            )
            self.assertEqual(direct_object.returncode, 0, direct_object.stderr)
            sections, symbols, _ = _inspect_object(object_path)
            self.assertIn(".text", sections)
            self.assertIn("قيمة_نظم", symbols)

            cross_target = (
                "x86_64-linux" if os.name == "nt" else "x86_64-windows"
            )
            cross_object = work / (
                "مساعدة-عابرة.o" if os.name == "nt" else "مساعدة-عابرة.obj"
            )
            direct_cross = self.run_baa(
                work,
                "-c",
                f"--nazm-path={nazm}",
                f"--target={cross_target}",
                str(nazm_source),
                "-o",
                str(cross_object),
            )
            self.assertEqual(direct_cross.returncode, 0, direct_cross.stderr)
            cross_sections, cross_symbols, _ = _inspect_object(cross_object)
            self.assertIn(".text", cross_sections)
            self.assertIn("قيمة_نظم", cross_symbols)

            manifest_path = work / "بيان-مختلط.json"
            executable = work / (
                "برنامج-مختلط.exe" if os.name == "nt" else "برنامج-مختلط"
            )
            path_with_nazm = (
                str(nazm.parent)
                + os.pathsep
                + os.environ.get("PATH", "")
            )
            with mock.patch.dict(os.environ, {"PATH": path_with_nazm}):
                mixed = self.run_baa(
                    work,
                    "--incremental",
                    "--emit-build-manifest",
                    str(manifest_path),
                    str(baa_source),
                    str(nazm_source),
                    "-o",
                    str(executable),
                )
            self.assertEqual(mixed.returncode, 0, mixed.stderr)
            run = subprocess.run(
                [str(executable)],
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(run.returncode, 0)

            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["assembler"], "gas")
            self.assertEqual(
                [
                    (unit["source_kind"], unit["assembler"])
                    for unit in manifest["units"]
                ],
                [("baa", "gas"), ("nazm", "nazm")],
            )
            self.assertFalse(manifest["units"][1]["cache"]["enabled"])
            self.assertEqual(
                manifest["units"][1]["cache"]["reason"],
                "nazm-input",
            )

    def test_direct_nazm_failures_and_unsupported_modes_are_visible(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_direct_nazm_fail_") as temp:
            work = Path(temp)
            source = work / "خاطئ.نظم"
            source.write_text(
                ".نص\n"
                ".عام خاطئ\n"
                "خاطئ:\n"
                "    تعليمة_غير_موجودة\n",
                encoding="utf-8",
            )
            output = work / ("خاطئ.obj" if os.name == "nt" else "خاطئ.o")
            nazm = _find_nazm()
            if nazm is not None:
                invalid = self.run_baa(
                    work,
                    "-c",
                    f"--nazm-path={nazm}",
                    str(source),
                    "-o",
                    str(output),
                )
                self.assertEqual(invalid.returncode, 1, invalid.stderr)
                self.assertFalse(output.exists())

            check = self.run_baa(work, "--check", str(source))
            self.assertEqual(check.returncode, 3, check.stderr)
            self.assertIn("ملفات `.نظم` المباشرة", check.stderr)

            missing = work / "نظم-مفقود"
            missing_tool = self.run_baa(
                work,
                "-c",
                f"--nazm-path={missing}",
                str(source),
                "-o",
                str(output),
            )
            self.assertEqual(missing_tool.returncode, 4, missing_tool.stderr)
            self.assertFalse(output.exists())

    def test_local_jump_source_is_canonical_arabic_for_both_targets(self) -> None:
        source = (
            ROOT / "tests" / "integration" / "backend" / "backend_mod_test.baa"
        )
        with tempfile.TemporaryDirectory(prefix="baa_nazm_jump_") as temp:
            work = Path(temp)
            for target in ("x86_64-windows", "x86_64-linux"):
                with self.subTest(target=target):
                    output = work / f"قفز-{target}.نظم"
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
                    self.assertIn("اقفز كتلة_٠_٦", text)
                    self.assertIn("كتلة_٠_٦:", text)
                    self.assertIsNone(re.search(r"[A-Za-z]", text))

    def test_scalar_decimal_source_is_arabic_and_assembles_for_both_targets(
        self,
    ) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_decimal_") as temp:
            work = Path(temp)
            source = work / "حساب-عشري.باء"
            source.write_text(
                "عشري احسب(عشري أ، عشري ب) {\n"
                "    عشري ج = أ + ب.\n"
                "    ج = ج - ب.\n"
                "    ج = ج * ب.\n"
                "    ج = ج / ب.\n"
                "    إذا (ج > ب) {\n"
                "        ج = -ج.\n"
                "    }\n"
                "    إرجع ج.\n"
                "}\n"
                "صحيح الرئيسية() {\n"
                "    عشري ناتج = احسب(كـ<عشري>(٦)، ٢.٠).\n"
                "    اطبع ناتج.\n"
                "    إرجع كـ<صحيح>(ناتج) + ٦.\n"
                "}\n",
                encoding="utf-8",
            )

            expected_mnemonics = (
                "جمع_عشري",
                "طرح_عشري",
                "ضرب_عشري",
                "قسمة_عشرية",
                "مقارنة_عشرية",
                "خلاف_عشري",
                "تحويل_صحيح_إلى_عشري",
                "تحويل_عشري_إلى_صحيح",
            )
            for target, object_format, object_suffix in (
                ("x86_64-windows", "كوف", ".obj"),
                ("x86_64-linux", "إلف64", ".o"),
            ):
                with self.subTest(target=target):
                    emitted = work / f"عشري-{target}.نظم"
                    proc = self.run_baa(
                        work,
                        "--emit-nazm",
                        f"--target={target}",
                        str(source),
                        "-o",
                        str(emitted),
                    )
                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    text = emitted.read_text(encoding="utf-8")
                    self.assertIsNone(
                        re.search(r"[A-Za-z]", text),
                        "Nazm source contains a Latin letter",
                    )
                    self.assertIn("سجل_عشري_٠", text)
                    for mnemonic in expected_mnemonics:
                        self.assertIn(mnemonic, text)
                    if target == "x86_64-linux":
                        self.assertRegex(
                            text,
                            r"انقل سجل_المركم_٣٢، ١\s+ناد اطبع_منسقا",
                        )

                    object_path = work / f"عشري-{target}{object_suffix}"
                    assembled = subprocess.run(
                        [
                            str(nazm),
                            "-ص",
                            object_format,
                            "-خ",
                            str(object_path),
                            str(emitted),
                        ],
                        cwd=str(work),
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                    )
                    self.assertEqual(assembled.returncode, 0, assembled.stderr)
                    self.assertTrue(object_path.is_file())

            exe_suffix = ".exe" if os.name == "nt" else ""
            output = work / f"برنامج-عشري{exe_suffix}"
            shadow = self.run_baa(
                work,
                f"--nazm-shadow={nazm}",
                str(source),
                "-o",
                str(output),
            )
            self.assertEqual(shadow.returncode, 0, shadow.stderr)
            production_run = subprocess.run(
                [str(output)], capture_output=True, timeout=30
            )
            shadow_run = subprocess.run(
                [str(Path(f"{output}.ظل-نظم{exe_suffix}"))],
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(production_run.returncode, 0)
            self.assertEqual(shadow_run.returncode, production_run.returncode)
            self.assertEqual(shadow_run.stdout, production_run.stdout)
            self.assertEqual(shadow_run.stderr, production_run.stderr)

    def test_baa_string_table_uses_arabic_read_only_symbols(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_strings_") as temp:
            work = Path(temp)
            source = work / "سلاسل.باء"
            source.write_text(
                "نص رسالة_عامة = \"Baa باء\".\n"
                "صحيح الرئيسية() {\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            for target in ("x86_64-windows", "x86_64-linux"):
                with self.subTest(target=target):
                    output = work / f"سلاسل-{target}.نظم"
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
                    self.assertIn(".بيانات_للقراءة", text)
                    self.assertIn("سلسلة_باء_٠:", text)
                    self.assertIn(".محاذاة ٨", text)
                    self.assertIn(".عدد٦٤", text)
                    self.assertIsNone(
                        re.search(r"[A-Za-z]", text),
                        "Nazm source contains a Latin letter",
                    )

    def test_baa_string_shadow_links_read_only_data_and_matches_runtime(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_string_shadow_") as temp:
            work = Path(temp)
            source = work / "سلاسل.باء"
            source.write_text(
                "نص رسالة_عامة = \"Baa باء\".\n"
                "صحيح الرئيسية() {\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )
            exe_suffix = ".exe" if os.name == "nt" else ""
            object_suffix = ".obj" if os.name == "nt" else ".o"
            output = work / f"برنامج-سلاسل{exe_suffix}"
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
            self.assertIn(".بيانات_للقراءة", nazm_text)
            self.assertIn("سلسلة_باء_٠:", nazm_text)

            sections, _, relocation_count = _inspect_object(shadow_object)
            self.assertIn(".rdata" if os.name == "nt" else ".rodata", sections)
            self.assertGreaterEqual(relocation_count, 1)

            production_run = subprocess.run(
                [str(output)],
                stdin=subprocess.DEVNULL,
                capture_output=True,
                timeout=30,
            )
            repetitions = 25 if os.name == "nt" else 1
            for _ in range(repetitions):
                shadow_run = subprocess.run(
                    [str(shadow_executable)],
                    stdin=subprocess.DEVNULL,
                    capture_output=True,
                    timeout=30,
                )
                self.assertEqual(shadow_run.returncode, production_run.returncode)
                self.assertEqual(shadow_run.stdout, production_run.stdout)
                self.assertEqual(shadow_run.stderr, production_run.stderr)

    def test_spilled_pointer_store_preserves_the_r11_address(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_spilled_pointer_") as temp:
            work = Path(temp)
            output = work / "تخزين-مؤشر.نظم"
            proc = self.run_baa(
                work,
                "--emit-nazm",
                "--target=x86_64-windows",
                "-I",
                str(ROOT),
                str(ROOT / "examples" / "error_handling_demo.baa"),
                "-o",
                str(output),
            )
            self.assertEqual(proc.returncode, 0, proc.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIn(
                "انقل سجل_المركم، ٠\n    انقل [سجل_عام_١١]، سجل_المركم",
                text,
            )
            self.assertNotIn(
                "انقل سجل_عام_١١، ٠\n    انقل [سجل_عام_١١]، سجل_عام_١١",
                text,
            )

    def test_global_function_pointer_uses_arabic_data_relocation(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_global_data_") as temp:
            work = Path(temp)
            source = work / "بيانات.باء"
            source.write_text(
                "صحيح ضاعف(صحيح س) {\n"
                "    إرجع س + س.\n"
                "}\n"
                "دالة(صحيح) -> صحيح مؤشر_عام = ضاعف.\n"
                "صحيح الرئيسية() {\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            exe_suffix = ".exe" if os.name == "nt" else ""
            object_suffix = ".obj" if os.name == "nt" else ".o"
            output = work / f"برنامج-بيانات{exe_suffix}"
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
            self.assertIn(".بيانات", nazm_text)
            self.assertIn("مؤشر_عام: .عدد٦٤ ضاعف", nazm_text)

            sections, global_symbols, relocation_count = _inspect_object(
                shadow_object
            )
            self.assertIn(".data", sections)
            self.assertIn("مؤشر_عام", global_symbols)
            self.assertGreaterEqual(relocation_count, 1)

            production_run = subprocess.run(
                [str(output)], capture_output=True, timeout=30
            )
            shadow_run = subprocess.run(
                [str(shadow_executable)], capture_output=True, timeout=30
            )
            self.assertEqual(shadow_run.returncode, production_run.returncode)
            self.assertEqual(shadow_run.stdout, production_run.stdout)
            self.assertEqual(shadow_run.stderr, production_run.stderr)

    def test_global_integer_uses_arabic_pc_relative_memory(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_global_value_") as temp:
            work = Path(temp)
            source = work / "قيمة-عامة.باء"
            source.write_text(
                "صحيح عداد_عام = ٧.\n"
                "صحيح الرئيسية() {\n"
                "    عداد_عام = عداد_عام + ٥.\n"
                "    إرجع عداد_عام.\n"
                "}\n",
                encoding="utf-8",
            )

            for target in ("x86_64-windows", "x86_64-linux"):
                with self.subTest(target=target):
                    emitted = work / f"قيمة-{target}.نظم"
                    proc = self.run_baa(
                        work,
                        "--emit-nazm",
                        f"--target={target}",
                        str(source),
                        "-o",
                        str(emitted),
                    )
                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    text = emitted.read_text(encoding="utf-8")
                    self.assertIsNone(re.search(r"[A-Za-z]", text))
                    self.assertGreaterEqual(
                        text.count("[مؤشر_التعليمة+عداد_عام]"), 3
                    )

            exe_suffix = ".exe" if os.name == "nt" else ""
            object_suffix = ".obj" if os.name == "nt" else ".o"
            output = work / f"قيمة-عامة{exe_suffix}"
            shadow = self.run_baa(
                work,
                f"--nazm-shadow={nazm}",
                str(source),
                "-o",
                str(output),
            )
            self.assertEqual(shadow.returncode, 0, shadow.stderr)

            shadow_object = Path(f"{output}.ظل-نظم{object_suffix}")
            sections, global_symbols, relocation_count = _inspect_object(
                shadow_object
            )
            self.assertIn(".data", sections)
            self.assertIn("عداد_عام", global_symbols)
            self.assertGreaterEqual(relocation_count, 3)

            production_run = subprocess.run(
                [str(output)], capture_output=True, timeout=30
            )
            shadow_run = subprocess.run(
                [str(Path(f"{output}.ظل-نظم{exe_suffix}"))],
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(production_run.returncode, 12)
            self.assertEqual(shadow_run.returncode, production_run.returncode)
            self.assertEqual(shadow_run.stdout, production_run.stdout)
            self.assertEqual(shadow_run.stderr, production_run.stderr)

    def test_structured_arch_operations_emit_canonical_arabic(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_arch_ops_") as temp:
            work = Path(temp)
            output = work / "عمليات-معمارية.نظم"
            proc = self.run_baa(
                work,
                "--emit-nazm",
                "-I",
                str(ROOT),
                str(
                    ROOT
                    / "tests"
                    / "integration"
                    / "ir"
                    / "ir_structured_arch_ops_test.baa"
                ),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 0, proc.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIn("اقرأ_عداد_الزمن", text)
            self.assertIn("لا_تفعل", text)
            self.assertIn("ازح_يسارا سجل_البيانات، ٣٢", text)
            self.assertIn("أو_بتيا سجل_المركم، سجل_البيانات", text)
            self.assertIsNone(re.search(r"[A-Za-z]", text))
            self.assertTrue(Path(f"{output}.خريطة-باء.json").is_file())

    def test_raw_inline_asm_is_visible_and_leaves_no_nazm_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_raw_inline_") as temp:
            work = Path(temp)
            source = work / "خام.baa"
            source.write_text(
                """
صحيح الرئيسية() {
    مجمع {
        "nop"
    }
    إرجع ٠.
}
""".strip()
                + "\n",
                encoding="utf-8",
            )
            output = work / "غير-مدعوم.نظم"
            proc = self.run_baa(
                work,
                "--emit-nazm",
                str(source),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 1, proc.stderr)
            self.assertIn("أزيلت جملة 'مجمع' الخام", proc.stderr)
            self.assertIn("اقرأ_عداد_الزمن()", proc.stderr)
            self.assertIn("ملف '.نظم'", proc.stderr)
            self.assertFalse(output.exists())
            self.assertFalse(Path(f"{output}.خريطة-باء.json").exists())

    def test_custom_startup_flag_does_not_duplicate_hosted_startup(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_custom_startup_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)

            for target in ("x86_64-windows", "x86_64-linux"):
                with self.subTest(target=target):
                    output = work / f"بدء-{target}.نظم"
                    proc = self.run_baa(
                        work,
                        "--emit-nazm",
                        "--startup=custom",
                        f"--target={target}",
                        str(source),
                        "-o",
                        str(output),
                    )

                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    text = output.read_text(encoding="utf-8")
                    self.assertIn(".عام الرئيسية", text)
                    self.assertNotIn("الرئيسية_بدء", text)
                    self.assertIsNone(re.search(r"[A-Za-z]", text))
                    self.assertTrue(
                        Path(f"{output}.خريطة-باء.json").is_file()
                    )

    def test_debug_info_emits_target_specific_object_sections(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_debug_info_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)

            for target, object_format, suffix, debug_section in (
                ("x86_64-windows", "كوف", ".obj", ".debug$S"),
                ("x86_64-linux", "إلف64", ".o", ".debug_line"),
            ):
                with self.subTest(target=target):
                    output = work / f"تنقيح-{target}.نظم"
                    proc = self.run_baa(
                        work,
                        "--emit-nazm",
                        "--debug-info",
                        f"--target={target}",
                        str(source),
                        "-o",
                        str(output),
                    )

                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    self.assertTrue(output.is_file())
                    self.assertTrue(
                        Path(f"{output}.خريطة-باء.json").is_file()
                    )
                    text = output.read_text(encoding="utf-8")
                    self.assertIn(".ملف_بايتات ١، ", text)
                    self.assertRegex(text, r"\.موضع ١، \d+، \d+")
                    self.assertNotIn(".file", text)
                    self.assertNotIn(".loc", text)

                    object_path = work / f"تنقيح-{target}{suffix}"
                    assembled = subprocess.run(
                        [
                            str(nazm),
                            "-ص",
                            object_format,
                            "-خ",
                            str(object_path),
                            str(output),
                        ],
                        cwd=str(work),
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                    )
                    self.assertEqual(
                        assembled.returncode, 0, assembled.stderr
                    )
                    sections, _, relocation_count = _inspect_object(
                        object_path
                    )
                    self.assertIn(debug_section, sections)
                    self.assertGreaterEqual(relocation_count, 1)

            exe_suffix = ".exe" if os.name == "nt" else ""
            executable = work / f"برنامج-تنقيح{exe_suffix}"
            shadow = self.run_baa(
                work,
                "--debug-info",
                f"--nazm-shadow={nazm}",
                str(source),
                "-o",
                str(executable),
            )
            self.assertEqual(shadow.returncode, 0, shadow.stderr)
            production_run = subprocess.run(
                [str(executable)], capture_output=True, timeout=30
            )
            shadow_run = subprocess.run(
                [str(Path(f"{executable}.ظل-نظم{exe_suffix}"))],
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(
                shadow_run.returncode, production_run.returncode
            )
            self.assertEqual(shadow_run.stdout, production_run.stdout)
            self.assertEqual(shadow_run.stderr, production_run.stderr)

    def test_stack_protector_has_a_distinct_nazm_blocker(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_stack_guard_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            output = work / "حماية.نظم"
            proc = self.run_baa(
                work,
                "--emit-nazm",
                "--target=x86_64-linux",
                "-fstack-protector",
                str(source),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 3, proc.stderr)
            self.assertIn("[عائق_نظم=حماية_المكدس]", proc.stderr)
            self.assertFalse(output.exists())
            self.assertFalse(Path(f"{output}.خريطة-باء.json").exists())

    def test_include_source_global_uses_stable_arabic_symbol(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_include_global_") as temp:
            work = Path(temp)
            output = work / "غير-مدعوم-تضمين.نظم"
            source = (
                ROOT
                / "tests"
                / "integration"
                / "backend"
                / "backend_include_bom_test.baa"
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

            self.assertEqual(proc.returncode, 0, proc.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIsNone(re.search(r"[A-Za-z]", text))
            self.assertIn("[مؤشر_التعليمة+قيمة_مضمنة]", text)
            source_map = Path(f"{output}.خريطة-باء.json")
            self.assertTrue(source_map.is_file())

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

    def test_shadow_assembler_failure_maps_back_to_baa_source(self) -> None:
        compiler = _find_c_compiler()
        if compiler is None:
            self.skipTest("A C compiler is required for the diagnostic adapter fixture")

        with tempfile.TemporaryDirectory(prefix="baa_nazm_source_map_") as temp:
            work = Path(temp)
            source = self.write_minimal_source(work)
            fake_source = work / "fake_nazm.c"
            fake_executable = work / ("fake-nazm.exe" if os.name == "nt" else "fake-nazm")
            fake_source.write_text(
                "#include <stdio.h>\n"
                "#include <string.h>\n"
                "#ifdef _WIN32\n"
                "#include <windows.h>\n"
                "#include <wchar.h>\n"
                "static FILE *open_input(const wchar_t *path) { return _wfopen(path, L\"rb\"); }\n"
                "static int adapter_main(int argc, wchar_t **argv) {\n"
                "  if (argc < 2) return 2;\n"
                "  FILE *input = open_input(argv[argc - 1]);\n"
                "  char path[32768];\n"
                "  if (WideCharToMultiByte(CP_UTF8, 0, argv[argc - 1], -1, path,\n"
                "                          (int)sizeof(path), NULL, NULL) <= 0) return 3;\n"
                "#else\n"
                "static FILE *open_input(const char *path) { return fopen(path, \"rb\"); }\n"
                "static int adapter_main(int argc, char **argv) {\n"
                "  if (argc < 2) return 2;\n"
                "  FILE *input = open_input(argv[argc - 1]);\n"
                "  const char *path = argv[argc - 1];\n"
                "#endif\n"
                "  if (!input) return 4;\n"
                "  char text[4096];\n"
                "  int line = 0;\n"
                "  int previous_was_span = 0;\n"
                "  while (fgets(text, (int)sizeof(text), input)) {\n"
                "    ++line;\n"
                "    if (previous_was_span && strncmp(text, \"    \" , 4) == 0 && text[4] != ';') break;\n"
                "    previous_was_span = strncmp(text, \"    ;\", 5) == 0;\n"
                "  }\n"
                "  fclose(input);\n"
                '  fprintf(stderr, "%s:%d:1: synthetic nazm failure\\n", path, line);\n'
                "  return 9;\n"
                "}\n"
                "#ifdef _WIN32\n"
                "int wmain(int argc, wchar_t **argv) { return adapter_main(argc, argv); }\n"
                "#else\n"
                "int main(int argc, char **argv) { return adapter_main(argc, argv); }\n"
                "#endif\n",
                encoding="utf-8",
            )
            compiler_env = os.environ.copy()
            compiler_parent = compiler.parent
            if compiler_parent != Path("."):
                compiler_env["PATH"] = (
                    str(compiler_parent)
                    + os.pathsep
                    + compiler_env.get("PATH", "")
                )
            build = subprocess.run(
                [
                    str(compiler),
                    *(["-municode"] if os.name == "nt" else []),
                    str(fake_source),
                    "-o",
                    str(fake_executable),
                ],
                cwd=str(work),
                env=compiler_env,
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=30,
            )
            self.assertEqual(build.returncode, 0, build.stderr)

            output = work / ("برنامج.exe" if os.name == "nt" else "برنامج")
            proc = self.run_baa(
                work,
                f"--nazm-shadow={fake_executable}",
                str(source),
                "-o",
                str(output),
            )

            self.assertEqual(proc.returncode, 4, proc.stderr)
            self.assertIn("synthetic nazm failure", proc.stderr)
            self.assertIn("موضع باء الأصلي:", proc.stderr)
            self.assertIn(str(source), proc.stderr)
            self.assertRegex(proc.stderr, re.escape(str(source)) + r":2:\d+")
            self.assertFalse(output.exists(), "GAS output must not hide a mapped Nazm failure")
            shadow_source = Path(f"{output}.ظل-نظم.نظم")
            self.assertFalse(shadow_source.exists())
            self.assertFalse(Path(f"{shadow_source}.خريطة-باء.json").exists())

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
            shadow_map = Path(f"{shadow_source}.خريطة-باء.json")
            shadow_object = Path(f"{output}.ظل-نظم{object_suffix}")
            shadow_executable = Path(f"{output}.ظل-نظم{exe_suffix}")
            self.assertTrue(shadow_source.is_file())
            self.assertTrue(shadow_map.is_file())
            self.assertTrue(shadow_object.is_file())
            self.assertTrue(shadow_executable.is_file())

            source_map = json.loads(shadow_map.read_text(encoding="utf-8"))
            self.assertEqual(source_map["schema"], "baa-nazm-source-map-v1")
            self.assertGreaterEqual(len(source_map["entries"]), 1)

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

    def test_supported_corpus_sources_link_and_match_runtime(self) -> None:
        nazm = _find_nazm()
        if nazm is None:
            self.skipTest("Nazm executable is unavailable in this checkout")

        target = "x86_64-windows" if os.name == "nt" else "x86_64-linux"
        matrix = json.loads(SHADOW_MATRIX.read_text(encoding="utf-8"))
        rows = [
            row
            for row in matrix["targets"][target]["sources"]
            if row["status"] == "emitted"
        ]
        self.assertEqual(
            len(rows), matrix["targets"][target]["summary"]["emitted"]
        )
        for row in rows:
            source = ROOT / row["source"]
            compile_only = bool(re.search(
                r"^//\s*RUN:\s*compile-only\s*$",
                source.read_text(encoding="utf-8-sig"),
                re.MULTILINE,
            ))
            flags = ["-I", str(ROOT), *row.get("flags", [])]
            for index, flag in enumerate(flags):
                if index > 0 and flags[index - 1] == "-I":
                    candidate = Path(flag)
                    if not candidate.is_absolute():
                        flags[index] = str(ROOT / candidate)
                elif flag.startswith("-I") and len(flag) > 2:
                    candidate = Path(flag[2:])
                    if not candidate.is_absolute():
                        flags[index] = f"-I{ROOT / candidate}"
            with self.subTest(source=source.name), tempfile.TemporaryDirectory(
                prefix="baa_nazm_corpus_shadow_"
            ) as temp:
                work = Path(temp)
                exe_suffix = ".exe" if os.name == "nt" else ""
                object_suffix = ".obj" if os.name == "nt" else ".o"
                if compile_only:
                    shadow_source = work / "برنامج-المصفوفة.نظم"
                    shadow_object = work / f"ظل-المصفوفة{object_suffix}"
                    emit = self.run_baa(
                        work,
                        "--emit-nazm",
                        f"--target={target}",
                        *flags,
                        str(source),
                        "-o",
                        str(shadow_source),
                    )
                    self.assertEqual(emit.returncode, 0, emit.stderr)
                    nazm_text = shadow_source.read_text(encoding="utf-8")
                    self.assertIsNone(re.search(r"[A-Za-z]", nazm_text))
                    assemble = subprocess.run(
                        [
                            str(nazm),
                            "-ص",
                            "كوف" if os.name == "nt" else "إلف64",
                            "-خ",
                            str(shadow_object),
                            str(shadow_source),
                        ],
                        cwd=str(work),
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                    )
                    self.assertEqual(assemble.returncode, 0, assemble.stderr)

                    production_object = work / f"إنتاج-المصفوفة{object_suffix}"
                    production = self.run_baa(
                        work,
                        "-c",
                        *flags,
                        str(source),
                        "-o",
                        str(production_object),
                    )
                    self.assertEqual(production.returncode, 0, production.stderr)
                    sections, global_symbols, _ = _inspect_object(shadow_object)
                    (
                        production_sections,
                        production_global_symbols,
                        _,
                    ) = _inspect_object(production_object)
                    self.assertIn(".text", sections)
                    self.assertIn("الرئيسية", global_symbols)
                    self.assertTrue(
                        _non_debug_sections(sections).issubset(
                            _non_debug_sections(production_sections)
                        )
                    )
                    if "--debug-info" in flags:
                        self.assertTrue(_debug_sections(sections))
                        self.assertTrue(_debug_sections(production_sections))
                    self.assertEqual(global_symbols, production_global_symbols)
                    continue

                output = work / f"برنامج-المصفوفة{exe_suffix}"
                proc = self.run_baa(
                    work,
                    f"--nazm-shadow={nazm}",
                    *flags,
                    str(source),
                    "-o",
                    str(output),
                )
                self.assertEqual(proc.returncode, 0, proc.stderr)

                shadow_source = Path(f"{output}.ظل-نظم.نظم")
                shadow_map = Path(f"{shadow_source}.خريطة-باء.json")
                shadow_object = Path(f"{output}.ظل-نظم{object_suffix}")
                shadow_executable = Path(f"{output}.ظل-نظم{exe_suffix}")
                nazm_text = shadow_source.read_text(encoding="utf-8")
                self.assertIsNone(re.search(r"[A-Za-z]", nazm_text))
                source_map = json.loads(shadow_map.read_text(encoding="utf-8"))
                self.assertEqual(source_map["schema"], "baa-nazm-source-map-v1")
                self.assertGreaterEqual(len(source_map["entries"]), 1)
                self.assertTrue(shadow_object.is_file())
                self.assertTrue(shadow_executable.is_file())

                production_object = work / f"إنتاج-المصفوفة{object_suffix}"
                production_object_proc = self.run_baa(
                    work,
                    "-c",
                    *flags,
                    str(source),
                    "-o",
                    str(production_object),
                )
                self.assertEqual(
                    production_object_proc.returncode,
                    0,
                    production_object_proc.stderr,
                )

                production_run = subprocess.run(
                    [str(output)],
                    stdin=subprocess.DEVNULL,
                    capture_output=True,
                    timeout=30,
                )
                shadow_run = subprocess.run(
                    [str(shadow_executable)],
                    stdin=subprocess.DEVNULL,
                    capture_output=True,
                    timeout=30,
                )
                self.assertEqual(shadow_run.returncode, production_run.returncode)
                self.assertEqual(shadow_run.stdout, production_run.stdout)
                self.assertEqual(shadow_run.stderr, production_run.stderr)
                if source.name == "math_and_format.baa":
                    self.assertIn(b"0.4794", production_run.stdout)
                    self.assertIn(b"4.794255e-01", production_run.stdout)

                # Raw relocation counts are not a semantic parity measure here:
                # Nazm resolves same-object symbols during assembly while GAS
                # commonly leaves them for the linker. Successful Arabic-startup
                # linking plus identical process behavior proves that the shadow
                # object retained every externally required relocation.
                sections, global_symbols, _ = _inspect_object(shadow_object)
                (
                    production_sections,
                    production_global_symbols,
                    _,
                ) = _inspect_object(production_object)
                self.assertIn(".text", sections)
                self.assertIn("الرئيسية", global_symbols)
                self.assertNotIn("main", global_symbols)
                self.assertTrue(
                    _non_debug_sections(sections).issubset(
                        _non_debug_sections(production_sections)
                    )
                )
                if "--debug-info" in flags:
                    self.assertTrue(_debug_sections(sections))
                    self.assertTrue(_debug_sections(production_sections))
                self.assertEqual(global_symbols, production_global_symbols)


if __name__ == "__main__":
    unittest.main()
