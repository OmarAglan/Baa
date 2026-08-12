#!/usr/bin/env python3
"""Contract tests for compiler-owned inlay-hints-json-v1 output."""

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
        ROOT / "build-linux" / "baa",
    ):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


def _byte_offset(source: str, needle: str, occurrence: int = 0) -> int:
    encoded = source.encode("utf-8")
    target = needle.encode("utf-8")
    start = 0
    for _ in range(occurrence + 1):
        found = encoded.find(target, start)
        if found < 0:
            raise AssertionError(f"Could not find occurrence {occurrence} of {needle!r}")
        start = found + len(target)
    return found


class InlayHintTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def hints(self, work: Path, logical: Path, source: str) -> dict:
        process = subprocess.run(
            [
                str(self.baa),
                "--inlay-hints=json",
                f"--source-stdin={logical}",
            ],
            cwd=str(work),
            input=source.encode("utf-8"),
            capture_output=True,
            timeout=30,
        )
        stdout = process.stdout.decode("utf-8", errors="replace")
        stderr = process.stderr.decode("utf-8", errors="replace")
        self.assertEqual(process.returncode, 0, f"{stdout}\n{stderr}")
        result = json.loads(stdout)
        self.assertEqual(result["schema_version"], "inlay-hints-json-v1")
        self.assertEqual(result["position_encoding"], "utf-8-bytes")
        self.assertIsInstance(result["complete"], bool)
        self.assertIsInstance(result["hints"], list)
        return result

    def test_emits_arabic_parameter_hints_at_exact_argument_bytes(self) -> None:
        source = (
            "صحيح اجمع(صحيح أول، صحيح ثان) { إرجع أول + ثان. }\n"
            "صحيح هوية(صحيح قيمة) { إرجع قيمة. }\n"
            "صحيح الرئيسية() {\n"
            "    صحيح قيمة = ٣.\n"
            "    إرجع اجمع(هوية(قيمة)، ٢).\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_inlay_مسار عربي_") as temp:
            work = Path(temp)
            logical = work / "مصدر عربي" / "رئيسي.baa"
            logical.parent.mkdir()
            result = self.hints(work, logical, source)

        self.assertTrue(result["complete"])
        self.assertEqual(Path(result["file"]), logical)
        self.assertEqual(
            result["hints"],
            [
                {
                    "position_byte": _byte_offset(source, "هوية", 1),
                    "kind": "parameter",
                    "label": "أول:",
                    "parameter": "أول",
                    "padding_right": True,
                },
                {
                    "position_byte": _byte_offset(source, "٢"),
                    "kind": "parameter",
                    "label": "ثان:",
                    "parameter": "ثان",
                    "padding_right": True,
                },
            ],
        )
        # هوية(قيمة) is self-explanatory because argument and parameter match.
        self.assertNotIn("قيمة:", {hint["label"] for hint in result["hints"]})

    def test_uses_included_prototype_parameter_names(self) -> None:
        source = (
            '#تضمين "واجهة.baahd"\n'
            "صحيح الرئيسية() { إرجع ضاعف(٣). }\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_inlay_include_") as temp:
            work = Path(temp)
            logical = work / "مصدر" / "رئيسي.baa"
            logical.parent.mkdir()
            (logical.parent / "واجهة.baahd").write_text(
                "خارجي صحيح ضاعف(صحيح قيمة).\n", encoding="utf-8"
            )
            result = self.hints(work, logical, source)

        self.assertTrue(result["complete"])
        self.assertEqual(len(result["hints"]), 1)
        self.assertEqual(result["hints"][0]["label"], "قيمة:")
        self.assertEqual(
            result["hints"][0]["position_byte"],
            _byte_offset(source, "٣"),
        )

    def test_marks_recovered_semantic_output_incomplete(self) -> None:
        source = (
            "صحيح اجمع(صحيح أول، صحيح ثان) { إرجع أول + ثان. }\n"
            "صحيح الرئيسية() { إرجع اجمع(مفقود، ٢). }\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_inlay_incomplete_") as temp:
            work = Path(temp)
            logical = work / "غير مكتمل.baa"
            result = self.hints(work, logical, source)

        self.assertFalse(result["complete"])
        self.assertEqual(
            [hint["label"] for hint in result["hints"]],
            ["أول:", "ثان:"],
        )

    def test_rejects_invalid_format_and_mixed_machine_modes(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_inlay_cli_") as temp:
            work = Path(temp)
            source = work / "رئيسي.baa"
            source.write_text("صحيح الرئيسية() { إرجع ٠. }\n", encoding="utf-8")
            invalid = subprocess.run(
                [str(self.baa), "--inlay-hints=text", str(source)],
                cwd=str(work),
                capture_output=True,
                timeout=30,
            )
            mixed = subprocess.run(
                [
                    str(self.baa),
                    "--inlay-hints=json",
                    "--semantic-index=json",
                    str(source),
                ],
                cwd=str(work),
                capture_output=True,
                timeout=30,
            )

        self.assertEqual(invalid.returncode, 2)
        self.assertEqual(mixed.returncode, 2)


if __name__ == "__main__":
    unittest.main()
