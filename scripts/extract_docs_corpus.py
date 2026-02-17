#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


FENCE_START_RE = re.compile(r"^```\s*baa\s*$")
FENCE_END_RE = re.compile(r"^```\s*$")


@dataclass
class Block:
    src_path: str
    index: int
    text: str


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        p = Path(env)
        if p.exists():
            return p

    candidates = [
        ROOT / "build-linux" / "baa",
        ROOT / "build" / "baa.exe",
        ROOT / "build" / "baa",
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(
        "Could not find compiler binary. Build it first (build-linux/baa or build/baa.exe), "
        "or set env var BAA to its path."
    )


def _iter_md_files() -> list[Path]:
    files: list[Path] = []
    if (ROOT / "README.md").exists():
        files.append(ROOT / "README.md")
    docs = ROOT / "docs"
    if docs.exists():
        files.extend(sorted(docs.rglob("*.md")))
    return files


def _git_list_md_files(ref: str) -> list[str]:
    p = subprocess.run(
        ["git", "ls-tree", "-r", "--name-only", ref],
        cwd=str(ROOT),
        text=True,
        capture_output=True,
        check=False,
    )
    if p.returncode != 0:
        return []
    out: list[str] = []
    for line in p.stdout.splitlines():
        s = line.strip()
        if s == "README.md" or (s.startswith("docs/") and s.endswith(".md")):
            out.append(s)
    return out


def _git_show_text(ref: str, path: str) -> str | None:
    p = subprocess.run(
        ["git", "show", f"{ref}:{path}"],
        cwd=str(ROOT),
        text=True,
        capture_output=True,
        check=False,
    )
    if p.returncode != 0:
        return None
    return p.stdout


def _extract_baa_blocks_from_text(text: str) -> list[str]:
    lines = text.splitlines()
    blocks: list[list[str]] = []
    cur: list[str] | None = None

    for line in lines:
        if cur is None:
            if FENCE_START_RE.match(line):
                cur = []
            continue

        if FENCE_END_RE.match(line):
            blocks.append(cur)
            cur = None
            continue

        cur.append(line)

    out: list[str] = []
    for b in blocks:
        text = "\n".join(b).strip() + "\n"
        out.append(text)
    return out


def _extract_baa_blocks(p: Path) -> list[str]:
    return _extract_baa_blocks_from_text(p.read_text(encoding="utf-8", errors="replace"))


def _is_candidate_program(text: str, allow_io: bool) -> bool:
    if "صحيح الرئيسية" not in text:
        return False
    if not allow_io and "اقرأ" in text:
        return False
    # Avoid placeholders / ellipsis
    if "..." in text or "…" in text:
        return False
    return True


def _compile_and_maybe_run(
    baa: Path,
    src_rel: Path,
    exe_path: Path,
    opt: str,
    verify: bool,
    run_it: bool,
    compile_timeout_s: float,
    run_timeout_s: float,
) -> tuple[int, str, str]:
    cmd = [str(baa), f"-{opt}"]
    if verify:
        cmd.append("--verify")
    cmd.extend([str(src_rel), "-o", str(exe_path)])

    try:
        p = subprocess.run(cmd, cwd=str(ROOT), text=True, capture_output=True, timeout=float(compile_timeout_s))
    except subprocess.TimeoutExpired:
        return 124, "", "compile timeout"
    if p.returncode != 0:
        return int(p.returncode), p.stdout, p.stderr

    if run_it:
        try:
            r = subprocess.run([str(exe_path)], cwd=str(ROOT), text=True, capture_output=True, timeout=float(run_timeout_s))
            return int(r.returncode), r.stdout, r.stderr
        except subprocess.TimeoutExpired:
            return 124, "", "run timeout"

    return 0, p.stdout, p.stderr


def _sanitize_ref(ref: str) -> str:
    out = []
    for ch in ref:
        if ch.isalnum() or ch in (".", "_", "-"):
            out.append(ch)
        else:
            out.append("_")
    return "".join(out)


def _git_refs_from_pattern(pat: str) -> list[str]:
    p = subprocess.run(
        ["git", "tag", "-l", pat],
        cwd=str(ROOT),
        text=True,
        capture_output=True,
        check=False,
    )
    if p.returncode != 0:
        return []
    refs = [s.strip() for s in p.stdout.splitlines() if s.strip()]
    # Sort by version-ish segments when possible; fall back to lexicographic.
    return sorted(refs)


def main() -> int:
    ap = argparse.ArgumentParser(description="Extract runnable `.baa` programs from docs fenced blocks.")
    ap.add_argument("--out-dir", default=str(ROOT / "tests" / "corpus_docs"))
    ap.add_argument("--git-ref", action="append", default=[], help="Extract from a git ref (tag/commit). Repeatable.")
    ap.add_argument("--git-ref-pattern", default=None, help="Convenience: extract from all tags matching pattern (e.g. 0.2.*)")
    ap.add_argument("--opt", default="O1", choices=["O0", "O1", "O2"])
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--run", action="store_true", help="Also execute produced binaries")
    ap.add_argument("--allow-io", action="store_true")
    ap.add_argument("--max", type=int, default=20)
    ap.add_argument("--compile-timeout", type=float, default=20.0)
    ap.add_argument("--run-timeout", type=float, default=2.0)
    args = ap.parse_args()

    baa = _find_baa()
    blocks: list[Block] = []
    refs: list[str] = list(args.git_ref)
    if args.git_ref_pattern:
        refs.extend(_git_refs_from_pattern(str(args.git_ref_pattern)))

    if refs:
        for ref in refs:
            for path in _git_list_md_files(ref):
                text = _git_show_text(ref, path)
                if text is None:
                    continue
                bs = _extract_baa_blocks_from_text(text)
                for i, t in enumerate(bs):
                    if _is_candidate_program(t, allow_io=bool(args.allow_io)):
                        blocks.append(Block(src_path=f"{ref}:{path}", index=i, text=t))
    else:
        md_files = _iter_md_files()
        for f in md_files:
            bs = _extract_baa_blocks(f)
            for i, t in enumerate(bs):
                if _is_candidate_program(t, allow_io=bool(args.allow_io)):
                    blocks.append(Block(src_path=str(f.relative_to(ROOT)), index=i, text=t))

    if not blocks:
        print("No candidate programs found in docs.")
        return 1

    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = (ROOT / out_dir).resolve()
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    accepted: list[dict] = []

    with tempfile.TemporaryDirectory(prefix="baa_docs_corpus_") as td:
        tmp_dir = Path(td)
        exe_ext = ".exe" if os.name == "nt" else ""
        for n, blk in enumerate(blocks):
            if len(accepted) >= int(args.max):
                break

            # Use subdir per ref when present.
            subdir = out_dir
            if ":" in blk.src_path:
                ref = blk.src_path.split(":", 1)[0]
                subdir = out_dir / _sanitize_ref(ref)
                subdir.mkdir(parents=True, exist_ok=True)

            name = f"doc_{len(accepted):03d}.baa"
            dst = subdir / name
            dst.write_text(
                "// مصدر: " + blk.src_path + f" (block {blk.index})\n" + blk.text,
                encoding="utf-8",
            )

            exe = tmp_dir / f"doc_{len(accepted):03d}{exe_ext}"
            rc, so, se = _compile_and_maybe_run(
                baa=baa,
                src_rel=dst.relative_to(ROOT),
                exe_path=exe,
                opt=str(args.opt),
                verify=bool(args.verify),
                run_it=bool(args.run),
                compile_timeout_s=float(args.compile_timeout),
                run_timeout_s=float(args.run_timeout),
            )
            if rc != 0:
                dst.unlink(missing_ok=True)
                continue

            accepted.append(
                {
                    "file": str(dst.relative_to(ROOT)),
                    "origin": {"path": blk.src_path, "block": blk.index},
                }
            )

    (out_dir / "MANIFEST.json").write_text(
        json.dumps({"accepted": accepted}, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )

    print(f"accepted: {len(accepted)}")
    print(f"out: {out_dir.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
