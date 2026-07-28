#!/usr/bin/env python3
"""Contract tests for Baa-owned tokens-json-v1 source tokenization."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _compiler() -> Path:
    configured = os.environ.get("BAA")
    if configured:
        return Path(configured).resolve()
    suffix = ".exe" if os.name == "nt" else ""
    for candidate in (
        ROOT / "build" / f"baa{suffix}",
        ROOT / "build-linux" / f"baa{suffix}",
    ):
        if candidate.is_file():
            return candidate.resolve()
    raise unittest.SkipTest("Set BAA to a built Baa compiler")


def _byte_position(source: bytes, offset: int) -> tuple[int, int]:
    prefix = source[:offset]
    line = prefix.count(b"\n") + 1
    last_newline = prefix.rfind(b"\n")
    column = offset + 1 if last_newline < 0 else offset - last_newline
    return line, column


class TokensJsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _compiler()

    def run_tokens(
        self, source: bytes, logical_path: Path | str = "رئيسي.baa"
    ) -> subprocess.CompletedProcess[bytes]:
        return subprocess.run(
            [
                str(self.baa),
                "--dump-tokens=json",
                f"--source-stdin={logical_path}",
            ],
            cwd=ROOT,
            input=source,
            capture_output=True,
            timeout=30,
        )

    def test_emits_raw_arabic_tokens_without_preprocessing(self) -> None:
        source = b"\xef\xbb\xbf" + (
            '#تضمين "غير موجود.baahd"\r\n'
            "ثابت صحيح قيمة = ١٢.\n"
            'نص وصف = "سطر أول\nسطر ثان". // تعليق عربي\n'
            "إذا (خطأ) { قيمة = قيمة + ١. }\n"
            "/* تعليق\n   ممتد */#إذا\n"
        ).encode("utf-8")
        logical = Path("مسار عربي") / "غير محفوظ.baa"
        proc = self.run_tokens(source, logical)

        self.assertEqual(
            proc.returncode,
            0,
            proc.stderr.decode("utf-8", errors="replace"),
        )
        data = json.loads(proc.stdout.decode("utf-8"))
        self.assertEqual(data["schema_version"], "tokens-json-v1")
        self.assertEqual(data["language"], "baa")
        self.assertEqual(Path(data["file"]), logical)
        self.assertEqual(data["position_encoding"], "utf-8-bytes")
        self.assertEqual(data["source_bytes"], len(source))

        kinds = [token["kind"] for token in data["tokens"]]
        self.assertIn("directive", kinds)
        self.assertIn("modifier", kinds)
        self.assertIn("type", kinds)
        self.assertIn("identifier", kinds)
        self.assertIn("number", kinds)
        self.assertIn("string", kinds)
        self.assertIn("comment", kinds)
        self.assertIn("keyword", kinds)
        self.assertIn("operator", kinds)

        directive = data["tokens"][0]
        start = directive["span"]["start"]["byte"]
        end = directive["span"]["end"]["byte"]
        self.assertEqual(start, 3)
        self.assertEqual(directive["span"]["start"]["column"], 4)
        self.assertEqual(
            source[start:end].decode("utf-8"),
            '#تضمين "غير موجود.baahd"',
        )

        multiline_string = next(
            token
            for token in data["tokens"]
            if token["kind"] == "string"
            and token["span"]["start"]["line"] == 3
        )
        self.assertEqual(multiline_string["span"]["end"]["line"], 4)
        block_comment = next(
            token
            for token in data["tokens"]
            if token["kind"] == "comment"
            and token["span"]["start"]["line"] == 6
        )
        self.assertEqual(block_comment["span"]["end"]["line"], 7)
        following_directive = next(
            token
            for token in data["tokens"]
            if token["kind"] == "directive"
            and token["span"]["start"]["line"] == 7
        )
        self.assertEqual(
            source[
                following_directive["span"]["start"]["byte"]:
                following_directive["span"]["end"]["byte"]
            ].decode("utf-8"),
            "#إذا",
        )

        for token in data["tokens"]:
            span = token["span"]
            start = span["start"]["byte"]
            end = span["end"]["byte"]
            self.assertLess(start, end)
            self.assertLessEqual(end, len(source))
            self.assertEqual(
                (span["start"]["line"], span["start"]["column"]),
                _byte_position(source, start),
            )
            self.assertEqual(
                (span["end"]["line"], span["end"]["column"]),
                _byte_position(source, end),
            )

    def test_rejects_invalid_utf8_with_source_exit_code(self) -> None:
        proc = self.run_tokens(b"\xff")
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(proc.stdout, b"")

    def test_reads_saved_source_and_reports_missing_source(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_tokens_saved_") as temp:
            source = Path(temp) / "مصدر عربي.baa"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            source_bytes = source.read_bytes()
            saved = subprocess.run(
                [str(self.baa), "--dump-tokens=json", str(source)],
                cwd=ROOT,
                capture_output=True,
                timeout=30,
            )
            missing = subprocess.run(
                [
                    str(self.baa),
                    "--dump-tokens=json",
                    str(Path(temp) / "غير موجود.baa"),
                ],
                cwd=ROOT,
                capture_output=True,
                timeout=30,
            )
        self.assertEqual(saved.returncode, 0)
        data = json.loads(saved.stdout.decode("utf-8"))
        self.assertEqual(data["source_bytes"], len(source_bytes))
        self.assertEqual(data["tokens"][0]["kind"], "type")
        self.assertEqual(missing.returncode, 1)

    def test_rejects_conflicting_and_unknown_token_modes(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_tokens_cli_") as temp:
            source = Path(temp) / "رئيسي.baa"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            conflict = subprocess.run(
                [
                    str(self.baa),
                    "--dump-tokens=json",
                    "--dump-symbols=json",
                    str(source),
                ],
                cwd=ROOT,
                capture_output=True,
                timeout=30,
            )
            unknown = subprocess.run(
                [str(self.baa), "--dump-tokens=xml", str(source)],
                cwd=ROOT,
                capture_output=True,
                timeout=30,
            )
        self.assertEqual(conflict.returncode, 2)
        self.assertEqual(unknown.returncode, 2)


if __name__ == "__main__":
    unittest.main()
