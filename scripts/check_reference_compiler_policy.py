#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

FORBIDDEN_BOOTSTRAP_TOKENS = (
    "BAA_BOOTSTRAP_COMPILER",
    "BAA0_COMPILER",
    "BOOTSTRAP_COMPILER",
)


def _build_entrypoints() -> list[Path]:
    paths = [
        ROOT / "CMakeLists.txt",
        ROOT / "CMakePresets.json",
        ROOT / "scripts" / "package_linux.sh",
        ROOT / "setup.iss",
    ]
    paths.extend(sorted((ROOT / ".github" / "workflows").glob("*.yml")))
    paths.extend(sorted((ROOT / ".github" / "workflows").glob("*.yaml")))
    return [path for path in paths if path.is_file()]


def _relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _check_cmake(errors: list[str]) -> None:
    path = ROOT / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")

    project_match = re.search(r"project\s*\(([^)]*)\)", text, flags=re.IGNORECASE | re.DOTALL)
    if project_match is None:
        errors.append("CMakeLists.txt: missing project() declaration")
    else:
        languages_match = re.search(
            r"\bLANGUAGES\s+(.+)$",
            project_match.group(1).strip(),
            flags=re.IGNORECASE,
        )
        languages = languages_match.group(1).split() if languages_match else []
        if languages != ["C"]:
            errors.append(
                "CMakeLists.txt: project must declare exactly 'LANGUAGES C' "
                f"(found: {languages or 'none'})"
            )

    for match in re.finditer(r"enable_language\s*\(([^)]*)\)", text, flags=re.IGNORECASE):
        languages = match.group(1).split()
        if languages != ["C"]:
            errors.append(
                "CMakeLists.txt: enable_language() may enable only C "
                f"(found: {languages or 'none'})"
            )

    if re.search(
        r"add_executable\s*\(\s*baa\s+\$\{SOURCES\}\s*\)",
        text,
        flags=re.IGNORECASE,
    ) is None:
        errors.append("CMakeLists.txt: baa target must be built from the explicit SOURCES list")

    source_paths = re.findall(r"(?<![\w./-])(src/[^\s\"')]+)", text)
    invalid_sources = sorted(
        source for source in source_paths if Path(source).suffix.lower() not in {".c", ".rc"}
    )
    if invalid_sources:
        errors.append(
            "CMakeLists.txt: the reference compiler target contains non-C inputs: "
            + ", ".join(invalid_sources)
        )

    if re.search(
        r"(?:add_executable|add_library|target_sources)\s*\([^)]*\.baa(?:hd)?\b",
        text,
        flags=re.IGNORECASE | re.DOTALL,
    ):
        errors.append("CMakeLists.txt: a compiler target consumes Baa source")

    if re.search(
        r"\bCOMMAND[^\n]*(?:baa|bootstrap)[^\n]*\.baa(?:hd)?\b",
        text,
        flags=re.IGNORECASE,
    ):
        errors.append("CMakeLists.txt: a custom command bootstraps from Baa source")


def main() -> int:
    errors: list[str] = []
    entrypoints = _build_entrypoints()

    if not entrypoints:
        errors.append("no mainline build entrypoints found")

    for path in entrypoints:
        text = path.read_text(encoding="utf-8")
        for token in FORBIDDEN_BOOTSTRAP_TOKENS:
            if token.casefold() in text.casefold():
                errors.append(f"{_relative(path)}: forbidden bootstrap requirement '{token}'")

    _check_cmake(errors)

    if errors:
        for error in errors:
            print(f"reference-compiler-policy: ERROR: {error}", file=sys.stderr)
        return 1

    checked = ", ".join(_relative(path) for path in entrypoints)
    print(f"reference-compiler-policy: PASS ({checked})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
