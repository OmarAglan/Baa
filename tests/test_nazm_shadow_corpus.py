from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "inventory_nazm_shadow_corpus.py"
INVENTORY_PATH = ROOT / "docs" / "generated" / "assembly_surface_v1.json"
MATRIX_PATH = ROOT / "docs" / "generated" / "baa_nazm_shadow_corpus_v1.json"
COVERAGE_PATH = ROOT / "docs" / "generated" / "baa_nazm_coverage_v1.json"
SPEC = importlib.util.spec_from_file_location("inventory_nazm_shadow_corpus", SCRIPT)
assert SPEC and SPEC.loader
MATRIX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MATRIX)


EXPECTED_TARGETS = {
    "x86_64-linux": {
        "summary": {"emitted": 46, "unsupported": 54, "error": 0},
        "blockers": {
            "إعدادات_التحويل": 4,
            "اسم_دالة_غير_عربي": 4,
            "تعليمة_آلة": 3,
            "ذاكرة_غير_مدعومة_لهذه_التعليمة": 13,
            "عرض_أو_نوع_معامل": 3,
            "نوع_معامل_قيمة_غير_مدعوم": 27,
        },
        "emitted_sources_sha256": (
            "1291c64dd31d03f6efdc3b31ad6c23f9fb80fe52d5d7c0fff988963bd573e090"
        ),
    },
    "x86_64-windows": {
        "summary": {"emitted": 47, "unsupported": 53, "error": 0},
        "blockers": {
            "إعدادات_التحويل": 4,
            "اسم_دالة_غير_عربي": 4,
            "تعليمة_آلة": 3,
            "ذاكرة_غير_مدعومة_لهذه_التعليمة": 15,
            "نوع_معامل_قيمة_غير_مدعوم": 27,
        },
        "emitted_sources_sha256": (
            "7e08d8fc33a468a9faaf068a1e4f9e753e46956b1d381c84a0b2b451bd668ccf"
        ),
    },
}


def _find_baa() -> Path | None:
    configured = os.environ.get("BAA")
    candidates = [
        Path(configured) if configured else None,
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build" / "baa.exe",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for candidate in candidates:
        if candidate and candidate.is_file():
            return candidate
    return None


class NazmShadowCorpusTests(unittest.TestCase):
    def setUp(self) -> None:
        self.inventory = json.loads(INVENTORY_PATH.read_text(encoding="utf-8"))
        self.matrix = json.loads(MATRIX_PATH.read_text(encoding="utf-8"))
        self.coverage = json.loads(COVERAGE_PATH.read_text(encoding="utf-8"))

    def test_every_inventory_source_has_an_explicit_shadow_result(self) -> None:
        self.assertEqual(self.matrix["schema"], "baa-nazm-shadow-corpus-v1")
        self.assertEqual(
            set(self.matrix["targets"]), {"x86_64-linux", "x86_64-windows"}
        )

        for target_name, inventory_target in self.inventory["targets"].items():
            target = self.matrix["targets"][target_name]
            expected = EXPECTED_TARGETS[target_name]
            rows = target["sources"]
            self.assertEqual(target["source_count"], 100)
            self.assertEqual(len(rows), 100)
            self.assertEqual(
                [row["source"] for row in rows], inventory_target["sources"]
            )
            self.assertEqual(target["summary"], expected["summary"])
            unsupported_count = expected["summary"]["unsupported"]
            self.assertEqual(
                sum(item["count"] for item in target["unsupported_reasons"]),
                unsupported_count,
            )
            self.assertEqual(
                sum(item["count"] for item in target["unsupported_blockers"]),
                unsupported_count,
            )
            self.assertTrue(all(
                row["status"] in {"emitted", "unsupported"} for row in rows
            ))
            self.assertTrue(all(
                row["exit_code"] == (0 if row["status"] == "emitted" else 3)
                for row in rows
            ))
            self.assertTrue(all(
                row.get("reason") for row in rows if row["status"] == "unsupported"
            ))
            self.assertTrue(all(
                row.get("blocker", {}).get("kind")
                for row in rows
                if row["status"] == "unsupported"
            ))
            blockers_by_kind: dict[str, int] = {}
            for blocker in target["unsupported_blockers"]:
                blockers_by_kind[blocker["kind"]] = (
                    blockers_by_kind.get(blocker["kind"], 0) + blocker["count"]
                )
            self.assertEqual(blockers_by_kind, expected["blockers"])

            emitted = [row for row in rows if row["status"] == "emitted"]
            self.assertEqual(
                hashlib.sha256(
                    "\n".join(row["source"] for row in emitted).encode("utf-8")
                ).hexdigest(),
                expected["emitted_sources_sha256"],
            )
            self.assertTrue(all(
                re.fullmatch(r"[0-9a-f]{64}", row["sha256"])
                for row in emitted
            ))

    def test_coverage_contract_embeds_the_full_shadow_matrix(self) -> None:
        raw = MATRIX_PATH.read_bytes()
        self.assertEqual(
            self.coverage["shadow_corpus"]["sha256"],
            hashlib.sha256(raw).hexdigest(),
        )
        for target_name, matrix_target in self.matrix["targets"].items():
            embedded = self.coverage["targets"][target_name]["corpus"][
                "shadow_matrix"
            ]
            self.assertEqual(embedded, matrix_target)

    def test_generated_artifact_names_are_arabic_only(self) -> None:
        name = MATRIX._arabic_output_name(42)
        self.assertEqual(name, "خرج-٠٠٤٢.نظم")
        self.assertIsNone(re.search(r"[A-Za-z]", name))

    def test_inventory_output_flags_do_not_mask_emitter_classification(self) -> None:
        index = MATRIX._source_flag_index({
            "source_flags": [{
                "source": "عينة.باء",
                "flags": ["-O1", "-S", "-fruntime-checks"],
            }]
        })
        self.assertEqual(index["عينة.باء"], ["-O1", "-fruntime-checks"])

    def test_arabic_blocker_suffix_is_parsed_without_becoming_the_reason(self) -> None:
        reason, blocker = MATRIX._diagnostic_and_blocker(
            "خطأ: صيغة غير مدعومة. "
            "[عائق_نظم=تعليمة_آلة؛تفصيل=جمع]\n"
        )
        self.assertEqual(reason, "خطأ: صيغة غير مدعومة.")
        self.assertEqual(blocker, {"kind": "تعليمة_آلة", "detail": "جمع"})

    @unittest.skipUnless(_find_baa(), "Baa compiler is unavailable")
    def test_checked_matrix_is_fresh_against_the_current_compiler(self) -> None:
        compiler = _find_baa()
        assert compiler
        exit_code = MATRIX.main([
            "--compiler",
            str(compiler),
            "--inventory",
            str(INVENTORY_PATH),
            "--output",
            str(MATRIX_PATH),
            "--check",
        ])
        self.assertEqual(exit_code, 0)


if __name__ == "__main__":
    unittest.main()
