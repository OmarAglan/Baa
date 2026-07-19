#!/usr/bin/env python3

from __future__ import annotations

import ctypes
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ENTRY_SYMBOL = "الرئيسية_بدء"
HELPER_SYMBOL = "دالة_مساعدة"


def _find_baa() -> Path | None:
    env = os.environ.get("BAA")
    if env:
        candidate = Path(env)
        if candidate.is_file():
            return candidate

    candidates = (
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
    )
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def _find_gcc() -> Path:
    explicit = os.environ.get("BAA_GCC")
    if explicit:
        candidate = Path(explicit)
        if candidate.is_file():
            return candidate
        raise FileNotFoundError(f"BAA_GCC points to a missing compiler: {candidate}")

    baa = _find_baa()
    if baa:
        candidates = (
            baa.parent / "gcc" / "bin" / "gcc.exe",
            baa.parent.parent / "gcc" / "bin" / "gcc.exe",
        )
        for candidate in candidates:
            if candidate.is_file():
                return candidate

    from_path = shutil.which("gcc")
    if from_path:
        return Path(from_path)

    common_msys2 = Path(r"C:\msys64\ucrt64\bin\gcc.exe")
    if common_msys2.is_file():
        return common_msys2

    raise FileNotFoundError(
        "Could not find the Windows GCC used by Baa; set BAA_GCC or add gcc to PATH"
    )


def _make_long_unicode_directory(root: Path) -> Path:
    current = root / "مسار عربي طويل مع مسافات"
    index = 0
    probe_name = "مدخل تجميع طويل.s"
    while len(str(current / probe_name)) < 220:
        current /= f"طبقة {index:02d} طويلة " + ("ع" * 16)
        index += 1
    current.mkdir(parents=True)
    return current


def _windows_short_path(path: Path) -> str:
    buffer = ctypes.create_unicode_buffer(32768)
    length = ctypes.windll.kernel32.GetShortPathNameW(
        str(path),
        buffer,
        len(buffer),
    )
    if length == 0 or length >= len(buffer):
        raise OSError(f"GetShortPathNameW failed for {path}")
    short_path = buffer.value
    if not short_path.isascii():
        raise OSError(f"short path is not ASCII: {short_path}")
    return short_path


