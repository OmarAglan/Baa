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
    configured = os.environ.get("BAA")
    if configured and Path(configured).is_file():
        return Path(configured)
    for candidate in (
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "baa",
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build-linux" / "baa",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
    ):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


class SymbolsJsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def run_baa(
        self, work: Path, *arguments: str, source: str | None = None
    ) -> subprocess.CompletedProcess[str]:
        proc = subprocess.run(
            [str(self.baa), *arguments],
            cwd=str(work),
            input=source.encode("utf-8") if source is not None else None,
            capture_output=True,
            timeout=30,
        )
        return subprocess.CompletedProcess(
            proc.args,
            proc.returncode,
            proc.stdout.decode("utf-8", errors="replace"),
            proc.stderr.decode("utf-8", errors="replace"),
        )

    def test_emits_hierarchical_arabic_document_symbols_and_exact_bytes(self) -> None:
        source = (
            "تعداد اتجاه {\n"
            "    شمال،\n"
            "    جنوب،\n"
            "}\n\n"
            "هيكل نقطة {\n"
            "    صحيح س.\n"
            "    صحيح ص.\n"
            "}\n\n"
            "نوع عدد = صحيح.\n"
            "ثابت عدد الحد = ١٠.\n\n"
            "صحيح اجمع(صحيح أ، صحيح ب) {\n"
            "    إرجع أ + ب.\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_symbols_مسار_") as temp:
            work = Path(temp)
            logical = work / "مصدر عربي" / "رموز.baa"
            logical.parent.mkdir()
            proc = self.run_baa(
                work,
                "--dump-symbols=json",
                f"--source-stdin={logical}",
                source=source,
            )

        self.assertEqual(proc.returncode, 0, f"{proc.stdout}\n{proc.stderr}")
        data = json.loads(proc.stdout)
        self.assertEqual(data["schema_version"], "symbols-json-v1")
        self.assertEqual(data["position_encoding"], "utf-8-bytes")
        self.assertEqual(Path(data["file"]), logical)

        symbols = {symbol["name"]: symbol for symbol in data["symbols"]}
        self.assertEqual(set(symbols), {"اتجاه", "نقطة", "عدد", "الحد", "اجمع"})
        self.assertEqual(symbols["اتجاه"]["kind"], "enum")
        self.assertEqual(
            [child["name"] for child in symbols["اتجاه"]["children"]],
            ["شمال", "جنوب"],
        )
        self.assertEqual(symbols["نقطة"]["kind"], "struct")
        self.assertEqual(
            [child["kind"] for child in symbols["نقطة"]["children"]],
            ["field", "field"],
        )
        self.assertEqual(symbols["الحد"]["type"]["display"], "صحيح")
        self.assertEqual(symbols["عدد"]["target_type"]["display"], "صحيح")
        self.assertTrue(symbols["الحد"]["modifiers"]["const"])
        self.assertEqual(
            [child["kind"] for child in symbols["اجمع"]["children"]],
            ["parameter", "parameter"],
        )

        encoded = source.encode("utf-8")
        for symbol in data["symbols"]:
            start = symbol["span"]["start"]["byte"]
            end = symbol["span"]["end"]["byte"]
            self.assertEqual(encoded[start:end].decode("utf-8"), symbol["name"])
            for child in symbol.get("children", []):
                start = child["span"]["start"]["byte"]
                end = child["span"]["end"]["byte"]
                self.assertEqual(encoded[start:end].decode("utf-8"), child["name"])

    def test_source_relative_include_symbols_do_not_leak_into_document_outline(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_symbols_include_") as temp:
            work = Path(temp)
            source_dir = work / "src"
            source_dir.mkdir()
            logical = source_dir / "main.baa"
            (source_dir / "api.baahd").write_text(
                "خارجي صحيح مساعد(صحيح قيمة).\n", encoding="utf-8"
            )
            source = (
                '#تضمين "api.baahd"\n'
                "صحيح الرئيسية() { إرجع مساعد(١). }\n"
            )
            proc = self.run_baa(
                work,
                "--dump-symbols=json",
                f"--source-stdin={logical}",
                source=source,
            )

        self.assertEqual(proc.returncode, 0, f"{proc.stdout}\n{proc.stderr}")
        data = json.loads(proc.stdout)
        self.assertEqual([item["name"] for item in data["symbols"]], ["الرئيسية"])

    def test_rejects_conflicting_or_unknown_machine_output_modes(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_symbols_cli_") as temp:
            work = Path(temp)
            source = work / "main.baa"
            source.write_text("صحيح الرئيسية() { إرجع ٠. }\n", encoding="utf-8")
            conflict = self.run_baa(
                work,
                "--dump-symbols=json",
                "--diagnostics=json",
                str(source),
            )
            unknown = self.run_baa(work, "--dump-symbols=xml", str(source))

        self.assertEqual(conflict.returncode, 2, conflict.stderr)
        self.assertEqual(unknown.returncode, 2, unknown.stderr)


if __name__ == "__main__":
    unittest.main()
