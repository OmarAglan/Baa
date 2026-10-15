#!/usr/bin/env python3
"""baa-sdk-v1 and baa-sdk-info-v1 contract tests (docs/BAA_SDK_V1.md)."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PACKAGER = ROOT / "scripts" / "package_sdk.py"


def _find_baa() -> Path:
    env = os.environ.get("BAA")
    if env:
        candidate = Path(env)
        if candidate.is_file():
            return candidate

    candidates = [
        ROOT / "build" / "presets" / "windows-verify" / "baa.exe",
        ROOT / "build" / "baa.exe",
        ROOT / "build-linux" / "presets" / "verify" / "baa",
        ROOT / "build-linux" / "baa",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Could not find compiler binary; set BAA or build first")


def _run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=kwargs.pop("timeout", 120),
        **kwargs,
    )


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _inside(path: str | None, root: Path) -> bool:
    if path is None:
        return False
    try:
        Path(path).resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


class SdkInfoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()
        cls.host_name = "x86_64-windows" if os.name == "nt" else "x86_64-linux"

    def sdk_info(self, baa: Path) -> dict:
        proc = _run([str(baa), "--sdk-info=json"], cwd=str(ROOT))
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stderr, "")
        return json.loads(proc.stdout)

    def test_build_tree_reports_its_own_parts(self) -> None:
        data = self.sdk_info(self.baa)
        self.assertEqual(data["schema_version"], "baa-sdk-info-v1")
        self.assertEqual(data["host_target"], self.host_name)
        self.assertIsNone(data["sdk_root"])

        target_info = json.loads(
            _run([str(self.baa), "--target-info=json"], cwd=str(ROOT)).stdout
        )
        self.assertEqual(data["compiler_version"], target_info["compiler_version"])

        self.assertEqual(
            Path(data["executable_dir"]).resolve(), self.baa.resolve().parent
        )
        # شجرة البناء ليس فيها مكتبة قياسية بجوار المصرّف، فيكون المسار null.
        if data["install_home"] is None:
            self.assertIsNone(data["stdlib_dir"])
            self.assertIsNone(data["targets_dir"])
        else:
            self.assertTrue((Path(data["stdlib_dir"]) / "baalib.baahd").is_file())
            self.assertTrue((Path(data["targets_dir"]) / "x86_64-windows.json").is_file())
        self.assertTrue(Path(data["runtime_library"]).is_file())
        for key in ("executable_dir", "install_home", "stdlib_dir", "targets_dir",
                    "runtime_library"):
            if data[key] is not None:
                self.assertNotIn("\\", data[key], key)

        self.assertIn(data["assembler"]["kind"], {"nazm-embedded", "external"})
        self.assertIsInstance(data["assembler"]["default"], bool)
        if data["assembler"]["kind"] == "external":
            self.assertIsNone(data["assembler"]["version"])
            self.assertFalse(data["assembler"]["default"])
        self.assertEqual(data["linker"]["kind"], "gcc")
        self.assertIsInstance(data["linker"]["bundled"], bool)
        self.assertIsInstance(data["linker"]["path"], str)

    def test_sdk_info_must_be_the_only_argument(self) -> None:
        for extra in (["--verbose"], ["-o", "x.exe"], ["--target-info=json"]):
            proc = _run([str(self.baa), "--sdk-info=json", *extra], cwd=str(ROOT))
            self.assertNotEqual(proc.returncode, 0, extra)
            self.assertIn("must be the only argument", proc.stderr)
            self.assertEqual(proc.stdout, "")

    def test_help_lists_sdk_info(self) -> None:
        proc = _run([str(self.baa), "--help"], cwd=str(ROOT))
        self.assertIn("--sdk-info=json", proc.stdout + proc.stderr)


class SdkPackageTests(unittest.TestCase):
    """Package without a bundled linker; the SDK compiler links with gcc on PATH."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.baa = _find_baa()
        cls.temp = Path(tempfile.mkdtemp(prefix="baa-sdk-test-"))
        cls.archives: list[Path] = []
        for run in ("a", "b"):
            proc = _run(
                [sys.executable, "-I", str(PACKAGER), "--baa", str(cls.baa),
                 "--no-bundled-linker", "--output-dir", str(cls.temp / run)],
                cwd=str(ROOT),
                timeout=300,
            )
            if proc.returncode != 0:
                raise AssertionError(f"package_sdk.py failed:\n{proc.stderr}")
            archive_text, digest = proc.stdout.strip().rsplit(" ", 1)
            archive = Path(archive_text)
            if _sha256(archive) != digest:
                raise AssertionError("package_sdk.py printed the wrong digest")
            cls.archives.append(archive)

    @classmethod
    def tearDownClass(cls) -> None:
        shutil.rmtree(cls.temp, ignore_errors=True)

    def test_archive_is_reproducible(self) -> None:
        first, second = self.archives
        self.assertEqual(first.name, second.name)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        sidecar = first.with_name(first.name + ".sha256").read_text(encoding="utf-8")
        self.assertEqual(sidecar, f"{_sha256(first)}  {first.name}\n")

    def test_archive_entries_and_inventory(self) -> None:
        archive = self.archives[0]
        top = archive.name[: -len(".tar.gz")]
        with tarfile.open(archive, "r:gz") as tar:
            members = tar.getmembers()
            names = [member.name for member in members]
            self.assertEqual(names, sorted(names, key=lambda name: name.encode("utf-8")))
            for member in members:
                self.assertTrue(member.isreg(), member.name)
                self.assertTrue(member.name.startswith(top + "/"), member.name)
                self.assertNotIn("..", member.name.split("/"))
                self.assertEqual(member.mtime, 0)
                self.assertEqual((member.uid, member.gid), (0, 0))
                self.assertIn(member.mode, (0o644, 0o755))

            # المثبّت يقرأ الجرد قبل أي ملف آخر.
            self.assertEqual(names[0], f"{top}/BAA-SDK.json")
            metadata_member = tar.getmember(f"{top}/BAA-SDK.json")
            text = tar.extractfile(metadata_member).read().decode("utf-8")
            metadata = json.loads(text)

            # الصيغة القياسية: سطر واحد لكل ملف في الجرد، ولا هروب في المسارات.
            lines = text.split("\n")
            start = lines.index('  "files": [')
            self.assertEqual(lines[-3:], ["  ]", "}", ""])
            entry_lines = lines[start + 1:-3]
            self.assertEqual(len(entry_lines), len(metadata["files"]))
            pattern = re.compile(
                r'^    \{"path": "[^"\\\x00-\x1f\x7f]+", "size": (0|[1-9][0-9]*), '
                r'"sha256": "[0-9a-f]{64}", "executable": (true|false)\}(,?)$'
            )
            for number, line in enumerate(entry_lines):
                match = pattern.match(line)
                self.assertIsNotNone(match, line)
                self.assertEqual(match.group(3), "" if number == len(entry_lines) - 1 else ",")
            self.assertEqual(metadata["schema_version"], "baa-sdk-v1")
            self.assertEqual(metadata["name"], "baa-sdk")
            self.assertEqual(top, f"baa-sdk-{metadata['version']}-{metadata['host']}")
            self.assertEqual(metadata["targets"], [metadata["host"]])

            components = metadata["components"]
            exe = "bin/baa.exe" if os.name == "nt" else "bin/baa"
            self.assertEqual(components["compiler"]["path"], exe)
            self.assertEqual(components["linker"]["kind"], "gcc")
            self.assertFalse(components["linker"]["bundled"])
            self.assertEqual(components["runtime"]["path"], "lib/baa/libbaa_runtime.a")

            inventory = {entry["path"]: entry for entry in metadata["files"]}
            archived = {name[len(top) + 1:]: member for name, member in zip(names, members)}
            self.assertEqual(set(inventory) | {"BAA-SDK.json"}, set(archived))
            for path in (exe, "lib/baa/libbaa_runtime.a",
                         "share/baa/stdlib/baalib.baahd",
                         "share/baa/targets/x86_64-windows.json"):
                self.assertIn(path, inventory)
            for path, entry in inventory.items():
                data = tar.extractfile(archived[path]).read()
                self.assertEqual(len(data), entry["size"], path)
                self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"], path)
                expected_mode = 0o755 if entry["executable"] else 0o644
                self.assertEqual(archived[path].mode, expected_mode, path)
            self.assertTrue(inventory[exe]["executable"])

    def test_extracted_sdk_finds_its_parts_and_builds(self) -> None:
        archive = self.archives[0]
        top = archive.name[: -len(".tar.gz")]
        destination = self.temp / "مجلد عربي"
        destination.mkdir()
        with tarfile.open(archive, "r:gz") as tar:
            if hasattr(tarfile, "data_filter"):
                tar.extractall(destination, filter="data")
            else:
                tar.extractall(destination)
        sdk = destination / top
        exe = sdk / "bin" / ("baa.exe" if os.name == "nt" else "baa")
        if os.name != "nt":
            exe.chmod(0o755)

        proc = _run([str(exe), "--sdk-info=json"], cwd=str(self.temp))
        self.assertEqual(proc.returncode, 0, proc.stderr)
        info = json.loads(proc.stdout)
        self.assertEqual(Path(info["sdk_root"]).resolve(), sdk.resolve())
        self.assertEqual(Path(info["install_home"]).resolve(),
                         (sdk / "share" / "baa").resolve())
        for key in ("stdlib_dir", "targets_dir", "runtime_library"):
            self.assertTrue(_inside(info[key], sdk), f"{key}={info[key]}")
        self.assertEqual(Path(info["runtime_library"]).resolve(),
                         (sdk / "lib" / "baa" / "libbaa_runtime.a").resolve())

        program = self.temp / "برنامج.baa"
        program.write_text(
            'صحيح الرئيسية() {\n    اطبع "مرحبا من الطقم".\n    إرجع ٠.\n}\n',
            encoding="utf-8",
        )
        output = self.temp / ("برنامج.exe" if os.name == "nt" else "برنامج")
        proc = _run([str(exe), str(program), "-o", str(output)], cwd=str(self.temp))
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        run = subprocess.run([str(output)], capture_output=True, timeout=30)
        self.assertEqual(run.returncode, 0)
        self.assertIn("مرحبا من الطقم", run.stdout.decode("utf-8"))


if __name__ == "__main__":
    unittest.main()