@unittest.skipUnless(os.name == "nt", "Windows GCC/LD Unicode path capability matrix")
class ToolchainUnicodePathTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gcc = _find_gcc()
        cls.baa = _find_baa()
        if cls.baa is None:
            raise FileNotFoundError("Could not find the Baa compiler; set BAA or build it")

    def run_checked(
        self,
        argv: list[str],
        *,
        cwd: Path,
        timeout: float = 30.0,
    ) -> subprocess.CompletedProcess[str]:
        env = os.environ.copy()
        env["PATH"] = str(self.gcc.parent) + os.pathsep + env.get("PATH", "")
        proc = subprocess.run(
            argv,
            cwd=str(cwd),
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=timeout,
        )
        self.assertEqual(
            proc.returncode,
            0,
            "command failed:\n"
            + subprocess.list2cmdline(argv)
            + f"\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}",
        )
        return proc

    def test_real_unicode_artifacts_via_no_copy_short_aliases(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_unicode_toolchain_") as temp:
            work = _make_long_unicode_directory(Path(temp))
            source_one = work / "مدخل أول مع مسافات.s"
            source_two = work / "مدخل ثان مع مسافات.s"
            object_one = work / "كائن أول مع مسافات.o"
            object_two = work / "كائن ثان مع مسافات.o"
            response = work / "خيارات رابط عربية.rsp"
            executable = work / "برنامج عربي نهائي.exe"

            source_one.write_text(
                ".text\n"
                f".globl {ENTRY_SYMBOL}\n"
                ".extern ExitProcess\n"
                f".extern {HELPER_SYMBOL}\n"
                f"{ENTRY_SYMBOL}:\n"
                "    andq $-16, %rsp\n"
                "    subq $32, %rsp\n"
                f"    call {HELPER_SYMBOL}\n"
                "    movl %eax, %ecx\n"
                "    call ExitProcess\n"
                "    hlt\n",
                encoding="utf-8",
            )
            source_two.write_text(
                ".text\n"
                f".globl {HELPER_SYMBOL}\n"
                f"{HELPER_SYMBOL}:\n"
                "    xorl %eax, %eax\n"
                "    ret\n",
                encoding="utf-8",
            )
            response.write_text(
                f"-u\n{ENTRY_SYMBOL}\n-e\n{ENTRY_SYMBOL}\n",
                encoding="utf-8",
                newline="\n",
            )
            object_one.touch()
            object_two.touch()
            executable.touch()

            matrix_paths = (
                source_one,
                source_two,
                object_one,
                object_two,
                response,
                executable,
            )
            self.assertTrue(
                all(any(ord(char) > 127 for char in str(path))
                    for path in matrix_paths)
            )
            self.assertTrue(all(" " in str(path) for path in matrix_paths))
            self.assertGreaterEqual(
                min(len(str(path)) for path in matrix_paths),
                220,
            )

            self.run_checked(
                [
                    str(self.gcc),
                    "-c",
                    _windows_short_path(source_one),
                    "-o",
                    _windows_short_path(object_one),
                ],
                cwd=work,
            )
            self.run_checked(
                [
                    str(self.gcc),
                    "-c",
                    _windows_short_path(source_two),
                    "-o",
                    _windows_short_path(object_two),
                ],
                cwd=work,
            )
            self.assertGreater(object_one.stat().st_size, 0)
            self.assertGreater(object_two.stat().st_size, 0)

            self.run_checked(
                [
                    str(self.gcc),
                    "-nostartfiles",
                    f"-Wl,@{_windows_short_path(response)}",
                    _windows_short_path(object_one),
                    _windows_short_path(object_two),
                    "-lkernel32",
                    "-o",
                    _windows_short_path(executable),
                ],
                cwd=work,
            )
            self.assertGreater(executable.stat().st_size, 0)

            run = subprocess.run(
                [str(executable)],
                cwd=str(work),
                capture_output=True,
                timeout=20,
            )
            self.assertEqual(run.returncode, 0, run.stderr)

    def test_parallel_baa_builds_use_isolated_direct_artifacts(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_parallel_toolchain_") as temp:
            work = Path(temp) / "مشروع عربي متواز"
            work.mkdir()
            source = work / "مدخل مشترك.baa"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            outputs = [
                work / f"برنامج متواز {index}.exe"
                for index in range(6)
            ]
            manifests = [
                work / f"بيان متواز {index}.json"
                for index in range(6)
            ]
            env = os.environ.copy()
            env["PATH"] = str(self.gcc.parent) + os.pathsep + env.get("PATH", "")
            processes = [
                subprocess.Popen(
                    [
                        str(self.baa),
                        "--emit-build-manifest",
                        manifest.name,
                        source.name,
                        "-o",
                        output.name,
                    ],
                    cwd=str(work),
                    env=env,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                for output, manifest in zip(outputs, manifests)
            ]
            results = [process.communicate(timeout=60) for process in processes]

            for index, (process, (stdout, stderr)) in enumerate(
                zip(processes, results)
            ):
                self.assertEqual(
                    process.returncode,
                    0,
                    f"parallel build {index} failed\nstdout:\n{stdout}\nstderr:\n{stderr}",
                )

            for output, manifest in zip(outputs, manifests):
                self.assertGreater(output.stat().st_size, 0)
                run = subprocess.run(
                    [str(output)],
                    cwd=str(work),
                    capture_output=True,
                    timeout=20,
                )
                self.assertEqual(run.returncode, 0, run.stderr)
                raw_manifest = manifest.read_bytes()
                try:
                    manifest_text = raw_manifest.decode("utf-8")
                except UnicodeDecodeError as error:
                    start = max(0, error.start - 24)
                    end = min(len(raw_manifest), error.end + 24)
                    self.fail(
                        "manifest is not UTF-8 near byte "
                        f"{error.start}: {raw_manifest[start:end].hex()}"
                    )
                data = json.loads(manifest_text)
                self.assertTrue(data["units"])
                for unit in data["units"]:
                    self.assertEqual(Path(unit["output"]).name, f"{source.stem}.o")
                    self.assertNotIn(".baa_", unit["output"])

            self.assertEqual(list(work.glob("*.baa_*")), [])

    def test_phase_timings_cover_direct_assemble_and_link(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_unicode_timing_") as temp:
            work = _make_long_unicode_directory(Path(temp))
            source = work / "مدخل قياس مباشر مع مسافات.baa"
            executable = work / "برنامج قياس مباشر مع مسافات.exe"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            expected = {
                "read",
                "parse",
                "analyze",
                "lower",
                "isel",
                "regalloc",
                "emit",
                "assemble",
                "link",
                "total",
            }
            for assembler, selector in (
                ("nazm", []),
                ("gas", ["--assembler=gas"]),
            ):
                with self.subTest(assembler=assembler):
                    selected_output = executable.with_stem(
                        f"{executable.stem}-{assembler}"
                    )
                    proc = self.run_checked(
                        [
                            str(self.baa),
                            "--time-phases",
                            *selector,
                            source.name,
                            "-o",
                            selected_output.name,
                        ],
                        cwd=work,
                        timeout=60,
                    )
                    timing_line = next(
                        (
                            line
                            for line in proc.stderr.splitlines()
                            if line.startswith("[TIME] ")
                        ),
                        "",
                    )
                    self.assertTrue(timing_line, proc.stderr)
                    values = {
                        name: float(value)
                        for name, value in re.findall(
                            r"([a-z_]+)=([0-9]+\.[0-9]+)",
                            timing_line,
                        )
                    }
                    self.assertTrue(expected.issubset(values), timing_line)
                    self.assertTrue(
                        all(values[name] >= 0.0 for name in expected)
                    )
                    self.assertGreater(values["assemble"], 0.0)
                    self.assertGreater(values["link"], 0.0)
                    self.assertGreaterEqual(
                        values["total"] + 0.001,
                        values["assemble"] + values["link"],
                    )
                    self.assertGreater(selected_output.stat().st_size, 0)
                    run = subprocess.run(
                        [str(selected_output)],
                        cwd=str(work),
                        capture_output=True,
                        timeout=20,
                    )
                    self.assertEqual(run.returncode, 0, run.stderr)
            self.assertFalse((work / "baa_stage").exists())
            self.assertEqual(list(work.glob("*.baa_*")), [])


if __name__ == "__main__":
    unittest.main()
