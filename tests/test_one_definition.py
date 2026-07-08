#!/usr/bin/env python3

from __future__ import annotations

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


class OneDefinitionDiagnosticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def run_baa(self, work: Path, *inputs: str) -> subprocess.CompletedProcess[str]:
        exe_ext = ".exe" if os.name == "nt" else ""
        out = work / f"app{exe_ext}"
        cmd = [str(self.baa), "-O1", *inputs, "-o", str(out)]
        return subprocess.run(
            cmd,
            cwd=str(work),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
        )

    def test_duplicate_exported_function_fails_before_linker(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_odr_func_") as temp:
            work = Path(temp)
            (work / "api.baahd").write_text("خارجي صحيح قيمة_مشتركة().\n", encoding="utf-8")
            (work / "a.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح قيمة_مشتركة() { إرجع ١. }\n",
                encoding="utf-8",
            )
            (work / "b.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح قيمة_مشتركة() { إرجع ٢. }\n",
                encoding="utf-8",
            )
            (work / "main.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح الرئيسية() { إرجع قيمة_مشتركة() - ١. }\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "main.baa", "a.baa", "b.baa")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertNotEqual(proc.returncode, 0, combined)
        self.assertIn("تعريف متعدد للرمز العام 'قيمة_مشتركة'", combined)
        self.assertIn("التعريف الأول: دالة في a.baa", combined)
        self.assertIn("التعريف الثاني: دالة في b.baa", combined)
        self.assertIn("مساعدة: اجعل أحد التعريفين تصريحاً 'خارجي'", combined)

    def test_duplicate_exported_global_fails_before_linker(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_odr_global_") as temp:
            work = Path(temp)
            (work / "api.baahd").write_text("خارجي صحيح قيمة_عامة.\n", encoding="utf-8")
            (work / "a.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح قيمة_عامة = ١.\n",
                encoding="utf-8",
            )
            (work / "b.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح قيمة_عامة = ٢.\n",
                encoding="utf-8",
            )
            (work / "main.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح الرئيسية() { إرجع قيمة_عامة - ١. }\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "main.baa", "a.baa", "b.baa")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertNotEqual(proc.returncode, 0, combined)
        self.assertIn("تعريف متعدد للرمز العام 'قيمة_عامة'", combined)
        self.assertIn("التعريف الأول: متغير عام في a.baa", combined)
        self.assertIn("التعريف الثاني: متغير عام في b.baa", combined)

    def test_file_local_static_globals_do_not_conflict(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_odr_static_") as temp:
            work = Path(temp)
            (work / "api.baahd").write_text(
                "خارجي صحيح قيمة_أ().\n"
                "خارجي صحيح قيمة_ب().\n",
                encoding="utf-8",
            )
            (work / "a.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "ساكن صحيح قيمة_داخلية = ١.\n"
                "صحيح قيمة_أ() { إرجع قيمة_داخلية. }\n",
                encoding="utf-8",
            )
            (work / "b.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "ساكن صحيح قيمة_داخلية = ٢.\n"
                "صحيح قيمة_ب() { إرجع قيمة_داخلية. }\n",
                encoding="utf-8",
            )
            (work / "main.baa").write_text(
                "#تضمين \"api.baahd\"\n"
                "صحيح الرئيسية() { إرجع قيمة_أ() + قيمة_ب() - ٣. }\n",
                encoding="utf-8",
            )

            proc = self.run_baa(work, "main.baa", "a.baa", "b.baa")

        combined = f"{proc.stdout}\n{proc.stderr}"
        self.assertEqual(proc.returncode, 0, combined)


if __name__ == "__main__":
    unittest.main()
