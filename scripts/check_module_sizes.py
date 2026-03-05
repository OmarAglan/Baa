#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "src"
ALLOWLIST_FILE = ROOT / "scripts" / "module_size_allowlist.txt"
WARN_LINES = 700
ERROR_LINES = 1000
MODULE_EXTS = {".c", ".h"}


@dataclass
class ModuleSize:
    path: str
    lines: int
    status: str


def _load_allowlist(path: Path) -> set[str]:
    if not path.exists():
        return set()

    allowed: set[str] = set()
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        allowed.add(line.replace("\\", "/"))
    return allowed


def _count_lines(path: Path) -> int:
    text = path.read_text(encoding="utf-8", errors="replace")
    if not text:
        return 0
    return len(text.splitlines())


def _collect_modules(
    src_dir: Path, warn_lines: int, error_lines: int, allowlist: set[str]
) -> list[ModuleSize]:
    modules: list[ModuleSize] = []
    for path in sorted(src_dir.iterdir()):
        if not path.is_file():
            continue
        if path.suffix.lower() not in MODULE_EXTS:
            continue

        rel_path = str(path.relative_to(ROOT)).replace("\\", "/")
        lines = _count_lines(path)
        if lines >= error_lines:
            status = "legacy_error" if rel_path in allowlist else "error"
        elif lines >= warn_lines:
            status = "warning"
        else:
            status = "ok"

        modules.append(
            ModuleSize(
                path=rel_path,
                lines=lines,
                status=status,
            )
        )

    modules.sort(key=lambda item: (-item.lines, item.path))
    return modules


def _build_summary(
    modules: list[ModuleSize], warn_lines: int, error_lines: int, allowlist: set[str]
) -> dict[str, object]:
    warnings = [module for module in modules if module.status == "warning"]
    errors = [module for module in modules if module.status == "error"]
    legacy_errors = [module for module in modules if module.status == "legacy_error"]
    return {
        "src_dir": str(SRC_DIR.relative_to(ROOT)).replace("\\", "/"),
        "warn_lines": warn_lines,
        "error_lines": error_lines,
        "file_count": len(modules),
        "warning_count": len(warnings),
        "error_count": len(errors),
        "legacy_error_count": len(legacy_errors),
        "allowlist_count": len(allowlist),
        "allowlist": sorted(allowlist),
        "largest_modules": [asdict(module) for module in modules[:10]],
        "modules": [asdict(module) for module in modules],
        "passed": len(errors) == 0,
    }


def _print_summary(summary: dict[str, object]) -> None:
    warn_lines = int(summary["warn_lines"])
    error_lines = int(summary["error_lines"])
    print(
        f"module-size-guard: scanned {summary['file_count']} files under {summary['src_dir']} "
        f"(warn>={warn_lines}, error>={error_lines})"
    )

    warnings = [item for item in summary["modules"] if item["status"] == "warning"]
    errors = [item for item in summary["modules"] if item["status"] == "error"]
    legacy_errors = [item for item in summary["modules"] if item["status"] == "legacy_error"]

    if warnings:
        print("Warnings:")
        for item in warnings:
            print(f"  WARN  {item['lines']:>5}  {item['path']}")

    if legacy_errors:
        print("Allowed legacy exceptions:")
        for item in legacy_errors:
            print(f"  LEGACY {item['lines']:>4}  {item['path']}")

    if errors:
        print("Errors:")
        for item in errors:
            print(f"  ERROR {item['lines']:>5}  {item['path']}")

    if not warnings and not errors:
        print("No modules exceed the configured thresholds.")

    print(
        "module-size-guard: "
        f"{summary['warning_count']} warning(s), "
        f"{summary['legacy_error_count']} legacy exception(s), "
        f"{summary['error_count']} error(s)"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check handwritten src/*.c and src/*.h module sizes."
    )
    parser.add_argument("--json-out", default="", help="Write the summary JSON to this path.")
    parser.add_argument("--warn-lines", type=int, default=WARN_LINES)
    parser.add_argument("--error-lines", type=int, default=ERROR_LINES)
    args = parser.parse_args()

    if args.warn_lines <= 0 or args.error_lines <= 0:
        print("error: thresholds must be positive integers", file=sys.stderr)
        return 2
    if args.warn_lines >= args.error_lines:
        print("error: --warn-lines must be smaller than --error-lines", file=sys.stderr)
        return 2

    allowlist = _load_allowlist(ALLOWLIST_FILE)
    modules = _collect_modules(SRC_DIR, args.warn_lines, args.error_lines, allowlist)
    summary = _build_summary(modules, args.warn_lines, args.error_lines, allowlist)

    if args.json_out:
        out_path = Path(args.json_out)
        if not out_path.is_absolute():
            out_path = ROOT / out_path
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    _print_summary(summary)
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
