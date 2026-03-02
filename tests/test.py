#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import shutil
from pathlib import Path
import shlex


ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
INTEGRATION_DIR = TESTS_DIR / "integration"


def _run(cmd: list[str], cwd: Path | None = None, stdin_text: str | None = None) -> int:
    kwargs = {}
    if stdin_text is not None:
        kwargs["input"] = stdin_text
        kwargs["text"] = True
    p = subprocess.run(cmd, cwd=str(cwd) if cwd else None, **kwargs)
    return int(p.returncode)


def _flags_markers(src: Path) -> list[str]:
    flags: list[str] = []
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if s.startswith("// FLAGS:"):
                raw = s.split(":", 1)[1].strip()
                if raw:
                    flags.extend(shlex.split(raw))
    except Exception:
        return []
    return flags


def _args_markers(src: Path) -> list[str]:
    args: list[str] = []
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if s.startswith("// ARGS:"):
                raw = s.split(":", 1)[1].strip()
                if raw:
                    args.extend(shlex.split(raw))
    except Exception:
        return []
    return args


def _expect_asm_markers(src: Path) -> list[str]:
    needles: list[str] = []
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if s.startswith("// EXPECT-ASM:"):
                raw = s.split(":", 1)[1].strip()
                if raw:
                    needles.append(raw)
    except Exception:
        return []
    return needles


def _stdin_markers(src: Path) -> str | None:
    lines: list[str] = []
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if s.startswith("// STDIN:"):
                raw = s.split(":", 1)[1]
                lines.append(raw.lstrip())
    except Exception:
        return None
    if not lines:
        return None
    # نضيف '\n' بين الأسطر ونجعل النهاية بآخر '\n' لسهولة اختبار getchar/scanf.
    return "\n".join(lines) + "\n"


def _run_markers(src: Path) -> set[str]:
    markers: set[str] = set()
    try:
        for line in src.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if not s.startswith("// RUN:"):
                continue
            raw = s.split(":", 1)[1].strip()
            if not raw:
                continue
            for part in raw.replace(",", " ").split():
                v = part.strip().lower()
                if v:
                    markers.add(v)
    except Exception:
        return set()
    return markers


def _default_run_markers(src: Path) -> set[str]:
    markers: set[str] = {"expect-pass"}
    try:
        src.relative_to(INTEGRATION_DIR / "backend")
        is_backend_test = True
    except ValueError:
        is_backend_test = False

    if is_backend_test:
        markers.add("runtime")
    else:
        markers.add("compile-only")
    return markers


def _safe_name(s: str) -> str:
    out = []
    for ch in s:
        if ch.isalnum() or ch in ("_", "-", "."):
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def _resolve_run_markers(src: Path) -> set[str]:
    markers = _run_markers(src)
    if not markers:
        markers = _default_run_markers(src)

    if "runtime" in markers and "compile-only" in markers:
        markers.remove("compile-only")
    if "expect-fail" in markers:
        markers.discard("expect-pass")
    if "expect-pass" not in markers and "expect-fail" not in markers:
        markers.add("expect-pass")
    return markers


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    if os.name == "nt":
        candidates = [
            ROOT / "build" / "baa.exe",
            ROOT / "build" / "baa",
        ]
    else:
        candidates = [
            ROOT / "build-linux" / "baa",
            ROOT / "build" / "baa",
        ]

    for c in candidates:
        if c.exists():
            return c

    raise FileNotFoundError(
        "Could not find compiler binary. Build it first (build-linux/baa or build/baa.exe), "
        "or set env var BAA to its path."
    )


def main() -> int:
    baa_real = _find_baa()
    baa_files = sorted(INTEGRATION_DIR.rglob("*.baa"))
    if not baa_files:
        print(f"No .baa tests found under {INTEGRATION_DIR}.")
        return 1

    failures: list[str] = []

    exe_ext = ".exe" if os.name == "nt" else ""
    out_dir = Path(f".baa_test_out_{os.getpid()}")
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Avoid "Text file busy" if a rebuild replaces the compiler while tests run.
    baa = baa_real
    try:
        if os.name == "nt":
            baa_copy = out_dir / "baa_test_copy.exe"
        else:
            baa_copy = out_dir / (baa_real.name + ".copy")
        shutil.copy2(baa_real, baa_copy)
        if os.name != "nt":
            st = baa_copy.stat()
            baa_copy.chmod(st.st_mode | 0o111)
        baa = baa_copy
    except Exception:
        baa = baa_real

    try:
        for src in baa_files:
            run_markers = _resolve_run_markers(src)
            if "skip" in run_markers:
                continue

            # IMPORTANT: use repo-relative paths to avoid space-splitting bugs
            # in toolchain command strings when the repo root contains spaces.
            src_rel = src.relative_to(ROOT)
            out_stem = _safe_name(str(src.relative_to(INTEGRATION_DIR).with_suffix("")))
            flags = _flags_markers(src)
            expect_asm = _expect_asm_markers(src)

            # إذا كان لدينا EXPECT-ASM نُحوّل الاختبار إلى compile-only على ملف assembly.
            if expect_asm:
                out = out_dir / f"{out_stem}.s"
                if "-S" not in flags and "-s" not in flags:
                    flags = ["-S", *flags]
            else:
                out = out_dir / f"{out_stem}{exe_ext}"

            # -O2 is required because --verify runs SSA verification (Mem2Reg) via the optimizer.
            rc = _run([str(baa), "-O2", "--verify", *flags, str(src_rel), "-o", str(out)], cwd=ROOT)
            if "expect-fail" in run_markers:
                if rc == 0:
                    failures.append(f"compile should fail but passed: {src.name}")
                continue
            if rc != 0:
                failures.append(f"compile/verify failed: {src.name}")
                continue

            if expect_asm:
                try:
                    asm_text = out.read_text(encoding="utf-8", errors="replace")
                except Exception:
                    failures.append(f"could not read asm output: {src.name}")
                    continue
                missing = [n for n in expect_asm if n not in asm_text]
                if missing:
                    failures.append(f"asm missing {missing}: {src.name}")
                continue

            if "runtime" not in run_markers:
                continue

            # Runtime tests are expected to return 0 on PASS.
            args = _args_markers(src)
            stdin_text = _stdin_markers(src)
            rc = _run([str(out), *args], cwd=ROOT, stdin_text=stdin_text)
            if rc != 0:
                failures.append(f"run failed (exit={rc}): {src.name}")
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    if failures:
        print("tests: FAIL")
        for f in failures:
            print(f"  - {f}")
        return 1

    print("tests: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
