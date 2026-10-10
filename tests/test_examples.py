#!/usr/bin/env python3

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples"


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


class PublicExamplesCompileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()
        cls.examples = sorted(EXAMPLES.glob("*.باء"))
        if not cls.examples:
            raise AssertionError("No public examples found under examples/*.باء")

    def test_public_examples_compile_with_verify(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_examples_") as temp:
            out_root = Path(temp)
            for example in self.examples:
                ext = ".exe" if os.name == "nt" else ""
                out = out_root / f"{example.stem}{ext}"
                with self.subTest(example=example.name):
                    proc = subprocess.run(
                        [str(self.baa), "-O2", "--verify", str(example), "-o", str(out)],
                        cwd=str(ROOT),
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        capture_output=True,
                        timeout=30,
                    )
                    combined = f"{proc.stdout}\n{proc.stderr}"
                    self.assertEqual(proc.returncode, 0, combined)
                    self.assertTrue(out.exists(), f"missing output for {example.name}")


class InstalledLayoutTests(unittest.TestCase):
    """An installed compiler finds its standard library without BAA_HOME."""

    LAYOUTS = {
        "beside-compiler": (Path("."), Path(".")),
        "prefix-share": (Path("bin"), Path("share") / "baa"),
    }

    def test_installed_compiler_finds_stdlib_without_environment(self) -> None:
        baa = _find_baa()
        env = {k: v for k, v in os.environ.items() if k not in ("BAA_HOME", "BAA_STDLIB")}
        for name, (bin_dir, home) in self.LAYOUTS.items():
            with tempfile.TemporaryDirectory(prefix="baa_layout_") as temp:
                root = Path(temp) / "تثبيت باء"
                (root / bin_dir).mkdir(parents=True, exist_ok=True)
                installed = root / bin_dir / baa.name
                shutil.copy2(baa, installed)
                shutil.copytree(ROOT / "stdlib", root / home / "stdlib")
                project = Path(temp) / "مشروع"
                project.mkdir()
                shutil.copy2(EXAMPLES / "math_and_format.باء", project / "نسبي.باء")
                (project / "مجرد.باء").write_text(
                    '#تضمين "baalib.baahd"\n\nصحيح الرئيسية() {\n    إرجع ٠.\n}\n',
                    encoding="utf-8",
                )
                for source in ("نسبي.باء", "مجرد.باء"):
                    with self.subTest(layout=name, source=source):
                        proc = subprocess.run(
                            [str(installed), "--check", source],
                            cwd=str(project),
                            env=env,
                            text=True,
                            encoding="utf-8",
                            errors="replace",
                            capture_output=True,
                            timeout=30,
                        )
                        self.assertEqual(proc.returncode, 0, f"{proc.stdout}\n{proc.stderr}")


if __name__ == "__main__":
    unittest.main()
