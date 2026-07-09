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
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build" / "baa.exe",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class FastCheckTests(unittest.TestCase):
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

    def test_valid_source_checks_without_toolchain_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_fast_check_ok_") as temp:
            work = Path(temp)
            (work / "main.baa").write_text(
                "صحيح الرئيسية() {\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )
            custom_output = work / "should_not_exist.exe"
            cross_target = "x86_64-linux" if os.name == "nt" else "x86_64-windows"

            proc = self.run_baa(
                work,
                "--check",
                f"--target={cross_target}",
                "main.baa",
                "-o",
                str(custom_output),
            )
            generated = (
                list(work.glob("out*"))
                + list(work.glob("*.o"))
                + list(work.glob("*.s"))
            )
            if custom_output.exists():
                generated.append(custom_output)

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        self.assertEqual(generated, [])

    def test_semantic_error_fails_without_toolchain_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_fast_check_bad_") as temp:
            work = Path(temp)
            (work / "bad.baa").write_text(
                "صحيح الرئيسية() {\n"
                "    س = ١.\n"
                "    إرجع ٠.\n"
                "}\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "--check", "bad.baa")
            generated = list(work.glob("out*")) + list(work.glob("*.o")) + list(work.glob("*.s"))

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertNotEqual(proc.returncode, 0, combined)
        self.assertIn("[B1000]", combined)
        self.assertIn("[semantic]", combined)
        self.assertEqual(generated, [])

    def test_manifest_records_check_mode_and_dependencies(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_fast_check_manifest_") as temp:
            work = Path(temp)
            (work / "api.baahd").write_text(
                "خارجي صحيح اجمع(صحيح أ، صحيح ب).\n",
                encoding="utf-8",
            )
            (work / "main.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح الرئيسية() {\n"
                "    إرجع اجمع(١، ٢).\n"
                "}\n",
                encoding="utf-8",
            )
            manifest = work / "manifest.json"

            proc = self.run_baa(
                work,
                "--check",
                "--emit-build-manifest",
                str(manifest),
                "main.baa",
            )
            data = json.loads(manifest.read_text(encoding="utf-8")) if manifest.exists() else {}

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)
        self.assertEqual(data.get("mode"), "check")
        units = data.get("units", [])
        self.assertEqual(len(units), 1)
        self.assertEqual(units[0].get("output"), "")
        deps = units[0].get("dependencies", [])
        self.assertTrue(
            any(str(dep.get("path", "")).replace("\\", "/").endswith("/api.baahd") for dep in deps),
            data,
        )


if __name__ == "__main__":
    unittest.main()
