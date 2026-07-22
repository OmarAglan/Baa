#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FINGERPRINT = re.compile(
    r"^nazm-api-v1;version=[^;]+;capabilities="
    r"nazm-capabilities-v1:[0-9a-f]{64}$"
)


class NazmApiIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if os.environ.get("BAA_EMBEDDED_NAZM_TEST") != "1":
            raise unittest.SkipTest("opt-in embedded Nazm build is not selected")
        cls.baa = Path(os.environ["BAA"])
        cls.nazm = Path(os.environ["NAZM"])
        if not cls.baa.is_file() or not cls.nazm.is_file():
            raise FileNotFoundError("BAA and NAZM must name built executables")

    def run_baa(
        self, work: Path, *arguments: str
    ) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment["BAA_STDLIB"] = str(ROOT / "stdlib")
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


if __name__ == "__main__":
    unittest.main()
