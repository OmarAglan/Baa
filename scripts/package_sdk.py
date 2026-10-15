#!/usr/bin/env python3
"""Package a baa-sdk-v1 archive from a built compiler.

The archive is a deterministic gzip-compressed ustar file with one top-level
directory, ``baa-sdk-<version>-<host>/``, holding:

  BAA-SDK.json                 identity, components and the file inventory
  bin/baa[.exe]
  lib/baa/libbaa_runtime.a
  share/baa/{stdlib,targets,docs,resources,examples,README.md,CHANGELOG.md}
  gcc/                         Windows only: the bundled linker kit

The compiler finds every part relative to its own ``bin/`` directory, so an
extracted SDK works from any path. See docs/BAA_SDK_V1.md.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "baa-sdk-v1"
METADATA = "BAA-SDK.json"
SHARE_TREES = ("stdlib", "targets", "docs", "resources", "examples")
SHARE_FILES = ("README.md", "CHANGELOG.md")
LINKER_MANIFEST = "BAA-TOOLCHAIN-MANIFEST.txt"
MAX_FILES = 8192
MAX_FILE_BYTES = 256 * 1024 * 1024
MAX_TOTAL_BYTES = 1024 * 1024 * 1024
MAX_PATH_BYTES = 240
# The metadata and Takween's install receipt; no SDK file may take these names.
RESERVED_NAMES = (METADATA, ".تكوين-طقم-v1")
WINDOWS_DEVICE_NAMES = frozenset(
    ["CON", "PRN", "AUX", "NUL"]
    + [f"COM{n}" for n in range(1, 10)]
    + [f"LPT{n}" for n in range(1, 10)]
)


def fail(message: str) -> None:
    print(f"package_sdk: {message}", file=sys.stderr)
    raise SystemExit(1)


def run_json(executable: Path, flag: str) -> dict:
    proc = subprocess.run(
        [str(executable), flag],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=60,
    )
    if proc.returncode != 0:
        fail(f"{executable} {flag} failed: {proc.stderr.strip()}")
    return json.loads(proc.stdout)


def git_revision() -> str | None:
    proc = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    return proc.stdout.strip() if proc.returncode == 0 else None


def copy_tree(source: Path, destination: Path) -> None:
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            fail(f"symbolic links are not allowed in an SDK: {path}")
        if path.is_file():
            target = destination / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)


def read_linker_manifest(gcc_dir: Path) -> dict[str, str]:
    manifest = gcc_dir / LINKER_MANIFEST
    if not manifest.is_file():
        fail(f"the bundled linker has no {LINKER_MANIFEST}: {gcc_dir}")
    values: dict[str, str] = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        key, sep, value = line.partition("=")
        if sep:
            values[key] = value
    if values.get("format") != "baa-portable-toolchain-v1":
        fail(f"{manifest} is not a baa-portable-toolchain-v1 manifest")
    return values


def is_executable(relative: str) -> bool:
    lowered = relative.lower()
    return (
        relative.startswith(("bin/", "gcc/bin/", "gcc/libexec/"))
        or lowered.endswith((".exe", ".dll"))
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def check_sdk_path(relative: str) -> None:
    # BAA-SDK.json writes paths without escapes, and every SDK must extract on
    # both Windows and Linux, so the path rules are the intersection of the two.
    if any(ch in '"\\:<>|?*' or ord(ch) < 0x20 or ord(ch) == 0x7F for ch in relative):
        fail(f"SDK paths may not contain control characters or any of \"\\:<>|?*: {relative!r}")
    if len(relative.encode("utf-8")) > MAX_PATH_BYTES:
        fail(f"SDK path longer than {MAX_PATH_BYTES} bytes: {relative}")
    if relative in RESERVED_NAMES:
        fail(f"SDK path is reserved for the SDK metadata or an installer: {relative}")
    for segment in relative.split("/"):
        if not segment or segment.endswith((".", " ")):
            fail(f"SDK path has an empty segment or one ending in '.' or space: {relative}")
        if segment.split(".")[0].upper() in WINDOWS_DEVICE_NAMES:
            fail(f"SDK path uses a Windows device name: {relative}")


def inventory(stage: Path) -> list[dict]:
    files = []
    for path in stage.rglob("*"):
        if path.is_file():
            relative = path.relative_to(stage).as_posix()
            if relative == METADATA:
                continue
            check_sdk_path(relative)
            files.append(
                {
                    "path": relative,
                    "size": path.stat().st_size,
                    "sha256": sha256_file(path),
                    "executable": is_executable(relative),
                }
            )
    files.sort(key=lambda entry: entry["path"].encode("utf-8"))
    if len(files) + 1 > MAX_FILES:
        fail(f"{len(files) + 1} files exceed the baa-sdk-v1 limit of {MAX_FILES}")
    total = 0
    for entry in files:
        if entry["size"] > MAX_FILE_BYTES:
            fail(f"{entry['path']} exceeds the per-file limit")
        total += entry["size"]
    if total > MAX_TOTAL_BYTES:
        fail("the SDK exceeds the total size limit")
    return files


def canonical_metadata(metadata: dict) -> str:
    """BAA-SDK.json in canonical form: indented, one inventory entry per line."""
    head = dict(metadata)
    files = head.pop("files")
    text = json.dumps(head, ensure_ascii=False, indent=2)
    entries = [
        "    " + json.dumps(
            {
                "path": entry["path"],
                "size": entry["size"],
                "sha256": entry["sha256"],
                "executable": entry["executable"],
            },
            ensure_ascii=False,
        )
        for entry in files
    ]
    return text[:-2] + ',\n  "files": [\n' + ",\n".join(entries) + "\n  ]\n}\n"


def write_archive(stage: Path, top: str, archive: Path, files: list[dict]) -> None:
    with archive.open("wb") as out, gzip.GzipFile(
        filename="", mode="wb", fileobj=out, compresslevel=9, mtime=0
    ) as gz, tarfile.open(
        fileobj=gz, mode="w", format=tarfile.USTAR_FORMAT, encoding="utf-8"
    ) as tar:
        # The metadata leads so an installer reads the inventory first; the
        # files follow in inventory order.
        entries = [{"path": METADATA, "executable": False}] + files
        for entry in entries:
            source = stage / entry["path"]
            info = tarfile.TarInfo(f"{top}/{entry['path']}")
            info.size = source.stat().st_size
            info.mode = 0o755 if entry["executable"] else 0o644
            info.mtime = 0
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            info.type = tarfile.REGTYPE
            try:
                with source.open("rb") as handle:
                    tar.addfile(info, handle)
            except ValueError as error:
                fail(f"{entry['path']} cannot be stored in ustar: {error}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--baa", required=True, type=Path, help="built baa executable")
    parser.add_argument("--runtime", type=Path, help="libbaa_runtime.a (default: beside baa)")
    parser.add_argument("--linker-dir", type=Path,
                        help="Windows: portable GCC kit with BAA-TOOLCHAIN-MANIFEST.txt")
    parser.add_argument("--no-bundled-linker", action="store_true",
                        help="Windows: package without a linker (tests only; uses gcc on PATH)")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--layout-only", action="store_true",
                        help="stage the SDK directory only; do not write an archive")
    args = parser.parse_args()

    baa = args.baa.resolve()
    if not baa.is_file():
        fail(f"no compiler at {baa}")
    runtime = (args.runtime or baa.parent / "libbaa_runtime.a").resolve()
    if not runtime.is_file():
        fail(f"no runtime library at {runtime}")

    sdk_info = run_json(baa, "--sdk-info=json")
    version = sdk_info["compiler_version"]
    host = sdk_info["host_target"]
    windows = host.endswith("-windows")
    if windows and not args.linker_dir and not args.no_bundled_linker:
        fail("a Windows SDK bundles its linker; pass --linker-dir (or --no-bundled-linker for tests)")
    if not windows and args.linker_dir:
        fail("only Windows SDKs bundle a linker")

    top = f"baa-sdk-{version}-{host}"
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="baa-sdk-") as temp:
        stage = Path(temp) / top
        exe_name = "baa.exe" if windows else "baa"
        (stage / "bin").mkdir(parents=True)
        shutil.copyfile(baa, stage / "bin" / exe_name)
        (stage / "lib" / "baa").mkdir(parents=True)
        shutil.copyfile(runtime, stage / "lib" / "baa" / "libbaa_runtime.a")
        share = stage / "share" / "baa"
        for name in SHARE_TREES:
            copy_tree(ROOT / name, share / name)
        for name in SHARE_FILES:
            shutil.copyfile(ROOT / name, share / name)

        linker: dict = {"kind": "gcc", "bundled": False, "path": "gcc", "version": None}
        if args.linker_dir:
            gcc_dir = args.linker_dir.resolve()
            values = read_linker_manifest(gcc_dir)
            copy_tree(gcc_dir, stage / "gcc")
            linker = {
                "kind": "gcc",
                "bundled": True,
                "path": "gcc/bin/gcc.exe",
                "version": values.get("gcc_version"),
                "manifest": f"gcc/{LINKER_MANIFEST}",
            }

        assembler = sdk_info["assembler"]
        files = inventory(stage)
        metadata = {
            "schema_version": SCHEMA,
            "name": "baa-sdk",
            "version": version,
            "host": host,
            "targets": [host],
            "components": {
                "compiler": {
                    "path": f"bin/{exe_name}",
                    "version": version,
                    "revision": git_revision(),
                },
                "assembler": {
                    "kind": assembler["kind"],
                    "version": assembler["version"],
                    "revision": assembler["revision"],
                },
                "linker": linker,
                "runtime": {"path": "lib/baa/libbaa_runtime.a"},
                "stdlib": {"path": "share/baa/stdlib"},
                "targets": {"path": "share/baa/targets"},
            },
            "files": files,
        }
        text = canonical_metadata(metadata)
        (stage / METADATA).write_bytes(text.encode("utf-8"))

        if args.layout_only:
            destination = output_dir / top
            if destination.exists():
                shutil.rmtree(destination)
            shutil.copytree(stage, destination)
            print(destination)
            return 0

        archive = output_dir / f"{top}.tar.gz"
        write_archive(stage, top, archive, files)
        digest = sha256_file(archive)
        (output_dir / f"{top}.tar.gz.sha256").write_text(
            f"{digest}  {archive.name}\n", encoding="utf-8", newline="\n"
        )
        print(f"{archive} {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
