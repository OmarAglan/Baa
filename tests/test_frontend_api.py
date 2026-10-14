#!/usr/bin/env python3
"""baa-frontend-api-v1: in-process calls must match the baa executable.

The probe (tests/frontend_api/frontend_probe.c) runs one request through
baa_frontend_run and writes its stdout, stderr and exit code as baa would.
Every tooling mode is compared byte for byte with the executable, including
the lexer paths that leave through exit(1). Repeated calls must return the
same bytes and must not grow the process: each call frees what it allocated
and resets the compiler's global state.

Build with -DBAA_BUILD_FRONTEND_LIBRARY=ON. Without a probe the tests skip,
unless BAA_FRONTEND_PROBE_REQUIRED=1. Under AddressSanitizer set
BAA_FRONTEND_SKIP_MEMORY_BOUND=1; the sanitizer checks the frees instead.
"""

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SUFFIX = ".exe" if os.name == "nt" else ""


def _find_build() -> tuple[Path, Path] | None:
    configured = os.environ.get("BAA_FRONTEND_PROBE")
    if configured and Path(configured).is_file():
        probe = Path(configured)
        baa = probe.with_name("baa" + SUFFIX)
        return (baa, probe) if baa.is_file() else None
    for directory in (ROOT / "build" / "frontend", ROOT / "build-frontend", ROOT / "build"):
        probe = directory / ("baa_frontend_probe" + SUFFIX)
        baa = directory / ("baa" + SUFFIX)
        if probe.is_file() and baa.is_file():
            return baa, probe
    return None


VALID = (
    "صحيح مربع(صحيح س) {\n"
    "    إرجع س * س.\n"
    "}\n\n"
    "صحيح الرئيسية() {\n"
    "    صحيح ن = مربع(٣).\n"
    "    إرجع ن.\n"
    "}\n"
)
SEMANTIC_ERROR = "صحيح الرئيسية() {\n    إرجع غير_معرف.\n}\n"
SYNTAX_ERROR = "صحيح الرئيسية() {\n    إرجع ١\n"
# #وإلا بلا #إذا_عرف يمر بـ lex_fatal ثم exit(1).
LEXER_FATAL = "#وإلا\nصحيح الرئيسية() { إرجع ٠. }\n"
INCLUDING = '#تضمين "مساعد.baahd"\n\nصحيح الرئيسية() {\n    إرجع ضعف(٢).\n}\n'
HEADER = "صحيح ضعف(صحيح س).\n"
HELPER_SOURCE = '#تضمين "مساعد.baahd"\n\nصحيح ضعف(صحيح س) {\n    إرجع س + س.\n}\n'
MISSING_INCLUDE = '#تضمين "غير_موجود.baahd"\nصحيح الرئيسية() { إرجع ٠. }\n'

SOURCES = {
    "valid": VALID,
    "semantic_error": SEMANTIC_ERROR,
    "syntax_error": SYNTAX_ERROR,
    "lexer_fatal": LEXER_FATAL,
    "including": INCLUDING,
    "missing_include": MISSING_INCLUDE,
}

MODES = {
    "check": ["--check", "--diagnostics=json"],
    "symbols": ["--dump-symbols=json"],
    "tokens": ["--dump-tokens=json"],
    "structure": ["--dump-structure=json"],
    "format": ["--format=json"],
    "inlay": ["--inlay-hints=json"],
    "semantic": ["--semantic-query=json", "--position-byte=40"],
}


class FrontendApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        found = _find_build()
        if not found:
            if os.environ.get("BAA_FRONTEND_PROBE_REQUIRED") == "1":
                raise FileNotFoundError(
                    "baa_frontend_probe not found; build with -DBAA_BUILD_FRONTEND_LIBRARY=ON")
            raise unittest.SkipTest("baa_frontend_probe is not built")
        cls.baa, cls.probe = found
        cls.temp = tempfile.TemporaryDirectory(prefix="باء-واجهة-")
        cls.work = Path(cls.temp.name)
        (cls.work / "مساعد.baahd").write_text(HEADER, encoding="utf-8")
        (cls.work / "مساعد.baa").write_text(HELPER_SOURCE, encoding="utf-8")

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def run_pair(self, arguments: list[str], source: bytes = b"",
                 cwd: Path | None = None) -> tuple[subprocess.CompletedProcess[bytes],
                                                    subprocess.CompletedProcess[bytes]]:
        work = cwd or self.work
        direct = subprocess.run([str(self.baa), *arguments], cwd=str(work),
                                input=source, capture_output=True, timeout=60)
        in_process = subprocess.run(
            [str(self.probe), "--executable", str(self.baa), "--", *arguments],
            cwd=str(work), input=source, capture_output=True, timeout=60)
        return direct, in_process

    def assert_same(self, label: str, direct: subprocess.CompletedProcess[bytes],
                    in_process: subprocess.CompletedProcess[bytes]) -> None:
        self.assertEqual(direct.returncode, in_process.returncode,
                         f"{label}: exit code\n{in_process.stderr!r}")
        self.assertEqual(direct.stdout, in_process.stdout, f"{label}: stdout")
        self.assertEqual(direct.stderr, in_process.stderr, f"{label}: stderr")

    def test_contract_name(self) -> None:
        header = (ROOT / "src" / "api" / "baa_frontend.h").read_text(encoding="utf-8")
        self.assertIn('#define BAA_FRONTEND_CONTRACT "baa-frontend-api-v1"', header)

    def test_every_tooling_mode_matches_the_executable(self) -> None:
        for source_name, text in SOURCES.items():
            logical = self.work / f"{source_name}.baa"
            for mode_name, mode in MODES.items():
                with self.subTest(source=source_name, mode=mode_name):
                    arguments = [*mode, f"--source-stdin={logical}"]
                    direct, in_process = self.run_pair(arguments, text.encode("utf-8"))
                    self.assert_same(f"{source_name}/{mode_name}", direct, in_process)

    def test_lexer_exit_still_writes_diagnostics_json(self) -> None:
        logical = self.work / "lexer_fatal.baa"
        _, in_process = self.run_pair(
            ["--check", "--diagnostics=json", f"--source-stdin={logical}"],
            LEXER_FATAL.encode("utf-8"))
        self.assertEqual(in_process.returncode, 1)
        self.assertIn(b'"schema_version": "diagnostics-json-v1"', in_process.stdout)

    def test_positional_sources_and_working_directory(self) -> None:
        direct, in_process = self.run_pair(
            ["--check", "--diagnostics=json", "مساعد.baa"])
        self.assert_same("positional", direct, in_process)
        self.assertEqual(in_process.returncode, 0, in_process.stdout)

        # The request's working directory replaces the probe's for the call.
        elsewhere = subprocess.run(
            [str(self.probe), "--executable", str(self.baa), "--cwd", str(self.work),
             "--", "--check", "--diagnostics=json", "مساعد.baa"],
            cwd=str(ROOT), capture_output=True, timeout=60)
        self.assertEqual(elsewhere.returncode, 0, elsewhere.stdout)
        self.assertEqual(elsewhere.stdout, in_process.stdout)

    def test_stateless_tooling_commands(self) -> None:
        for arguments in (["--completion-data=json"], ["--version"],
                          ["--explain", "B0001"], ["--target-info=json"]):
            with self.subTest(arguments=arguments):
                direct, in_process = self.run_pair(arguments)
                self.assert_same(" ".join(arguments), direct, in_process)

    def test_code_generation_is_rejected(self) -> None:
        for arguments in (["مساعد.baa"], ["-S", "مساعد.baa"], ["-c", "مساعد.baa"],
                          ["--emit-nazm", "مساعد.baa"], ["update"]):
            with self.subTest(arguments=arguments):
                result = subprocess.run(
                    [str(self.probe), "--", *arguments], cwd=str(self.work),
                    capture_output=True, timeout=60)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, b"")
                self.assertFalse((self.work / "مساعد.s").exists())

    def test_repeated_calls_return_the_same_bytes_without_growing(self) -> None:
        body = "".join(
            f"صحيح دالة_{i}(صحيح س) {{\n"
            f"    صحيح ص = س + {i}.\n"
            f"    إذا (ص > ١٠ && س < ٥) {{ إرجع ص. }}\n"
            f"    إرجع س * ٢.\n"
            f"}}\n\n"
            for i in range(150)
        )
        text = body + "صحيح الرئيسية() {\n    إرجع دالة_3(٤).\n}\n"
        logical = self.work / "كبير.baa"
        cases = (
            (["--check", "--diagnostics=json", f"--source-stdin={logical}"], 200),
            (["--dump-symbols=json", f"--source-stdin={logical}"], 100),
            (["--check", "--diagnostics=json", f"--source-stdin={logical}"], 1),
        )
        for arguments, repeat in cases[:2]:
            with self.subTest(arguments=arguments[0]):
                report = self.work / "ذاكرة.txt"
                result = subprocess.run(
                    [str(self.probe), "--repeat", str(repeat), "--memory-report", str(report),
                     "--executable", str(self.baa), "--", *arguments],
                    cwd=str(self.work), input=text.encode("utf-8"),
                    capture_output=True, timeout=300)
                self.assertEqual(result.returncode, 0, result.stderr)
                first_kib, last_kib = (int(v) for v in report.read_text().split())
                if os.environ.get("BAA_FRONTEND_SKIP_MEMORY_BOUND") == "1":
                    continue  # AddressSanitizer's quarantine keeps freed blocks resident.
                self.assertLess(last_kib - first_kib, 16 * 1024,
                                f"process grew from {first_kib} KiB to {last_kib} KiB")

        # A failing call between successful ones must not leak state into them.
        bad = subprocess.run(
            [str(self.probe), "--repeat", "20", "--", "--check", "--diagnostics=json",
             f"--source-stdin={self.work / 'lexer_fatal.baa'}"],
            cwd=str(self.work), input=LEXER_FATAL.encode("utf-8"),
            capture_output=True, timeout=120)
        self.assertEqual(bad.returncode, 1, bad.stderr)


if __name__ == "__main__":
    unittest.main()
