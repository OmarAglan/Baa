#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT_DIR = ROOT / f".baa_parser_harness_out_{os.getpid()}"

CONFIGURE_TIMEOUT_S = 90.0
BUILD_TIMEOUT_S = 180.0
COMPILE_TIMEOUT_S = 45.0
RUN_TIMEOUT_S = 20.0

POSITIVE_CASES = (
    ("frontend-parser-toplevel", "tests/integration/frontend/frontend_baa_parser_toplevel_test.baa", False),
    ("frontend-parser-decl-dispatch", "tests/integration/frontend/frontend_baa_parser_decl_dispatch_test.baa", False),
    ("frontend-parser-alias-shell", "tests/integration/frontend/frontend_baa_parser_alias_shell_test.baa", False),
    ("frontend-parser-qualifier-shell", "tests/integration/frontend/frontend_baa_parser_qualifier_shell_test.baa", True),
    ("backend-test", "tests/integration/backend/backend_test.baa", True),
    ("array-init", "tests/integration/backend/backend_array_init_test.baa", True),
    ("struct-pointer-member", "tests/integration/backend/backend_struct_pointer_member_test.baa", True),
    ("type-alias", "tests/integration/backend/backend_type_alias_test.baa", True),
    ("func-ptr", "tests/integration/backend/backend_func_ptr_test.baa", True),
)

NEGATIVE_CASES = (
    "tests/neg/syntax_missing_dot.baa",
    "tests/neg/syntax_unexpected_top_level_recovery.baa",
    "tests/neg/parser_top_level_recovery_then_decl.baa",
    "tests/neg/parser_decl_qualifier_missing_type.baa",
    "tests/neg/parser_decl_qualifier_duplicate_const.baa",
    "tests/neg/parser_decl_qualifier_duplicate_static.baa",
    "tests/neg/parser_type_alias_missing_name.baa",
    "tests/neg/parser_type_alias_missing_target.baa",
    "tests/neg/parser_funcptr_alias_nested_signature.baa",
    "tests/neg/parser_static_function_unsupported.baa",
    "tests/neg/type_alias_inside_function.baa",
    "tests/neg/type_alias_unknown_target_type.baa",
    "tests/neg/type_alias_redefine.baa",
)


