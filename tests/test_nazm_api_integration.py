#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FINGERPRINT = re.compile(
    r"^nazm-api-v1;version=[^;]+;capabilities="
    r"nazm-capabilities-v1:[0-9a-f]{64}$"
)
EMBEDDED_VERSION = re.compile(
    r"^Embedded Nazm (\d+\.\d+\.\d+\S*) \(([A-Za-z0-9._-]+)\), (opt-in|default)$",
    re.MULTILINE,
)
NAZM_NAMES = ("نظم", "نظم.exe")
SKIPPED_CORPUS_FLAGS = ("-S", "-c", "--target=", "--startup=")
MINIMUM_CORPUS_OBJECTS = 80
MINIMAL_PROGRAM = "صحيح الرئيسية() {\n    إرجع ٠.\n}\n"


def _path_without_nazm() -> str:
    """PATH بلا أي مجلد يحمل ملف نظم التنفيذي."""
    kept = []
    for entry in os.environ.get("PATH", "").split(os.pathsep):
        if not entry:
            continue
        if any((Path(entry) / name).is_file() for name in NAZM_NAMES):
            continue
        kept.append(entry)
    return os.pathsep.join(kept)


def _corpus_flags(source: Path) -> list[str]:
    """أعلام `// FLAGS:` التي لا تغير نوع المخرج ولا الهدف."""
    flags: list[str] = []
    for line in source.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if stripped.startswith("// FLAGS:"):
            flags.extend(shlex.split(stripped[len("// FLAGS:"):]))
    return [flag for flag in flags if not flag.startswith(SKIPPED_CORPUS_FLAGS)]


class NazmApiIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if os.environ.get("BAA_EMBEDDED_NAZM_TEST") != "1":
            raise unittest.SkipTest("opt-in embedded Nazm build is not selected")
        cls.baa = Path(os.environ["BAA"])
        cls.nazm = Path(os.environ["NAZM"])
        if not cls.baa.is_file() or not cls.nazm.is_file():
            raise FileNotFoundError("BAA and NAZM must name built executables")
        version = subprocess.run(
            [str(cls.baa), "--version"],
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )
        match = EMBEDDED_VERSION.search(version.stdout)
        if version.returncode != 0 or not match:
            raise AssertionError(
                "baa --version does not identify its embedded Nazm:\n"
                + version.stdout
                + version.stderr
            )
        cls.embedded_version, cls.embedded_revision, cls.mode = match.groups()

    def run_baa(
        self,
        work: Path,
        *arguments: str,
        nazm_environment: str | None = None,
        hide_nazm: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment["BAA_STDLIB"] = str(ROOT / "stdlib")
        environment.pop("BAA_NAZM", None)
        if nazm_environment is not None:
            environment["BAA_NAZM"] = nazm_environment
        if hide_nazm:
            environment["PATH"] = _path_without_nazm()
        return subprocess.run(
            [str(self.baa), *arguments],
            cwd=work,
            env=environment,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )

    def test_embedded_objects_match_cli_and_are_fingerprint_cacheable(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_api_") as temporary:
            work = Path(temporary)
            source = work / "برنامج.باء"
            source.write_text(
                "صحيح الرئيسية() {\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )
            suffix = ".obj" if os.name == "nt" else ".o"
            embedded_object = work / f"مدمج{suffix}"
            cached_object = work / f"مخزن{suffix}"
            external_object = work / f"عملية{suffix}"
            manifest = work / "بيان.json"
            cache = work / "كاش"

            first = self.run_baa(
                work,
                "-c",
                "--نظم-داخل-العملية",
                "--incremental",
                "--cache-dir",
                str(cache),
                "--emit-build-manifest",
                str(manifest),
                str(source),
                "-o",
                str(embedded_object),
            )
            self.assertEqual(first.returncode, 0, first.stderr)
            first_manifest = json.loads(manifest.read_text(encoding="utf-8"))
            fingerprint = first_manifest["assembler_fingerprint"]
            self.assertRegex(fingerprint, FINGERPRINT)
            self.assertTrue(first_manifest["units"][0]["cache"]["enabled"])
            self.assertFalse(first_manifest["units"][0]["cache"]["hit"])

            second = self.run_baa(
                work,
                "-c",
                "--نظم-داخل-العملية",
                "--incremental",
                "--cache-dir",
                str(cache),
                "--emit-build-manifest",
                str(manifest),
                str(source),
                "-o",
                str(cached_object),
            )
            self.assertEqual(second.returncode, 0, second.stderr)
            second_manifest = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(second_manifest["assembler_fingerprint"], fingerprint)
            self.assertTrue(second_manifest["units"][0]["cache"]["hit"])
            self.assertEqual(embedded_object.read_bytes(), cached_object.read_bytes())

            external = self.run_baa(
                work,
                "-c",
                f"--nazm-path={self.nazm}",
                str(source),
                "-o",
                str(external_object),
            )
            self.assertEqual(external.returncode, 0, external.stderr)
            self.assertEqual(embedded_object.read_bytes(), external_object.read_bytes())

    def test_embedded_source_failures_preserve_contract_codes(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_api_error_") as temporary:
            work = Path(temporary)
            source = work / "خاطئ.نظم"
            source.write_text(
                ".نص\n"
                ".عام خاطئ\n"
                "خاطئ:\n"
                "    تعليمة_غير_موجودة\n",
                encoding="utf-8",
            )
            output = work / ("خاطئ.obj" if os.name == "nt" else "خاطئ.o")
            result = self.run_baa(
                work,
                "-c",
                "--نظم-داخل-العملية",
                str(source),
                "-o",
                str(output),
            )
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn("خطأ في", result.stderr)
            self.assertFalse(output.exists())

            conflict = self.run_baa(
                work,
                "-c",
                "--assembler=gas",
                "--نظم-داخل-العملية",
                str(source),
                "-o",
                str(output),
            )
            self.assertEqual(conflict.returncode, 2, conflict.stderr)

    def test_version_identifies_the_embedded_nazm(self) -> None:
        expected_mode = os.environ.get("BAA_EMBEDDED_NAZM_EXPECT_MODE")
        if expected_mode:
            self.assertEqual(self.mode, expected_mode)
        expected_revision = os.environ.get("BAA_EMBEDDED_NAZM_EXPECT_REVISION")
        if expected_revision:
            self.assertEqual(self.embedded_revision, expected_revision)
        with tempfile.TemporaryDirectory(prefix="baa_nazm_api_id_") as temporary:
            work = Path(temporary)
            source = work / "برنامج.باء"
            source.write_text(MINIMAL_PROGRAM, encoding="utf-8")
            manifest = work / "بيان.json"
            result = self.run_baa(
                work,
                "-c",
                "--نظم-داخل-العملية",
                "--emit-build-manifest",
                str(manifest),
                str(source),
                "-o",
                str(work / "كائن.o"),
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            fingerprint = json.loads(manifest.read_text(encoding="utf-8"))[
                "assembler_fingerprint"
            ]
            self.assertIn(f";version={self.embedded_version};", fingerprint)

    def test_selection_without_an_installed_nazm(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_nazm_api_select_") as temporary:
            work = Path(temporary)
            source = work / "برنامج.باء"
            source.write_text(MINIMAL_PROGRAM, encoding="utf-8")
            missing = work / "نظم-غير-موجود"

            def build(name: str, *flags: str, nazm_environment: str | None = None):
                output = work / f"{name}.o"
                result = self.run_baa(
                    work,
                    "-c",
                    *flags,
                    str(source),
                    "-o",
                    str(output),
                    nazm_environment=nazm_environment,
                    hide_nazm=True,
                )
                return result, output

            forced, forced_object = build("مفروض", "--نظم-داخل-العملية")
            self.assertEqual(forced.returncode, 0, forced.stderr)

            plain, plain_object = build("عادي")
            if self.mode == "default":
                self.assertEqual(plain.returncode, 0, plain.stderr)
                self.assertEqual(plain_object.read_bytes(), forced_object.read_bytes())
            else:
                self.assertEqual(plain.returncode, 4, plain.stderr)
                self.assertFalse(plain_object.exists())

            # الاختيار الصريح يتقدم على المضمن في الوضعين: ملف مفقود يفشل ولا يُتجاوز.
            by_flag, by_flag_object = build("مسار", f"--nazm-path={missing}")
            self.assertEqual(by_flag.returncode, 4, by_flag.stderr)
            self.assertFalse(by_flag_object.exists())
            by_environment, by_environment_object = build(
                "بيئة", nazm_environment=str(missing)
            )
            self.assertEqual(by_environment.returncode, 4, by_environment.stderr)
            self.assertFalse(by_environment_object.exists())

            external, external_object = build("خارجي", nazm_environment=str(self.nazm))
            self.assertEqual(external.returncode, 0, external.stderr)
            self.assertEqual(external_object.read_bytes(), forced_object.read_bytes())

    def test_integration_corpus_objects_match_the_subprocess(self) -> None:
        sources = sorted((ROOT / "tests" / "integration").rglob("*.baa"))
        self.assertTrue(sources, "integration corpus is empty")
        compared = 0
        with tempfile.TemporaryDirectory(prefix="baa_nazm_api_corpus_") as temporary:
            work = Path(temporary)
            for index, source in enumerate(sources):
                flags = _corpus_flags(source)
                for level in ("-O0", "-O2"):
                    embedded_object = work / f"{index}{level}-مضمن.o"
                    external_object = work / f"{index}{level}-عملية.o"
                    embedded = self.run_baa(
                        ROOT,
                        "-c",
                        level,
                        *flags,
                        "--نظم-داخل-العملية",
                        str(source),
                        "-o",
                        str(embedded_object),
                    )
                    external = self.run_baa(
                        ROOT,
                        "-c",
                        level,
                        *flags,
                        f"--nazm-path={self.nazm}",
                        str(source),
                        "-o",
                        str(external_object),
                    )
                    label = f"{source.relative_to(ROOT).as_posix()} {level}"
                    self.assertEqual(
                        embedded.returncode,
                        external.returncode,
                        f"{label}\n{embedded.stderr}\n{external.stderr}",
                    )
                    if embedded.returncode != 0:
                        continue
                    self.assertEqual(
                        embedded_object.read_bytes(),
                        external_object.read_bytes(),
                        label,
                    )
                    compared += 1
                    embedded_object.unlink()
                    external_object.unlink()
        self.assertGreaterEqual(compared, MINIMUM_CORPUS_OBJECTS)


if __name__ == "__main__":
    unittest.main()
