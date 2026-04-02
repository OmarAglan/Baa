#!/usr/bin/env python3

from __future__ import annotations

import argparse
import difflib
import json
import os
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class ReproCase:
    name: str
    channel: str
    expected_rc: int
    args: tuple[str, ...]
    output_name: str


CASES: tuple[ReproCase, ...] = (
    ReproCase(
        name="ir-opt-main",
        channel="stdout",
        expected_rc=0,
        args=(
            "-O2",
            "--verify",
            "--dump-ir-opt",
            "-S",
            "tests/integration/ir/ir_test.baa",
            "-o",
            "{output}",
        ),
        output_name="ir_opt_main.s",
    ),
    ReproCase(
        name="ir-opt-phi",
        channel="stdout",
        expected_rc=0,
        args=(
            "-O2",
            "--verify",
            "--dump-ir-opt",
            "-S",
            "tests/integration/ir/ir_verify_phi_freeze_test.baa",
            "-o",
            "{output}",
        ),
        output_name="ir_opt_phi.s",
    ),
    ReproCase(
        name="diag-deref",
        channel="stderr",
        expected_rc=1,
        args=(
            "-O1",
            "-c",
            "tests/neg/semantic_deref_non_pointer.baa",
            "-o",
            "{output}",
        ),
        output_name="diag_deref.o",
    ),
    ReproCase(
        name="diag-include-cycle",
        channel="stderr",
        expected_rc=1,
        args=(
            "-O1",
            "-c",
            "tests/neg/syntax_include_cycle_relative_alias.baa",
            "-o",
            "{output}",
        ),
        output_name="diag_include_cycle.o",
    ),
    ReproCase(
        name="asm-sysv",
        channel="file",
        expected_rc=0,
        args=(
            "-O2",
            "-S",
            "--target=x86_64-linux",
            "tests/integration/ir/ir_sysv_varargs_al_zero_asm_test.baa",
            "-o",
            "{output}",
        ),
        output_name="asm_sysv.s",
    ),
    ReproCase(
        name="asm-windows",
        channel="file",
        expected_rc=0,
        args=(
            "-O2",
            "-S",
            "--target=x86_64-windows",
            "tests/integration/ir/ir_windows_call_abi_4args_asm_test.baa",
            "-o",
            "{output}",
        ),
        output_name="asm_windows.s",
    ),
)


def _default_preset() -> str:
    return "windows-release" if os.name == "nt" else "linux-release"


def _load_presets() -> dict[str, object]:
    return json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))


def _resolve_binary_dir(preset_name: str) -> Path:
    presets = _load_presets()
    for preset in presets.get("configurePresets", []):
        if preset.get("name") != preset_name:
            continue
        binary_dir = str(preset.get("binaryDir", ""))
        if not binary_dir:
            break
        binary_dir = binary_dir.replace("${sourceDir}", str(ROOT))
        return Path(binary_dir)
    raise KeyError(f"configure preset not found: {preset_name}")


def _find_compiler(preset_name: str) -> Path:
    build_dir = _resolve_binary_dir(preset_name)
    names = ["baa.exe", "baa"] if os.name == "nt" else ["baa", "baa.exe"]
    for name in names:
        candidate = build_dir / name
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"compiler not found under preset build dir: {build_dir}")


def _normalize_text(text: str, run_root: Path) -> str:
    normalized = text.replace("\r\n", "\n")
    normalized = normalized.replace(str(ROOT), "<ROOT>")
    normalized = normalized.replace(str(run_root), "<RUN>")
    normalized = normalized.replace("\\", "/")
    return normalized


def _run_case(compiler: Path, case: ReproCase, run_root: Path) -> tuple[int, str]:
    output_path = run_root / case.output_name
    args = [arg.format(output=str(output_path)) for arg in case.args]
    proc = subprocess.run(
        [str(compiler), *args],
        cwd=str(ROOT),
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
    )

    if proc.returncode != case.expected_rc:
        raise RuntimeError(
            f"{case.name}: expected rc={case.expected_rc}, got rc={proc.returncode}\n"
            f"STDOUT:\n{proc.stdout or ''}\nSTDERR:\n{proc.stderr or ''}"
        )

    if case.channel == "stdout":
        payload = proc.stdout or ""
    elif case.channel == "stderr":
        payload = proc.stderr or ""
    elif case.channel == "file":
        payload = output_path.read_text(encoding="utf-8", errors="replace")
    else:
        raise RuntimeError(f"unsupported comparison channel: {case.channel}")

    return proc.returncode, _normalize_text(payload, run_root)


def _diff_preview(left: str, right: str) -> str:
    lines = list(
        difflib.unified_diff(
            left.splitlines(),
            right.splitlines(),
            fromfile="run-a",
            tofile="run-b",
            lineterm="",
        )
    )
    if not lines:
        return ""
    return "\n".join(lines[:80])


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check deterministic compiler outputs for a fixed corpus."
    )
    parser.add_argument("--preset", default=_default_preset())
    parser.add_argument("--json-out", default="")
    args = parser.parse_args()

    summary: dict[str, object] = {
        "preset": args.preset,
        "compiler": "",
        "passed": False,
        "cases": [],
    }

    try:
        compiler = _find_compiler(args.preset)
        summary["compiler"] = str(compiler)

        with tempfile.TemporaryDirectory(prefix="baa_repro_a_") as temp_a, tempfile.TemporaryDirectory(
            prefix="baa_repro_b_"
        ) as temp_b:
            run_a = Path(temp_a)
            run_b = Path(temp_b)

            all_ok = True
            for case in CASES:
                rc_a, payload_a = _run_case(compiler, case, run_a)
                rc_b, payload_b = _run_case(compiler, case, run_b)
                matched = (rc_a == rc_b) and (payload_a == payload_b)
                case_summary = {
                    "name": case.name,
                    "channel": case.channel,
                    "expected_rc": case.expected_rc,
                    "matched": matched,
                    "diff_preview": "" if matched else _diff_preview(payload_a, payload_b),
                }
                summary["cases"].append(case_summary)
                all_ok = all_ok and matched
                if matched:
                    print(f"reproducibility: PASS {case.name}")
                else:
                    print(f"reproducibility: FAIL {case.name}", file=sys.stderr)
                    if case_summary["diff_preview"]:
                        print(case_summary["diff_preview"], file=sys.stderr)

            summary["passed"] = all_ok
            if all_ok:
                print(f"reproducibility: PASS (preset={args.preset}, cases={len(CASES)})")
    except Exception as exc:
        summary["error"] = str(exc)
        print(f"reproducibility: FAIL ({exc})", file=sys.stderr)

    if args.json_out:
        out_path = Path(args.json_out)
        if not out_path.is_absolute():
            out_path = ROOT / out_path
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    return 0 if summary.get("passed") else 1


if __name__ == "__main__":
    raise SystemExit(main())
