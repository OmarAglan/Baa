#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PILOT_SRC = ROOT / "src" / "frontend" / "lexer_token_names_baa0.baa"
LEXER_HEADER = ROOT / "src" / "frontend" / "lexer.h"
LEXER_DEBUG_C = ROOT / "src" / "frontend" / "lexer_debug.c"
DEFAULT_LOG_DIR = ROOT / ".baa_selfhost_logs"

TIMEOUT_S = 30.0

BANNED_WORDS = [
    "عشري",
    "عشري٣٢",
    "مجمع",
    "اتحاد",
    "اختر",
    "حالة",
    "افتراضي",
    "قائمة_معاملات",
    "بدء_معاملات",
    "معامل_تالي",
    "نهاية_معاملات",
    "وقت_حالي",
    "وقت_كنص",
    "عشوائي",
    "متغير_بيئة",
    "نفذ_أمر",
    "جذر_تربيعي",
    "جيب",
    "جيب_تمام",
    "ظل",
]

BANNED_PATTERNS = [
    ("function-pointer", re.compile(r"دالة\s*\(")),
    ("variadic-ellipsis", re.compile(r"\.\.\.")),
    ("multidim-array", re.compile(r"\]\s*\[")),
    ("float-literal", re.compile(r"(?<![\w\u0600-\u06FF])[\d٠-٩]+\.[\d٠-٩]+(?![\w\u0600-\u06FF])")),
]


@dataclass
class CheckResult:
    name: str
    passed: bool
    returncode: int
    duration_s: float
    detail: str


def _find_baa(arg: str) -> Path:
    if arg:
        p = Path(arg)
        if p.exists():
            return p

    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = (
        [ROOT / "build" / "baa.exe", ROOT / "build" / "baa"]
        if os.name == "nt"
        else [ROOT / "build-linux" / "baa", ROOT / "build" / "baa"]
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("Could not find compiler binary. Build it first, or pass --baa.")


def _find_cc(arg: str) -> str:
    if arg:
        return arg
    env = os.environ.get("CC")
    if env:
        return env
    if os.name == "nt":
        bundled = ROOT / "gcc" / "bin" / "gcc.exe"
        if bundled.exists():
            return str(bundled)
    return "gcc"


def _run(name: str, cmd: list[str], log_dir: Path, timeout_s: float = TIMEOUT_S) -> CheckResult:
    started = time.monotonic()
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(ROOT),
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=timeout_s,
        )
        duration = time.monotonic() - started
        (log_dir / f"{_safe_name(name)}.stdout.log").write_text(proc.stdout or "", encoding="utf-8")
        (log_dir / f"{_safe_name(name)}.stderr.log").write_text(proc.stderr or "", encoding="utf-8")
        passed = proc.returncode == 0
        return CheckResult(name, passed, int(proc.returncode), duration, "ok" if passed else "non-zero exit")
    except subprocess.TimeoutExpired as exc:
        duration = time.monotonic() - started
        (log_dir / f"{_safe_name(name)}.stdout.log").write_text(exc.stdout or "", encoding="utf-8")
        (log_dir / f"{_safe_name(name)}.stderr.log").write_text(exc.stderr or "", encoding="utf-8")
        return CheckResult(name, False, 124, duration, f"timeout ({timeout_s}s)")


def _safe_name(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in ("_", "-", ".") else "_" for ch in value)


def _print_result(result: CheckResult) -> None:
    state = "PASS" if result.passed else "FAIL"
    print(f"[{state}] {result.name} (rc={result.returncode}, {result.duration_s:.2f}s)")
    if not result.passed:
        print(f"       detail: {result.detail}")


def _parse_token_names() -> list[str]:
    text = LEXER_HEADER.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"typedef\s+enum\s*\{(?P<body>.*?)\}\s*BaaTokenType\s*;", text, re.S)
    if not match:
        raise RuntimeError("Could not find BaaTokenType enum in lexer.h")
    names: list[str] = []
    for raw in match.group("body").splitlines():
        line = raw.split("//", 1)[0].strip().rstrip(",")
        if line.startswith("TOKEN_"):
            names.append(line.split("=", 1)[0].strip())
    if not names or names[-1] != "TOKEN_INVALID":
        raise RuntimeError("BaaTokenType enum parse did not end at TOKEN_INVALID")
    return names


def _strip_comments_and_literals(text: str) -> str:
    out: list[str] = []
    i = 0
    in_string = False
    in_char = False
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if in_string:
            if ch == "\\":
                out.append(" ")
                if i + 1 < len(text):
                    out.append(" ")
                    i += 2
                    continue
            elif ch == "\"":
                in_string = False
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if in_char:
            if ch == "\\":
                out.append(" ")
                if i + 1 < len(text):
                    out.append(" ")
                    i += 2
                    continue
            elif ch == "'":
                in_char = False
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if ch == "/" and nxt == "/":
            while i < len(text) and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if ch == "\"":
            in_string = True
            out.append(" ")
            i += 1
            continue
        if ch == "'":
            in_char = True
            out.append(" ")
            i += 1
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def _policy_check_pilot() -> CheckResult:
    text = PILOT_SRC.read_text(encoding="utf-8", errors="replace")
    stripped = _strip_comments_and_literals(text)
    violations: list[str] = []
    for word in BANNED_WORDS:
        pattern = re.compile(rf"(?<![\w\u0600-\u06FF]){re.escape(word)}(?![\w\u0600-\u06FF])")
        if pattern.search(stripped):
            violations.append(word)
    for name, pattern in BANNED_PATTERNS:
        if pattern.search(stripped):
            violations.append(name)

    passed = not violations
    return CheckResult(
        name="selfhost-pilot-baa0-policy",
        passed=passed,
        returncode=0 if passed else 1,
        duration_s=0.0,
        detail="ok" if passed else "banned Baa0 feature(s): " + ", ".join(violations),
    )


