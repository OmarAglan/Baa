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
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LOG_DIR = ROOT / ".baa_mixed_harness_logs"
DEFAULT_OUT_DIR = ROOT / ".baa_mixed_harness_out"
SNAPSHOT_DIR = ROOT / "tests" / "snapshots" / "mixed_harness" / "lexer_token_stream"
DIAGNOSTIC_SNAPSHOT_DIR = ROOT / "tests" / "snapshots" / "mixed_harness" / "lexer_diagnostics"
LEXER_FIXTURE_DIR = ROOT / "tests" / "fixtures" / "mixed_harness" / "lexer"

PILOT_SRC = ROOT / "src" / "frontend" / "lexer_token_names_baa0.baa"
LEXER_CANDIDATE_SRC = ROOT / "src" / "frontend" / "lexer_candidate_baa0.baa"
LEXER_TRANSITION_SRC = ROOT / "src" / "frontend" / "lexer_transition_baa0.baa"
LEXER_HEADER = ROOT / "src" / "frontend" / "lexer.h"
LEXER_DEBUG_C = ROOT / "src" / "frontend" / "lexer_debug.c"

TIMEOUT_S = 60.0

TARGET_TOKEN_NAMES = "token-names"
TARGET_LEXER_TOKEN_STREAM = "lexer-token-stream"
TARGET_LEXER_DIAGNOSTICS = "lexer-diagnostics"
TARGET_LEXER_TRANSITION = "lexer-transition"
TARGET_ALL = "all"

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

LEXER_CASES = [
    ("basic_utf8", LEXER_FIXTURE_DIR / "basic_utf8.baa"),
    ("macro_include", LEXER_FIXTURE_DIR / "macro_include_main.baa"),
    ("conditional_macros", LEXER_FIXTURE_DIR / "conditional_macros.baa"),
    ("stress_utf8_identifiers", ROOT / "tests" / "stress" / "stress_utf8_identifiers.baa"),
]

LEXER_DIAGNOSTIC_CASES = [
    ("bad_escape_string", ROOT / "tests" / "neg" / "lexer_bad_escape_in_string.baa"),
    ("bad_escape_char", ROOT / "tests" / "neg" / "lexer_bad_escape_in_char.baa"),
    ("unclosed_ifdef", ROOT / "tests" / "neg" / "syntax_pp_unclosed_ifdef.baa"),
    ("include_cycle", ROOT / "tests" / "neg" / "syntax_include_cycle_relative_alias.baa"),
]


@dataclass
class CheckResult:
    name: str
    passed: bool
    returncode: int
    duration_s: float
    detail: str
    artifact: str = ""


def _rel(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT.resolve())).replace("\\", "/")
    except ValueError:
        return str(path).replace("\\", "/")


def _safe_name(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in ("_", "-", ".") else "_" for ch in value)


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


def _run_capture(name: str, cmd: list[str], log_dir: Path, timeout_s: float = TIMEOUT_S) -> tuple[CheckResult, str]:
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
        return (
            CheckResult(name, passed, int(proc.returncode), duration, "ok" if passed else "non-zero exit"),
            proc.stdout or "",
        )
    except subprocess.TimeoutExpired as exc:
        duration = time.monotonic() - started
        stdout = exc.stdout or ""
        stderr = exc.stderr or ""
        (log_dir / f"{_safe_name(name)}.stdout.log").write_text(stdout, encoding="utf-8")
        (log_dir / f"{_safe_name(name)}.stderr.log").write_text(stderr, encoding="utf-8")
        return (CheckResult(name, False, 124, duration, f"timeout ({timeout_s}s)"), stdout)


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


def _policy_check_baa0_source(name: str, source: Path) -> CheckResult:
    text = source.read_text(encoding="utf-8", errors="replace")
    stripped = _strip_comments_and_literals(text)
    violations: list[str] = []
    for word in BANNED_WORDS:
        pattern = re.compile(rf"(?<![\w\u0600-\u06FF]){re.escape(word)}(?![\w\u0600-\u06FF])")
        if pattern.search(stripped):
            violations.append(word)
    for pattern_name, pattern in BANNED_PATTERNS:
        if pattern.search(stripped):
            violations.append(pattern_name)

    passed = not violations
    return CheckResult(
        name=name,
        passed=passed,
        returncode=0 if passed else 1,
        duration_s=0.0,
        detail="ok" if passed else "banned Baa0 feature(s): " + ", ".join(violations),
    )


