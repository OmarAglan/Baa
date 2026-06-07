#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BOOTSTRAP_REF = "0.9.1.4.5"
BOOTSTRAP_BACKEND_PATCH_FILES = (
    "src/backend/emit_support.c",
    "src/backend/emit_inst_body_a.inc",
    "src/backend/emit_inst_body_b.inc",
    "src/backend/emit_inst_body_c.inc",
)


def run(cmd: list[str], cwd: Path = ROOT) -> None:
    print("+ " + " ".join(str(part) for part in cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def ensure_ref(ref: str) -> None:
    verify = subprocess.run(
        ["git", "rev-parse", "--verify", f"{ref}^{{commit}}"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if verify.returncode == 0:
        return

    run(["git", "fetch", "--force", "--depth=1", "origin", f"refs/tags/{ref}:refs/tags/{ref}"])
    subprocess.run(["git", "rev-parse", "--verify", f"{ref}^{{commit}}"], cwd=ROOT, check=True)


def safe_clean_dir(path: Path) -> None:
    resolved_root = ROOT.resolve()
    resolved_path = path.resolve()
    if resolved_path == resolved_root or resolved_root not in resolved_path.parents:
        raise ValueError(f"refusing to clean path outside repository: {resolved_path}")
    if resolved_path.exists():
        shutil.rmtree(resolved_path)
    resolved_path.mkdir(parents=True, exist_ok=True)


def compiler_path(build_dir: Path) -> Path:
    return build_dir / ("baa.exe" if os.name == "nt" else "baa")


def patch_bootstrap_backend(source_dir: Path) -> None:
    for rel in BOOTSTRAP_BACKEND_PATCH_FILES:
        current = ROOT / rel
        target = source_dir / rel
        if not current.exists() or not target.exists():
            raise FileNotFoundError(f"cannot patch bootstrap backend file: {rel}")
        shutil.copy2(current, target)
    print("ci-bootstrap: patched backend emitter legality files")


def main() -> int:
    parser = argparse.ArgumentParser(description="Prepare the Baa bootstrap compiler for CI")
    parser.add_argument("--ref", default=DEFAULT_BOOTSTRAP_REF)
    parser.add_argument("--source-dir", default=".ci/bootstrap-src")
    parser.add_argument("--build-dir", default=".ci/bootstrap-build")
    parser.add_argument("--cmake-generator", default=None)
    parser.add_argument("--build-type", default=None)
    args = parser.parse_args()

    source_dir = (ROOT / args.source_dir).resolve()
    build_dir = (ROOT / args.build_dir).resolve()
    archive_path = build_dir.parent / "bootstrap-src.tar"

    ensure_ref(args.ref)
    safe_clean_dir(source_dir)
    safe_clean_dir(build_dir)

    run(["git", "archive", "--format=tar", "--output", str(archive_path), args.ref])
    with tarfile.open(archive_path, "r") as archive:
        archive.extractall(source_dir)
    archive_path.unlink(missing_ok=True)
    patch_bootstrap_backend(source_dir)

    configure = ["cmake", "-S", str(source_dir), "-B", str(build_dir)]
    if args.cmake_generator:
        configure.extend(["-G", args.cmake_generator])
    if args.build_type:
        configure.append(f"-DCMAKE_BUILD_TYPE={args.build_type}")
    run(configure)
    run(["cmake", "--build", str(build_dir)])

    bootstrap = compiler_path(build_dir)
    if not bootstrap.exists():
        print(f"ci-bootstrap: missing compiler at {bootstrap}", file=sys.stderr)
        return 1

    print(f"ci-bootstrap: compiler={bootstrap}")
    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as output:
            output.write(f"compiler={bootstrap}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
