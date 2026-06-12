#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

from qa_parser_harness import (  # noqa: E402
    COMPILE_TIMEOUT_S,
    NEGATIVE_CASES,
    POSITIVE_CASES,
    ROOT,
    _build_harness_compiler,
    _configure_harness_build,
    _diagnostic_count,
    _expected_diag_count,
    _expected_markers,
    _find_compiler,
    _run_capture,
    _safe_clean_dir,
)


DEFAULT_OUT_DIR = ROOT / f".baa_parser_parity_out_{os.getpid()}"
AST_DUMP_TIMEOUT_S = 45.0


def _as_repo_path(path_text: str) -> Path:
    path = Path(path_text)
    if not path.is_absolute():
        path = ROOT / path
    return path.resolve()


def _normalize_text(text: str) -> str:
    root_native = str(ROOT)
    root_slash = root_native.replace("\\", "/")
    normalized = (text or "").replace("\r\n", "\n").replace("\r", "\n")
    normalized = normalized.replace(root_native, "<ROOT>")
    normalized = normalized.replace(root_slash, "<ROOT>")
    normalized = normalized.replace("\\", "/")
    return "\n".join(line.rstrip() for line in normalized.split("\n"))


def _safe_case_name(rel: str) -> str:
    out: list[str] = []
    for ch in rel:
        if ch.isalnum() or ch in ("_", "-", "."):
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def _write_mismatch(out_dir: Path, rel: str, suffix: str, text: str) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"{_safe_case_name(rel)}.{suffix}"
    path.write_text(text, encoding="utf-8", errors="replace")


def _check_ast_case(production_compiler: Path, baseline_compiler: Path, out_dir: Path, rel: str) -> None:
    production_res = _run_capture([str(production_compiler), "--dump-ast", rel], AST_DUMP_TIMEOUT_S)
    baseline_res = _run_capture([str(baseline_compiler), "--dump-ast", rel], AST_DUMP_TIMEOUT_S)

    if production_res.returncode != 0:
        sys.stdout.write(production_res.stdout or "")
        sys.stderr.write(production_res.stderr or "")
        raise RuntimeError(f"production compiler failed AST dump: {rel}")
    if baseline_res.returncode != 0:
        sys.stdout.write(baseline_res.stdout or "")
        sys.stderr.write(baseline_res.stderr or "")
        raise RuntimeError(f"C baseline compiler failed AST dump: {rel}")

    production_ast = _normalize_text(production_res.stdout or "")
    baseline_ast = _normalize_text(baseline_res.stdout or "")
    if production_ast != baseline_ast:
        mismatch_dir = out_dir / "ast-mismatches"
        _write_mismatch(mismatch_dir, rel, "production.ast", production_ast)
        _write_mismatch(mismatch_dir, rel, "baseline.ast", baseline_ast)
        raise RuntimeError(f"AST parity mismatch: {rel}")

    print(f"parser-parity: PASS ast {rel}")


def _check_expected_markers(src: Path, rel: str, output_text: str, side: str) -> None:
    missing = [marker for marker in _expected_markers(src) if marker not in output_text]
    if missing:
        raise RuntimeError(f"{side} missing expected diagnostics {missing}: {rel}")

    expected_count = _expected_diag_count(src)
    if expected_count is not None:
        actual_count = _diagnostic_count(output_text)
        if actual_count != expected_count:
            raise RuntimeError(
                f"{side} diagnostic count mismatch for {rel}: "
                f"expected {expected_count}, got {actual_count}"
            )


def _check_diagnostic_case(production_compiler: Path, baseline_compiler: Path, out_dir: Path, rel: str) -> None:
    src = ROOT / rel
    case_out = out_dir / "diag-outputs"
    case_out.mkdir(parents=True, exist_ok=True)

    production_res = _run_capture(
        [str(production_compiler), "-O2", "--verify", rel, "-o", str(case_out / "production.out")],
        COMPILE_TIMEOUT_S,
    )
    baseline_res = _run_capture(
        [str(baseline_compiler), "-O2", "--verify", rel, "-o", str(case_out / "baseline.out")],
        COMPILE_TIMEOUT_S,
    )

    if production_res.returncode == 0:
        raise RuntimeError(f"production compiler unexpectedly accepted negative case: {rel}")
    if baseline_res.returncode == 0:
        raise RuntimeError(f"C baseline compiler unexpectedly accepted negative case: {rel}")

    production_output = (production_res.stdout or "") + (production_res.stderr or "")
    baseline_output = (baseline_res.stdout or "") + (baseline_res.stderr or "")
    _check_expected_markers(src, rel, production_output, "production")
    _check_expected_markers(src, rel, baseline_output, "baseline")

    production_norm = _normalize_text(production_output)
    baseline_norm = _normalize_text(baseline_output)
    if production_norm != baseline_norm:
        mismatch_dir = out_dir / "diagnostic-mismatches"
        _write_mismatch(mismatch_dir, rel, "production.diag", production_norm)
        _write_mismatch(mismatch_dir, rel, "baseline.diag", baseline_norm)
        raise RuntimeError(f"diagnostic parity mismatch: {rel}")

    print(f"parser-parity: PASS diagnostic {rel}")


def _resolve_baseline_compiler(args: argparse.Namespace, compiler: Path, out_dir: Path) -> Path:
    if args.baseline_compiler:
        baseline_compiler = _as_repo_path(args.baseline_compiler)
        if not baseline_compiler.exists():
            raise FileNotFoundError(f"missing C baseline compiler: {baseline_compiler}")
        return baseline_compiler

    build_dir = out_dir / "build"
    _configure_harness_build(compiler, build_dir, use_baa_parser=False)
    return _build_harness_compiler(build_dir)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare production Baa parser wrapper output with the C parser baseline")
    parser.add_argument("--compiler", default="")
    parser.add_argument("--baseline-compiler", "--harness-compiler", dest="baseline_compiler", default="")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    parser.add_argument("--keep-artifacts", action="store_true")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir

    passed = False
    try:
        compiler = _find_compiler(args.compiler or None)
        _safe_clean_dir(out_dir)
        baseline_compiler = _resolve_baseline_compiler(args, compiler, out_dir)

        for _case_name, rel, _should_run in POSITIVE_CASES:
            _check_ast_case(compiler, baseline_compiler, out_dir, rel)

        for rel in NEGATIVE_CASES:
            _check_diagnostic_case(compiler, baseline_compiler, out_dir, rel)

        print(f"parser-parity: PASS compiler={compiler} baseline={baseline_compiler}")
        passed = True
        return 0
    except Exception as exc:
        print(f"parser-parity: FAIL {exc}", file=sys.stderr)
        print(f"parser-parity: artifacts={out_dir}", file=sys.stderr)
        return 1
    finally:
        if passed and not args.keep_artifacts:
            shutil.rmtree(out_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