def _policy_check_pilot() -> CheckResult:
    return _policy_check_baa0_source("token-names-baa0-policy", PILOT_SRC)


def _write_token_name_harness(path: Path, token_names: list[str]) -> None:
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

    printf(\"mixed-harness: token-name parity PASS (%d tokens)\\n\", {count});
    return 0;
}}
""",
        encoding="utf-8",
    )


def _write_lexer_dump_harness(path: Path) -> None:
    path.write_text(
        r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/frontend/lexer.h"

static char* read_all(const char* path)
{
    FILE* f = fopen(path, "rb");
    long size = 0;
    char* buf = NULL;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char*)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void slashify(char* text)
{
    if (!text) return;
    for (char* p = text; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
}

static const char* normalize_path(const char* root, const char* path, char* out, size_t out_cap)
{
    size_t root_len = root ? strlen(root) : 0u;
    size_t path_len = path ? strlen(path) : 0u;
    if (!out || out_cap == 0u || !path) return "";
    if (path_len + 1u > out_cap) return path;
    memcpy(out, path, path_len + 1u);
    slashify(out);

    if (root && root_len > 0u) {
        char root_buf[4096];
        if (root_len + 1u < sizeof(root_buf)) {
            memcpy(root_buf, root, root_len + 1u);
            slashify(root_buf);
            root_len = strlen(root_buf);
            if (strncmp(out, root_buf, root_len) == 0) {
                const char* rel = out + root_len;
                if (*rel == '/') ++rel;
                memmove(out, rel, strlen(rel) + 1u);
            }
        }
    }
    return out;
}

static void json_string(const char* text)
{
    putchar('"');
    if (text) {
        for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
            if (*p == '"' || *p == '\\') {
                putchar('\\');
                putchar((int)*p);
            } else if (*p == '\n') {
                fputs("\\n", stdout);
            } else if (*p == '\r') {
                fputs("\\r", stdout);
            } else if (*p == '\t') {
                fputs("\\t", stdout);
            } else if (*p < 0x20u) {
                printf("\\u%04x", (unsigned int)*p);
            } else {
                putchar((int)*p);
            }
        }
    }
    putchar('"');
}

static void print_token(const char* root, const char* case_path, Token tok, int index)
{
    char case_norm[4096];
    char file_norm[4096];
    const char* token_name = token_type_to_str(tok.type);
    printf("{\"case\":");
    json_string(normalize_path(root, case_path, case_norm, sizeof(case_norm)));
    printf(",\"index\":%d,\"type\":%d,\"name\":", index, (int)tok.type);
    json_string(token_name ? token_name : "");
    printf(",\"value\":");
    json_string(tok.value ? tok.value : "");
    printf(",\"line\":%d,\"col\":%d,\"length\":%d,\"file\":",
           tok.line,
           tok.col,
           tok.length);
    json_string(normalize_path(root, tok.filename, file_norm, sizeof(file_norm)));
    printf("}\n");
}

int main(int argc, char** argv)
{
    const char* root = NULL;
    if (argc < 3) {
        fprintf(stderr, "usage: lexer_token_dump <repo-root> <source>...\n");
        return 2;
    }
    root = argv[1];
    for (int argi = 2; argi < argc; ++argi) {
        const char* path = argv[argi];
        char* source = read_all(path);
        Lexer lexer;
        int index = 0;
        if (!source) {
            fprintf(stderr, "failed to read %s\n", path);
            return 3;
        }
        lexer_init(&lexer, source, path, NULL, 0u);
        for (;;) {
            Token tok = lexer_next_token(&lexer);
            print_token(root, path, tok, index++);
            if (tok.value) free(tok.value);
            if (tok.type == TOKEN_EOF || tok.type == TOKEN_INVALID) break;
            if (index > 20000) {
                fprintf(stderr, "token limit exceeded for %s\n", path);
                free(source);
                lexer_free_dependencies(&lexer);
                return 4;
            }
        }
        lexer_free_dependencies(&lexer);
        free(source);
    }
    return 0;
}
''',
        encoding="utf-8",
    )


def _write_lexer_candidate_harness(path: Path) -> None:
    path.write_text(
        r'''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/frontend/lexer.h"

extern int64_t مرشح_المحلل_اللفظي_شغل(int64_t root_file_id);

static const char* g_repo_root = NULL;
static const char* g_case_path = NULL;

static char* read_all(const char* path)
{
    FILE* f = fopen(path, "rb");
    long size = 0;
    char* buf = NULL;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char*)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void slashify(char* text)
{
    if (!text) return;
    for (char* p = text; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
}

static const char* normalize_path(const char* root, const char* path, char* out, size_t out_cap)
{
    size_t root_len = root ? strlen(root) : 0u;
    size_t path_len = path ? strlen(path) : 0u;
    if (!out || out_cap == 0u || !path) return "";
    if (path_len + 1u > out_cap) return path;
    memcpy(out, path, path_len + 1u);
    slashify(out);

    if (root && root_len > 0u) {
        char root_buf[4096];
        if (root_len + 1u < sizeof(root_buf)) {
            memcpy(root_buf, root, root_len + 1u);
            slashify(root_buf);
            root_len = strlen(root_buf);
            if (strncmp(out, root_buf, root_len) == 0) {
                const char* rel = out + root_len;
                if (*rel == '/') ++rel;
                memmove(out, rel, strlen(rel) + 1u);
            }
        }
    }
    return out;
}

static void json_string(const char* text)
{
    putchar('"');
    if (text) {
        for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
            if (*p == '"' || *p == '\\') {
                putchar('\\');
                putchar((int)*p);
            } else if (*p == '\n') {
                fputs("\\n", stdout);
            } else if (*p == '\r') {
                fputs("\\r", stdout);
            } else if (*p == '\t') {
                fputs("\\t", stdout);
            } else if (*p < 0x20u) {
                printf("\\u%04x", (unsigned int)*p);
            } else {
                putchar((int)*p);
            }
        }
    }
    putchar('"');
}

static void print_token(const char* root, const char* case_path, Token tok, int index)
{
    char case_norm[4096];
    char file_norm[4096];
    const char* token_name = token_type_to_str(tok.type);
    printf("{\"case\":");
    json_string(normalize_path(root, case_path, case_norm, sizeof(case_norm)));
    printf(",\"index\":%d,\"type\":%d,\"name\":", index, (int)tok.type);
    json_string(token_name ? token_name : "");
    printf(",\"value\":");
    json_string(tok.value ? tok.value : "");
    printf(",\"line\":%d,\"col\":%d,\"length\":%d,\"file\":",
           tok.line,
           tok.col,
           tok.length);
    json_string(normalize_path(root, tok.filename, file_norm, sizeof(file_norm)));
    printf("}\n");
}

static int dump_current_case(void)
{
    char* source = NULL;
    Lexer lexer;
    int index = 0;
    if (!g_repo_root || !g_case_path) {
        fprintf(stderr, "candidate host case is not initialized\n");
        return 2;
    }

    source = read_all(g_case_path);
    if (!source) {
        fprintf(stderr, "failed to read %s\n", g_case_path);
        return 3;
    }

    lexer_init(&lexer, source, g_case_path, NULL, 0u);
    for (;;) {
        Token tok = lexer_next_token(&lexer);
        print_token(g_repo_root, g_case_path, tok, index++);
        if (tok.value) free(tok.value);
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_INVALID) break;
        if (index > 20000) {
            fprintf(stderr, "token limit exceeded for %s\n", g_case_path);
            free(source);
            lexer_free_dependencies(&lexer);
            return 4;
        }
    }

    lexer_free_dependencies(&lexer);
    free(source);
    return 0;
}

int64_t مرشح_المضيف_شغل_الأساس(int64_t root_file_id)
{
    (void)root_file_id;
    return (int64_t)dump_current_case();
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: lexer_candidate_dump <repo-root> <source>...\n");
        return 2;
    }

    g_repo_root = argv[1];
    for (int argi = 2; argi < argc; ++argi) {
        int64_t rc = 0;
        g_case_path = argv[argi];
        rc = مرشح_المحلل_اللفظي_شغل((int64_t)(argi - 2));
        if (rc != 0) {
            fprintf(stderr, "candidate returned %lld for %s\n", (long long)rc, g_case_path);
            return (int)rc;
        }
    }
    return 0;
}
''',
        encoding="utf-8",
    )


def _normalize_jsonl(text: str) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for line_no, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        try:
            item = json.loads(line)
        except json.JSONDecodeError as exc:
            raise RuntimeError(f"invalid JSONL at line {line_no}: {exc}") from exc
        if not isinstance(item, dict):
            raise RuntimeError(f"JSONL line {line_no} is not an object")
        for key in ("case", "file"):
            if isinstance(item.get(key), str):
                item[key] = item[key].replace("\\", "/")
        rows.append(item)
    return rows


def _write_snapshot(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = "".join(json.dumps(row, ensure_ascii=False, sort_keys=True) + "\n" for row in rows)
    path.write_text(text, encoding="utf-8")


def _read_snapshot(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        raise RuntimeError(f"missing snapshot: {_rel(path)}")
    return _normalize_jsonl(path.read_text(encoding="utf-8"))


def _diff_rows(expected: list[dict[str, Any]], actual: list[dict[str, Any]]) -> str:
    if len(expected) != len(actual):
        return f"row count mismatch: expected {len(expected)}, actual {len(actual)}"
    for i, (exp, got) in enumerate(zip(expected, actual)):
        if exp != got:
            return (
                f"first mismatch at row {i}: "
                f"expected={json.dumps(exp, ensure_ascii=False, sort_keys=True)} "
                f"actual={json.dumps(got, ensure_ascii=False, sort_keys=True)}"
            )
    return ""


def _run_token_names(baa: Path, cc: str, log_dir: Path, out_dir: Path) -> list[CheckResult]:
    results: list[CheckResult] = []
    policy = _policy_check_pilot()
    results.append(policy)
    if not policy.passed:
        return results

    token_names = _parse_token_names()
    harness_c = out_dir / "token_name_parity.c"
    pilot_obj = out_dir / "lexer_token_names_baa0.o"
    exe = out_dir / ("token_name_parity.exe" if os.name == "nt" else "token_name_parity")
    _write_token_name_harness(harness_c, token_names)

    compile_baa = _run(
        "token-names-compile-baa",
        [
            str(baa),
            "-O2",
            "--verify-ir",
            "--verify-ssa",
            "-c",
            str(PILOT_SRC.relative_to(ROOT)),
            "-o",
            str(pilot_obj),
        ],
        log_dir,
    )
    results.append(compile_baa)
    if not compile_baa.passed:
        return results

    link = _run(
        "token-names-link",
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
            str(exe),
        ],
        log_dir,
    )
    results.append(link)
    if not link.passed:
        return results

    results.append(_run("token-names-run", [str(exe)], log_dir))
    return results


def _run_lexer_token_stream(
    baa: Path,
    cc: str,
    log_dir: Path,
    out_dir: Path,
    update_snapshots: bool,
) -> list[CheckResult]:
    results: list[CheckResult] = []
    harness_c = out_dir / "lexer_token_dump.c"
    exe = out_dir / ("lexer_token_dump.exe" if os.name == "nt" else "lexer_token_dump")
    _write_lexer_dump_harness(harness_c)

    compile_c = _run(
        "lexer-token-stream-build-c-baseline",
        [
            cc,
            "-std=gnu11",
            "-finput-charset=UTF-8",
            "-I",
            str(ROOT),
            str(harness_c),
            str(ROOT / "src" / "frontend" / "lexer.c"),
            str(ROOT / "src" / "support" / "error.c"),
            str(ROOT / "src" / "support" / "read_file.c"),
            "-o",
            str(exe),
        ],
        log_dir,
    )
    results.append(compile_c)
    if not compile_c.passed:
        return results

    for case_name, case_path in LEXER_CASES:
        if not case_path.exists():
            results.append(CheckResult(f"lexer-token-stream:{case_name}", False, 1, 0.0, "missing fixture"))
            continue

        run, stdout = _run_capture(
            f"lexer-token-stream-run:{case_name}",
            [str(exe), str(ROOT), str(case_path)],
            log_dir,
        )
        if not run.passed:
            results.append(run)
            continue

        started = time.monotonic()
        snapshot = SNAPSHOT_DIR / f"{case_name}.jsonl"
        actual_path = log_dir / f"lexer-token-stream-{case_name}.actual.jsonl"
        actual_path.write_text(stdout, encoding="utf-8")
        try:
            actual = _normalize_jsonl(stdout)
            if update_snapshots:
                _write_snapshot(snapshot, actual)
                detail = "snapshot updated"
                passed = True
            else:
                expected = _read_snapshot(snapshot)
                diff = _diff_rows(expected, actual)
                passed = not diff
                detail = "ok" if passed else diff
                if diff:
                    (log_dir / f"lexer-token-stream-{case_name}.diff.txt").write_text(diff + "\n", encoding="utf-8")
        except RuntimeError as exc:
            passed = False
            detail = str(exc)
        results.append(
            CheckResult(
                f"lexer-token-stream:{case_name}",
                passed,
                0 if passed else 1,
                time.monotonic() - started,
                detail,
                _rel(snapshot),
            )
        )

    results.extend(_run_lexer_candidate_token_stream(baa, cc, log_dir, out_dir, update_snapshots))
    return results


def _run_lexer_candidate_token_stream(
    baa: Path,
    cc: str,
    log_dir: Path,
    out_dir: Path,
    update_snapshots: bool,
) -> list[CheckResult]:
    results: list[CheckResult] = []
    policy = _policy_check_baa0_source("lexer-candidate-baa0-policy", LEXER_CANDIDATE_SRC)
    results.append(policy)
    if not policy.passed:
        return results

    harness_c = out_dir / "lexer_candidate_dump.c"
    candidate_obj = out_dir / "lexer_candidate_baa0.o"
    exe = out_dir / ("lexer_candidate_dump.exe" if os.name == "nt" else "lexer_candidate_dump")
    _write_lexer_candidate_harness(harness_c)

    compile_baa = _run(
        "lexer-token-stream-compile-baa-candidate",
        [
            str(baa),
            "-O2",
            "--verify-ir",
            "--verify-ssa",
            "-c",
            str(LEXER_CANDIDATE_SRC.relative_to(ROOT)),
            "-o",
            str(candidate_obj),
        ],
        log_dir,
    )
    results.append(compile_baa)
    if not compile_baa.passed:
        return results

    link = _run(
        "lexer-token-stream-link-baa-candidate",
        [
            cc,
            "-std=gnu11",
            "-finput-charset=UTF-8",
            "-I",
            str(ROOT),
            str(harness_c),
            str(ROOT / "src" / "frontend" / "lexer.c"),
            str(ROOT / "src" / "support" / "error.c"),
            str(ROOT / "src" / "support" / "read_file.c"),
            str(candidate_obj),
            "-o",
            str(exe),
        ],
        log_dir,
    )
    results.append(link)
    if not link.passed:
        return results

    for case_name, case_path in LEXER_CASES:
        if not case_path.exists():
            results.append(CheckResult(f"lexer-token-stream-baa-candidate:{case_name}", False, 1, 0.0, "missing fixture"))
            continue

        run, stdout = _run_capture(
            f"lexer-token-stream-baa-candidate-run:{case_name}",
            [str(exe), str(ROOT), str(case_path)],
            log_dir,
        )
        if not run.passed:
            results.append(run)
            continue

        started = time.monotonic()
        snapshot = SNAPSHOT_DIR / f"{case_name}.jsonl"
        actual_path = log_dir / f"lexer-token-stream-baa-candidate-{case_name}.actual.jsonl"
        actual_path.write_text(stdout, encoding="utf-8")
        try:
            actual = _normalize_jsonl(stdout)
            if update_snapshots:
                expected = actual
                detail = "candidate bridge matched updated baseline"
                passed = True
            else:
                expected = _read_snapshot(snapshot)
                diff = _diff_rows(expected, actual)
                passed = not diff
                detail = "candidate bridge matched baseline" if passed else diff
                if diff:
                    (log_dir / f"lexer-token-stream-baa-candidate-{case_name}.diff.txt").write_text(
                        diff + "\n",
                        encoding="utf-8",
                    )
        except RuntimeError as exc:
            passed = False
            detail = str(exc)
        results.append(
            CheckResult(
                f"lexer-token-stream-baa-candidate:{case_name}",
                passed,
                0 if passed else 1,
                time.monotonic() - started,
                detail,
                _rel(snapshot),
            )
        )

    return results


def _normalize_diagnostic_text(text: str, out_dir: Path) -> str:
    root_native = str(ROOT.resolve())
    root_posix = root_native.replace("\\", "/")
    out_native = str(out_dir.resolve())
    out_posix = out_native.replace("\\", "/")
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    for root in (root_native, root_posix):
        normalized = normalized.replace(root + "\\", "")
        normalized = normalized.replace(root + "/", "")
        normalized = normalized.replace(root, ".")
    for out_root in (out_native, out_posix):
        normalized = normalized.replace(out_root + "\\", "<out>/")
        normalized = normalized.replace(out_root + "/", "<out>/")
        normalized = normalized.replace(out_root, "<out>")
    lines = [line.rstrip() for line in normalized.splitlines() if line.strip()]
    return "\n".join(lines) + ("\n" if lines else "")


def _run_lexer_diagnostics(
    baa: Path,
    log_dir: Path,
    out_dir: Path,
    update_snapshots: bool,
) -> list[CheckResult]:
    results: list[CheckResult] = []
    for case_name, case_path in LEXER_DIAGNOSTIC_CASES:
        if not case_path.exists():
            results.append(CheckResult(f"lexer-diagnostics:{case_name}", False, 1, 0.0, "missing fixture"))
            continue

        started = time.monotonic()
        output = out_dir / ("lexer_diag_tmp.exe" if os.name == "nt" else "lexer_diag_tmp")
        cmd = [str(baa), "-O1", _rel(case_path), "-o", str(output)]
        try:
            proc = subprocess.run(
                cmd,
                cwd=str(ROOT),
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=TIMEOUT_S,
            )
            duration = time.monotonic() - started
            raw = (proc.stderr or "") + (proc.stdout or "")
            (log_dir / f"lexer-diagnostics-{case_name}.stderr.log").write_text(proc.stderr or "", encoding="utf-8")
            (log_dir / f"lexer-diagnostics-{case_name}.stdout.log").write_text(proc.stdout or "", encoding="utf-8")
            actual = _normalize_diagnostic_text(raw, out_dir)
            actual_path = log_dir / f"lexer-diagnostics-{case_name}.actual.txt"
            actual_path.write_text(actual, encoding="utf-8")

            snapshot = DIAGNOSTIC_SNAPSHOT_DIR / f"{case_name}.txt"
            if proc.returncode == 0:
                results.append(
                    CheckResult(
                        f"lexer-diagnostics:{case_name}",
                        False,
                        1,
                        duration,
                        "expected compiler failure, got success",
                        _rel(snapshot),
                    )
                )
                continue

            if update_snapshots:
                snapshot.parent.mkdir(parents=True, exist_ok=True)
                snapshot.write_text(actual, encoding="utf-8")
                passed = True
                detail = "snapshot updated"
            else:
                if not snapshot.exists():
                    passed = False
                    detail = f"missing snapshot: {_rel(snapshot)}"
                else:
                    expected = snapshot.read_text(encoding="utf-8")
                    passed = expected == actual
                    detail = "ok" if passed else "diagnostic snapshot mismatch"
                    if not passed:
                        diff = _diff_rows(
                            [{"text": line} for line in expected.splitlines()],
                            [{"text": line} for line in actual.splitlines()],
                        )
                        (log_dir / f"lexer-diagnostics-{case_name}.diff.txt").write_text(diff + "\n", encoding="utf-8")

            results.append(
                CheckResult(
                    f"lexer-diagnostics:{case_name}",
                    passed,
                    0 if passed else 1,
                    duration,
                    detail,
                    _rel(snapshot),
                )
            )
        except subprocess.TimeoutExpired as exc:
            duration = time.monotonic() - started
            (log_dir / f"lexer-diagnostics-{case_name}.stdout.log").write_text(exc.stdout or "", encoding="utf-8")
            (log_dir / f"lexer-diagnostics-{case_name}.stderr.log").write_text(exc.stderr or "", encoding="utf-8")
            results.append(
                CheckResult(
                    f"lexer-diagnostics:{case_name}",
                    False,
                    124,
                    duration,
                    f"timeout ({TIMEOUT_S}s)",
                )
            )

    return results


def _write_lexer_transition_harness(path: Path) -> None:
    path.write_text(
        r'''#include <stdint.h>
#include <stdio.h>

extern int64_t انتقالالمحللاختبار(void);

int main(void)
{
    int64_t rc = انتقالالمحللاختبار();
    if (rc != 0) {
        fprintf(stderr, "lexer transition Baa harness failed: %lld\n", (long long)rc);
        return (int)rc;
    }

    puts("mixed-harness: lexer transition PASS");
    return 0;
}
''',
        encoding="utf-8",
    )


def _run_lexer_transition(baa: Path, cc: str, log_dir: Path, out_dir: Path) -> list[CheckResult]:
    results: list[CheckResult] = []
    policy = _policy_check_baa0_source("lexer-transition-baa0-policy", LEXER_TRANSITION_SRC)
    results.append(policy)
    if not policy.passed:
        return results

    harness_c = out_dir / "lexer_transition_harness.c"
    transition_obj = out_dir / "lexer_transition_baa0.o"
    exe = out_dir / ("lexer_transition_harness.exe" if os.name == "nt" else "lexer_transition_harness")
    _write_lexer_transition_harness(harness_c)

    compile_baa = _run(
        "lexer-transition-compile-baa",
        [
            str(baa),
            "-O2",
            "--verify-ir",
            "--verify-ssa",
            "-c",
            str(LEXER_TRANSITION_SRC.relative_to(ROOT)),
            "-o",
            str(transition_obj),
        ],
        log_dir,
    )
    results.append(compile_baa)
    if not compile_baa.passed:
        return results

    link = _run(
        "lexer-transition-link",
        [
            cc,
            "-std=gnu11",
            "-finput-charset=UTF-8",
            str(harness_c),
            str(transition_obj),
            "-o",
            str(exe),
        ],
        log_dir,
    )
    results.append(link)
    if not link.passed:
        return results

    results.append(_run("lexer-transition-run", [str(exe)], log_dir))
    return results


def _print_result(result: CheckResult) -> None:
    state = "PASS" if result.passed else "FAIL"
    artifact = f" artifact={result.artifact}" if result.artifact else ""
    print(f"[{state}] {result.name} (rc={result.returncode}, {result.duration_s:.2f}s) - {result.detail}{artifact}")


def _write_summary(path: str, baa: Path, cc: str, target: str, results: list[CheckResult], log_dir: Path) -> None:
    if not path:
        return
    out = Path(path)
    if not out.is_absolute():
        out = ROOT / out
    out.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "gate": "mixed-harness",
        "target": target,
        "compiler": str(baa),
        "cc": cc,
        "log_dir": _rel(log_dir),
        "passed": all(r.passed for r in results),
        "total": len(results),
        "failed": sum(1 for r in results if not r.passed),
        "results": [asdict(r) for r in results],
    }
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Reusable mixed C+Baa self-hosting harness")
    parser.add_argument(
        "--target",
        choices=[
            TARGET_TOKEN_NAMES,
            TARGET_LEXER_TOKEN_STREAM,
            TARGET_LEXER_DIAGNOSTICS,
            TARGET_LEXER_TRANSITION,
            TARGET_ALL,
        ],
        default=TARGET_ALL,
    )
    parser.add_argument("--baa", default="")
    parser.add_argument("--cc", default="")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR))
    parser.add_argument("--summary-json", default="")
    parser.add_argument("--update-snapshots", action="store_true")
    args = parser.parse_args(argv)

    baa = _find_baa(args.baa)
    cc = _find_cc(args.cc)
    log_dir = Path(args.log_dir)
    if not log_dir.is_absolute():
        log_dir = ROOT / log_dir
    if log_dir.exists():
        shutil.rmtree(log_dir, ignore_errors=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    out_dir = DEFAULT_OUT_DIR / str(os.getpid())
    if out_dir.exists():
        shutil.rmtree(out_dir, ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"mixed-harness: target={args.target}")
    print(f"mixed-harness: compiler={baa}")
    print(f"mixed-harness: cc={cc}")

    results: list[CheckResult] = []
    try:
        if args.target in (TARGET_TOKEN_NAMES, TARGET_ALL):
            results.extend(_run_token_names(baa, cc, log_dir, out_dir))
        if args.target in (TARGET_LEXER_TOKEN_STREAM, TARGET_ALL):
            results.extend(_run_lexer_token_stream(baa, cc, log_dir, out_dir, args.update_snapshots))
        if args.target in (TARGET_LEXER_DIAGNOSTICS, TARGET_ALL):
            results.extend(_run_lexer_diagnostics(baa, log_dir, out_dir, args.update_snapshots))
        if args.target in (TARGET_LEXER_TRANSITION, TARGET_ALL):
            results.extend(_run_lexer_transition(baa, cc, log_dir, out_dir))
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)

    for result in results:
        _print_result(result)

    _write_summary(args.summary_json, baa, cc, args.target, results, log_dir)
    return 0 if results and all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