def _write_harness(path: Path, token_names: list[str]) -> None:
    entries = ",\n".join(f"        {name}" for name in token_names)
    count = len(token_names)
    path.write_text(
        f"""#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include \"src/frontend/lexer.h\"

extern const uint64_t* اسم_نوع_وحدة_باء(int64_t type);

static int decode_baa_text(const uint64_t* text, char* out, size_t out_cap)
{{
    size_t pos = 0;
    if (!text || !out || out_cap == 0) return 0;

    for (size_t i = 0; text[i] != 0; ++i) {{
        uint64_t cell = text[i];
        unsigned char len = (unsigned char)((cell >> 32) & 0xffu);
        if (len == 0 || len > 4 || pos + len >= out_cap) return 0;
        for (unsigned char j = 0; j < len; ++j) {{
            out[pos++] = (char)((cell >> (8u * j)) & 0xffu);
        }}
    }}

    out[pos] = '\\0';
    return 1;
}}

static const BaaTokenType k_tokens[{count}] = {{
{entries}
}};

int main(void)
{{
    for (int i = 0; i < {count}; ++i) {{
        const char* expected = token_type_to_str(k_tokens[i]);
        char actual[128];
        const uint64_t* actual_text = اسم_نوع_وحدة_باء((int64_t)k_tokens[i]);
        if (!decode_baa_text(actual_text, actual, sizeof(actual)) ||
            !expected || strcmp(expected, actual) != 0) {{
            fprintf(stderr, \"token parity mismatch at %d: expected '%s' got '%s'\\n\",
                    i,
                    expected ? expected : \"<null>\",
                    actual_text ? actual : \"<null>\");
            return 1;
        }}
    }}

    char unknown[128];
    if (!decode_baa_text(اسم_نوع_وحدة_باء(-1), unknown, sizeof(unknown)) ||
        strcmp(unknown, \"UNKNOWN\") != 0) {{
        fprintf(stderr, \"unknown token parity mismatch\\n\");
        return 2;
    }}

    printf(\"selfhost-pilot: token-name parity PASS (%d tokens)\\n\", {count});
    return 0;
}}
""",
        encoding="utf-8",
    )


def _write_summary(path: str, baa: Path, cc: str, results: list[CheckResult], log_dir: Path) -> None:
    if not path:
        return
    out = ROOT / path if not Path(path).is_absolute() else Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "gate": "selfhost-pilot",
        "compiler": str(baa),
        "cc": cc,
        "log_dir": str(log_dir.relative_to(ROOT) if log_dir.is_relative_to(ROOT) else log_dir),
        "passed": all(r.passed for r in results),
        "total": len(results),
        "failed": sum(1 for r in results if not r.passed),
        "results": [asdict(r) for r in results],
    }
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Baa self-hosting pilot parity gate")
    parser.add_argument("--baa", default="")
    parser.add_argument("--cc", default="")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR))
    parser.add_argument("--summary-json", default="")
    args = parser.parse_args()

    log_dir = Path(args.log_dir)
    if log_dir.exists():
        shutil.rmtree(log_dir, ignore_errors=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    baa = _find_baa(args.baa)
    cc = _find_cc(args.cc)
    print(f"selfhost-pilot: compiler={baa}")
    print(f"selfhost-pilot: cc={cc}")

    results: list[CheckResult] = []
    out_dir = ROOT / f".baa_selfhost_pilot_out_{os.getpid()}"
    if out_dir.exists():
        shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    try:
        policy = _policy_check_pilot()
        results.append(policy)
        if not policy.passed:
            for result in results:
                _print_result(result)
            _write_summary(args.summary_json, baa, cc, results, log_dir)
            return 1

        token_names = _parse_token_names()
        harness_c = out_dir / "token_name_parity.c"
        pilot_obj = out_dir / ("lexer_token_names_baa0.o")
        harness_exe = out_dir / ("token_name_parity.exe" if os.name == "nt" else "token_name_parity")
        _write_harness(harness_c, token_names)

        compile_baa = _run(
            "selfhost-pilot-compile-baa",
            [str(baa), "-O2", "--verify-ir", "--verify-ssa", "-c", str(PILOT_SRC.relative_to(ROOT)), "-o", str(pilot_obj)],
            log_dir,
        )
        results.append(compile_baa)

        if compile_baa.passed:
            link = _run(
                "selfhost-pilot-link-harness",
                [
                    cc,
                    "-std=gnu11",
                    "-finput-charset=UTF-8",
                    "-I",
                    str(ROOT),
                    "-include",
                    "src/frontend/lexer.h",
                    str(harness_c),
                    str(LEXER_DEBUG_C),
                    str(pilot_obj),
                    "-o",
                    str(harness_exe),
                ],
                log_dir,
            )
            results.append(link)

            if link.passed:
                run = _run("selfhost-pilot-run-harness", [str(harness_exe)], log_dir)
                results.append(run)
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)

    for result in results:
        _print_result(result)

    _write_summary(args.summary_json, baa, cc, results, log_dir)
    return 0 if results and all(r.passed for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
