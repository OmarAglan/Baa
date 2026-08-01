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


class SemanticQueryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()

    def query(self, work: Path, logical: Path, source: str, position: int) -> dict:
        proc = subprocess.run(
            [
                str(self.baa),
                "--semantic-query=json",
                f"--position-byte={position}",
                f"--source-stdin={logical}",
            ],
            cwd=str(work),
            input=source.encode("utf-8"),
            capture_output=True,
            timeout=30,
        )
        stdout = proc.stdout.decode("utf-8", errors="replace")
        stderr = proc.stderr.decode("utf-8", errors="replace")
        self.assertEqual(proc.returncode, 0, f"{stdout}\n{stderr}")
        result = json.loads(stdout)
        self.assertEqual(result["schema_version"], "semantic-query-json-v1")
        self.assertEqual(result["position_encoding"], "utf-8-bytes")
        self.assertEqual(result["position_byte"], position)
        return result

    def index(self, work: Path, logical: Path, source: str) -> dict:
        proc = subprocess.run(
            [
                str(self.baa),
                "--semantic-index=json",
                f"--source-stdin={logical}",
            ],
            cwd=str(work),
            input=source.encode("utf-8"),
            capture_output=True,
            timeout=30,
        )
        stdout = proc.stdout.decode("utf-8", errors="replace")
        stderr = proc.stderr.decode("utf-8", errors="replace")
        self.assertEqual(proc.returncode, 0, f"{stdout}\n{stderr}")
        result = json.loads(stdout)
        self.assertEqual(result["schema_version"], "semantic-index-json-v1")
        self.assertEqual(result["position_encoding"], "utf-8-bytes")
        self.assertIsInstance(result["occurrences"], list)
        return result

    def test_semantic_index_classifies_identifier_roles(self) -> None:
        source = (
            "تعداد لون { أحمر، أزرق، }\n"
            "هيكل نقطة {\n"
            "    صحيح س.\n"
            "}\n"
            "صحيح احسب(صحيح معامل) {\n"
            "    هيكل نقطة قيمة.\n"
            "    تعداد لون اختيار = لون:أحمر.\n"
            "    قيمة:س = معامل.\n"
            "    إذا (اختيار == لون:أحمر) { إرجع قيمة:س. }\n"
            "    إرجع قيمة:س.\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_roles_") as temp:
            work = Path(temp)
            logical = work / "أدوار.baa"
            result = self.index(work, logical, source)

        occurrences = result["occurrences"]
        kinds_by_name: dict[str, set[str]] = {}
        for occurrence in occurrences:
            name = occurrence["location"]["name"]
            kinds_by_name.setdefault(name, set()).add(
                occurrence["location"]["kind"]
            )
            self.assertEqual(
                occurrence["location"]["kind"],
                occurrence["symbol"]["kind"],
            )

        self.assertEqual(kinds_by_name["احسب"], {"function"})
        self.assertEqual(kinds_by_name["قيمة"], {"variable"})
        self.assertEqual(kinds_by_name["معامل"], {"parameter"})
        self.assertEqual(kinds_by_name["س"], {"field"})
        self.assertEqual(kinds_by_name["أحمر"], {"enum-member"})
        self.assertEqual(kinds_by_name["نقطة"], {"struct"})

    def test_completion_recovers_visible_scope_while_typing(self) -> None:
        source = (
            '#تضمين "واجهة.baahd"\n'
            "صحيح عام = ١.\n"
            "صحيح الرئيسية(صحيح معامل) {\n"
            "    صحيح قيمة_خارجية = ٢.\n"
            "    إذا (خطأ) {\n"
            "        صحيح سري = ٩.\n"
            "    }\n"
            "    إذا (صواب) {\n"
            "        صحيح معامل = ٣.\n"
            "        لا_تفعل(مع).\n"
            "        صحيح مخفي_بعد = ٤.\n"
            "    }\n"
            "    صحيح لاحق = ٥.\n"
            "    إرجع قيمة_خارجية.\n"
            "}\n"
        )
        prefix = "        لا_تفعل(مع"
        position = (
            source.encode("utf-8").index((prefix + ").\n").encode("utf-8"))
            + len(prefix.encode("utf-8"))
        )
        with tempfile.TemporaryDirectory(
            prefix="baa_semantic_completion_مسار_"
        ) as temp:
            work = Path(temp)
            logical = work / "مصدر عربي" / "إكمال.baa"
            logical.parent.mkdir()
            (logical.parent / "واجهة.baahd").write_text(
                "خارجي صحيح من_واجهة(صحيح قيمة).\n",
                encoding="utf-8",
            )
            result = self.query(work, logical, source, position)

        completion = result["completion"]
        items = {item["label"]: item for item in completion["items"]}
        self.assertIn("معامل", items, list(items))
        self.assertEqual(items["معامل"]["scope"], "local")
        self.assertEqual(items["قيمة_خارجية"]["scope"], "local")
        self.assertEqual(items["عام"]["scope"], "global")
        self.assertEqual(items["من_واجهة"]["scope"], "included")
        self.assertNotIn("سري", items)
        self.assertNotIn("مخفي_بعد", items)
        self.assertNotIn("لاحق", items)
        self.assertEqual(items["معامل"]["insert_text_format"], "plain")
        self.assertIn("متغير", items["معامل"]["documentation"])

    def test_external_identity_is_stable_across_translation_units(self) -> None:
        caller_source = (
            '#تضمين "واجهة.baahd"\n'
            "صحيح الرئيسية() {\n"
            "    إرجع ضاعف(٣).\n"
            "}\n"
        )
        implementation_source = (
            '#تضمين "واجهة.baahd"\n'
            "صحيح ضاعف(صحيح قيمة) {\n"
            "    إرجع قيمة * ٢.\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_project_") as temp:
            work = Path(temp)
            source_dir = work / "مصدر عربي"
            source_dir.mkdir()
            header = source_dir / "واجهة.baahd"
            header.write_text(
                "خارجي صحيح ضاعف(صحيح قيمة).\n",
                encoding="utf-8",
            )
            caller = source_dir / "الرئيسية.baa"
            implementation = source_dir / "الحساب.baa"
            query = self.query(
                work,
                caller,
                caller_source,
                _byte_offset(caller_source, "ضاعف"),
            )
            caller_index = self.index(work, caller, caller_source)
            implementation_index = self.index(
                work, implementation, implementation_source
            )

        identity = query["symbol"]
        self.assertEqual(identity["domain"], "external")
        self.assertEqual(identity["kind"], "function")
        self.assertEqual(identity["name"], "ضاعف")

        caller_occurrences = [
            occurrence
            for occurrence in caller_index["occurrences"]
            if occurrence["symbol"] == identity
        ]
        implementation_occurrences = [
            occurrence
            for occurrence in implementation_index["occurrences"]
            if occurrence["symbol"] == identity
        ]
        self.assertEqual(
            [item["role"] for item in caller_occurrences],
            ["declaration", "reference"],
        )
        self.assertEqual(
            [item["role"] for item in implementation_occurrences],
            ["declaration", "definition"],
        )
        self.assertEqual(
            Path(implementation_occurrences[-1]["location"]["file"]),
            implementation.resolve(),
        )

    def test_resolves_shadowed_arabic_locals_and_function_signature(self) -> None:
        source = (
            "صحيح اجمع(صحيح أول، صحيح ثان) {\n"
            "    إرجع أول + ثان.\n"
            "}\n\n"
            "صحيح الرئيسية() {\n"
            "    صحيح قيمة = ٣.\n"
            "    إذا (قيمة > ٠) {\n"
            "        صحيح قيمة = ٤.\n"
            "        اطبع قيمة.\n"
            "    }\n"
            "    صحيح ناتج = اجمع(قيمة، ٢).\n"
            "    إرجع ناتج.\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_مسار_") as temp:
            work = Path(temp)
            logical = work / "مشروع عربي" / "دلالات.baa"
            logical.parent.mkdir()

            inner = self.query(
                work, logical, source, _byte_offset(source, "قيمة", 3)
            )
            outer = self.query(
                work, logical, source, _byte_offset(source, "قيمة", 4)
            )
            call = self.query(
                work, logical, source, _byte_offset(source, "اجمع", 1)
            )
            second_argument = self.query(
                work, logical, source, _byte_offset(source, "٢")
            )

        self.assertEqual(inner["hover"]["name"], "قيمة")
        self.assertEqual(inner["hover"]["display"], "صحيح قيمة")
        self.assertEqual(inner["hover"]["declaration"]["line"], 8)
        self.assertEqual(outer["hover"]["declaration"]["line"], 6)
        self.assertEqual(inner["definition"]["range"]["start"]["line"], 8)
        self.assertEqual(
            [(item["range"]["start"]["line"], item["role"])
             for item in inner["references"]],
            [(8, "declaration"), (9, "reference")],
        )
        self.assertEqual(outer["definition"]["range"]["start"]["line"], 6)
        self.assertEqual(
            [item["range"]["start"]["line"] for item in outer["references"]],
            [6, 7, 11],
        )

        self.assertEqual(call["hover"]["kind"], "function")
        self.assertEqual(
            call["hover"]["display"],
            "صحيح اجمع(صحيح أول، صحيح ثان)",
        )
        self.assertEqual(call["hover"]["declaration"]["line"], 1)
        self.assertEqual(call["definition"]["range"]["start"]["line"], 1)
        self.assertEqual(
            [item["range"]["start"]["line"] for item in call["references"]],
            [1, 11],
        )

        signature = second_argument["signature_help"]
        self.assertEqual(signature["name"], "اجمع")
        self.assertEqual(signature["active_parameter"], 1)
        self.assertEqual(
            [parameter["label"] for parameter in signature["parameters"]],
            ["صحيح أول", "صحيح ثان"],
        )

    def test_resolves_included_prototype_and_exact_hover_bytes(self) -> None:
        source = (
            '#تضمين "واجهة.baahd"\n'
            "صحيح الرئيسية() {\n"
            "    إرجع ضاعف(٣).\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_include_") as temp:
            work = Path(temp)
            logical = work / "مصدر" / "رئيسي.baa"
            logical.parent.mkdir()
            header = logical.parent / "واجهة.baahd"
            header.write_text(
                "خارجي صحيح ضاعف(صحيح قيمة).\n",
                encoding="utf-8",
            )
            position = _byte_offset(source, "ضاعف")
            result = self.query(work, logical, source, position)

        hover = result["hover"]
        self.assertEqual(hover["display"], "صحيح ضاعف(صحيح قيمة)")
        self.assertEqual(Path(hover["declaration"]["file"]), header.resolve())
        self.assertEqual(Path(result["definition"]["file"]), header.resolve())
        self.assertEqual(result["definition"]["range"]["start"]["line"], 1)
        self.assertNotIn("byte", result["definition"]["range"]["start"])
        self.assertEqual(len(result["references"]), 2)
        self.assertEqual(
            {Path(item["file"]) for item in result["references"]},
            {header.resolve(), logical.resolve()},
        )
        encoded = source.encode("utf-8")
        start = hover["range"]["start"]["byte"]
        end = hover["range"]["end"]["byte"]
        self.assertEqual(encoded[start:end].decode("utf-8"), "ضاعف")

    def test_references_ignore_text_and_comments(self) -> None:
        source = (
            "صحيح الرئيسية() {\n"
            "    صحيح قيمة = ١.\n"
            "    اطبع قيمة.\n"
            "    إرجع قيمة.\n"
            "}\n"
            "// قيمة داخل تعليق ليست مرجعاً\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_refs_") as temp:
            work = Path(temp)
            logical = work / "مراجع.baa"
            result = self.query(
                work, logical, source, _byte_offset(source, "قيمة", 0)
            )

        self.assertIsNotNone(result["hover"], result)
        self.assertIsNotNone(result["definition"], result)
        self.assertEqual(
            [item["range"]["start"]["line"] for item in result["references"]],
            [2, 3, 4],
        )
        self.assertEqual(
            [item["role"] for item in result["references"]],
            ["declaration", "reference", "reference"],
        )

    def test_signature_survives_an_incomplete_call_while_typing(self) -> None:
        source = (
            "صحيح اجمع(صحيح أول، صحيح ثان) {\n"
            "    إرجع أول + ثان.\n"
            "}\n"
            "صحيح الرئيسية() {\n"
            "    اجمع(\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_incomplete_") as temp:
            work = Path(temp)
            logical = work / "كتابة.baa"
            result = self.query(
                work, logical, source, len(source.encode("utf-8"))
            )

        self.assertIsNone(result["hover"])
        self.assertEqual(
            result["signature_help"]["label"],
            "صحيح اجمع(صحيح أول، صحيح ثان)",
        )
        self.assertEqual(result["signature_help"]["active_parameter"], 0)

    def test_signature_survives_a_temporary_arity_error_from_autopair(self) -> None:
        source = (
            "صحيح اجمع(صحيح أول، صحيح ثان) {\n"
            "    إرجع أول + ثان.\n"
            "}\n"
            "صحيح الرئيسية() {\n"
            "    إرجع اجمع().\n"
            "}\n"
        )
        with tempfile.TemporaryDirectory(prefix="baa_semantic_autopair_") as temp:
            work = Path(temp)
            logical = work / "إكمال تلقائي.baa"
            close_parenthesis = _byte_offset(source, "اجمع", 1) + len(
                "اجمع(".encode("utf-8")
            )
            result = self.query(work, logical, source, close_parenthesis)

        self.assertIsNone(result["hover"])
        self.assertEqual(
            result["signature_help"]["label"],
            "صحيح اجمع(صحيح أول، صحيح ثان)",
        )
        self.assertEqual(result["signature_help"]["active_parameter"], 0)

    def test_rejects_missing_or_invalid_query_position(self) -> None:
        with tempfile.TemporaryDirectory(prefix="baa_semantic_cli_") as temp:
            work = Path(temp)
            source = work / "رئيسي.baa"
            source.write_text(
                "صحيح الرئيسية() { إرجع ٠. }\n",
                encoding="utf-8",
            )
            missing = subprocess.run(
                [str(self.baa), "--semantic-query=json", str(source)],
                cwd=str(work),
                capture_output=True,
                timeout=30,
            )
            alone = subprocess.run(
                [str(self.baa), "--position-byte=0", str(source)],
                cwd=str(work),
                capture_output=True,
                timeout=30,
            )

        self.assertEqual(missing.returncode, 2)
        self.assertEqual(alone.returncode, 2)


if __name__ == "__main__":
    unittest.main()