def _find_compiler(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env = os.environ.get("BAA")
    if env:
        candidates.append(Path(env))
    if os.name == "nt":
        candidates.extend([ROOT / "build" / "baa.exe", ROOT / "build" / "baa"])
    else:
        candidates.extend([ROOT / "build-linux" / "baa", ROOT / "build" / "baa"])

    for candidate in candidates:
        path = candidate if candidate.is_absolute() else ROOT / candidate
        if path.exists():
            return path.resolve()

    raise FileNotFoundError("لم يتم العثور على مترجم باء. ابنِ المشروع أولاً أو مرر --compiler.")


def _safe_clean_dir(path: Path) -> None:
    resolved_root = ROOT.resolve()
    resolved = path.resolve()
    if resolved == resolved_root or resolved_root not in resolved.parents:
        raise ValueError(f"refusing to clean path outside repository: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True, exist_ok=True)


def _run_capture(cmd: list[str], timeout_s: float) -> subprocess.CompletedProcess[str]:
    print("+ " + " ".join(str(part) for part in cmd), flush=True)
    return subprocess.run(
        cmd,
        cwd=str(ROOT),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=timeout_s,
    )


def _run_required(cmd: list[str], timeout_s: float, label: str) -> None:
    try:
        result = _run_capture(cmd, timeout_s)
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(f"{label}: انتهت المهلة بعد {timeout_s}s") from exc
    if result.returncode != 0:
        if result.stdout:
            sys.stdout.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise RuntimeError(f"{label}: فشل الأمر برمز {result.returncode}")


def _compiler_path(build_dir: Path) -> Path:
    return build_dir / ("baa.exe" if os.name == "nt" else "baa")


def _configure_harness_build(compiler: Path, build_dir: Path, use_baa_parser: bool = True) -> None:
    cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(build_dir),
        f"-DBAA_BOOTSTRAP_COMPILER={compiler}",
        f"-DBAA_USE_BAA_PARSER_TOPLEVEL={'ON' if use_baa_parser else 'OFF'}",
        "-DBAA_WARNINGS_AS_ERRORS=ON",
    ]
    if os.name == "nt":
        cmd.extend(["-G", "MinGW Makefiles"])
    else:
        cmd.append("-DCMAKE_BUILD_TYPE=Release")
    _run_required(cmd, CONFIGURE_TIMEOUT_S, "configure parser harness")


def _build_harness_compiler(build_dir: Path) -> Path:
    _run_required(["cmake", "--build", str(build_dir)], BUILD_TIMEOUT_S, "build parser harness")
    compiler = _compiler_path(build_dir)
    if not compiler.exists():
        raise FileNotFoundError(f"missing parser harness compiler: {compiler}")
    return compiler


def _expected_markers(src: Path) -> list[str]:
    markers: list[str] = []
    for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if stripped.startswith("// EXPECT:"):
            markers.append(stripped.split(":", 1)[1].strip())
    return markers


def _expected_diag_count(src: Path) -> int | None:
    for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if stripped.startswith("// EXPECT-DIAG-COUNT:"):
            return int(stripped.split(":", 1)[1].strip(), 10)
    return None


def _diagnostic_count(stderr_text: str) -> int:
    return len(re.findall(r"(?m)^\[(?:Error|Warning)\]", stderr_text or ""))


def _compile_positive(compiler: Path, out_dir: Path, name: str, rel: str, run: bool) -> None:
    exe_ext = ".exe" if os.name == "nt" else ""
    out = out_dir / f"{name}{exe_ext}"
    result = _run_capture(
        [str(compiler), "-O2", "--verify", rel, "-o", str(out)],
        COMPILE_TIMEOUT_S,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout or "")
        sys.stderr.write(result.stderr or "")
        raise RuntimeError(f"positive case failed to compile: {rel}")

    if not run:
        print(f"parser-harness: PASS compile {rel}")
        return

    run_result = _run_capture([str(out)], RUN_TIMEOUT_S)
    if run_result.returncode != 0:
        sys.stdout.write(run_result.stdout or "")
        sys.stderr.write(run_result.stderr or "")
        raise RuntimeError(f"positive case failed at runtime: {rel}")
    print(f"parser-harness: PASS runtime {rel}")


def _compile_negative(compiler: Path, out_dir: Path, rel: str) -> None:
    src = ROOT / rel
    out = out_dir / f"{src.stem}.negative"
    result = _run_capture(
        [str(compiler), "-O2", "--verify", rel, "-o", str(out)],
        COMPILE_TIMEOUT_S,
    )
    if result.returncode == 0:
        raise RuntimeError(f"negative case unexpectedly compiled: {rel}")

    combined = (result.stdout or "") + (result.stderr or "")
    missing = [marker for marker in _expected_markers(src) if marker not in combined]
    if missing:
        sys.stdout.write(result.stdout or "")
        sys.stderr.write(result.stderr or "")
        raise RuntimeError(f"negative case missing expected diagnostics {missing}: {rel}")

    expected_count = _expected_diag_count(src)
    if expected_count is not None:
        actual_count = _diagnostic_count(result.stderr or "")
        if actual_count != expected_count:
            sys.stdout.write(result.stdout or "")
            sys.stderr.write(result.stderr or "")
            raise RuntimeError(
                f"negative case diagnostic count mismatch for {rel}: "
                f"expected {expected_count}, got {actual_count}"
            )

    print(f"parser-harness: PASS negative {rel}")


def _run_parser_corpus(compiler: Path, out_dir: Path) -> None:
    cases_out = out_dir / "cases"
    cases_out.mkdir(parents=True, exist_ok=True)

    for name, rel, should_run in POSITIVE_CASES:
        _compile_positive(compiler, cases_out, name, rel, should_run)

    for rel in NEGATIVE_CASES:
        _compile_negative(compiler, cases_out, rel)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build and test the Baa parser top-level harness")
    parser.add_argument("--compiler", default="")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    parser.add_argument("--keep-artifacts", action="store_true")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    build_dir = out_dir / "build"

    try:
        compiler = _find_compiler(args.compiler or None)
        _safe_clean_dir(out_dir)
        _configure_harness_build(compiler, build_dir)
        harness_compiler = _build_harness_compiler(build_dir)
        _run_parser_corpus(harness_compiler, out_dir)
        print(f"parser-harness: PASS compiler={harness_compiler}")
        return 0
    except Exception as exc:
        print(f"parser-harness: FAIL {exc}", file=sys.stderr)
        print(f"parser-harness: artifacts={out_dir}", file=sys.stderr)
        return 1
    finally:
        if not args.keep_artifacts:
            shutil.rmtree(out_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
