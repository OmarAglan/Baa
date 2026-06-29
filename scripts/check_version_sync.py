#!/usr/bin/env python3

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

ENGLISH_VERSION_DOCS = (
    "docs/API_REFERENCE.md",
    "docs/BAA_IR_SPECIFICATION.md",
    "docs/BOOTSTRAP_CONTRACT.md",
    "docs/CONTRACT_FREEZE_V0_5.md",
    "docs/INTERNALS.md",
    "docs/IR_DEVELOPER_GUIDE.md",
    "docs/LANGUAGE.md",
    "docs/STYLE_GUIDE.md",
    "docs/USER_GUIDE.md",
)


def _read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def _require(errors: list[str], relative: str, expected: str) -> None:
    if expected not in _read(relative):
        errors.append(f"{relative}: missing expected version marker: {expected}")


def main() -> int:
    errors: list[str] = []
    cmake = _read("CMakeLists.txt")
    match = re.search(
        r"project\s*\(\s*baa\s+VERSION\s+([0-9]+(?:\.[0-9]+){2,3})\s+LANGUAGES\s+C\s*\)",
        cmake,
        flags=re.IGNORECASE,
    )
    if match is None:
        print("version-sync: ERROR: CMakeLists.txt has no canonical Baa C project version", file=sys.stderr)
        return 1

    version = match.group(1)
    parts = [int(part) for part in version.split(".")]
    while len(parts) < 4:
        parts.append(0)
    resource_version = ",".join(str(part) for part in parts)

    _require(errors, "src/support/version.h", f'#define BAA_VERSION "{version}"')
    _require(errors, "src/baa.h", f"@version {version}")
    _require(errors, "src/baa.rc", f"FILEVERSION     {resource_version}")
    _require(errors, "src/baa.rc", f"PRODUCTVERSION  {resource_version}")
    _require(errors, "src/baa.rc", f'VALUE "FileVersion",      "{version}"')
    _require(errors, "src/baa.rc", f'VALUE "ProductVersion",   "{version}"')
    _require(errors, "setup.iss", f'#define MyAppVersion "{version}"')
    _require(errors, "README.md", f"%D8%A7%D8%B1-{version}-blue.svg")

    for relative in ENGLISH_VERSION_DOCS:
        _require(errors, relative, f"**Version:** {version}")
    _require(errors, "docs/BAA_BOOK_AR.md", f"**الإصدار:** {version}")

    presets = json.loads(_read("CMakePresets.json"))
    dates: dict[str, str] = {}
    for preset in presets.get("configurePresets", []):
        name = str(preset.get("name", ""))
        if name in {"windows-release", "linux-release"}:
            cache = preset.get("cacheVariables", {})
            dates[name] = str(cache.get("BAA_REPRODUCIBLE_BUILD_DATE", ""))
    if set(dates) != {"windows-release", "linux-release"}:
        errors.append("CMakePresets.json: missing Windows/Linux release presets")
    elif not dates["windows-release"] or len(set(dates.values())) != 1:
        errors.append("CMakePresets.json: release presets must share one non-empty build date")

    if errors:
        for error in errors:
            print(f"version-sync: ERROR: {error}", file=sys.stderr)
        return 1

    print(f"version-sync: PASS (version={version}, build_date={next(iter(dates.values()))})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
