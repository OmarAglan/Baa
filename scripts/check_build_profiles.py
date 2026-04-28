#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PRESETS = ROOT / "CMakePresets.json"

REQUIRED_CONFIGURE = {
    "windows-dev",
    "windows-debug",
    "windows-release",
    "windows-verify",
    "linux-dev",
    "linux-debug",
    "linux-release",
    "linux-verify",
}

REQUIRED_BUILD = REQUIRED_CONFIGURE


def _collect_cache(presets: dict[str, dict], name: str, seen: set[str] | None = None) -> dict:
    if seen is None:
        seen = set()
    if name in seen:
        raise ValueError(f"cyclic preset inheritance: {name}")
    seen.add(name)

    preset = presets[name]
    cache: dict = {}
    inherits = preset.get("inherits")
    if isinstance(inherits, str):
        cache.update(_collect_cache(presets, inherits, seen))
    elif isinstance(inherits, list):
        for parent in inherits:
            cache.update(_collect_cache(presets, parent, seen))

    own = preset.get("cacheVariables", {})
    if isinstance(own, dict):
        cache.update(own)
    return cache


def main() -> int:
    if not PRESETS.exists():
        print("build-profiles: missing CMakePresets.json")
        return 1

    data = json.loads(PRESETS.read_text(encoding="utf-8"))
    configure = {p.get("name"): p for p in data.get("configurePresets", []) if p.get("name")}
    build = {p.get("name"): p for p in data.get("buildPresets", []) if p.get("name")}

    missing_configure = sorted(REQUIRED_CONFIGURE - set(configure))
    missing_build = sorted(REQUIRED_BUILD - set(build))
    if missing_configure or missing_build:
        print(f"build-profiles: missing configure={missing_configure} build={missing_build}")
        return 1

    for name in sorted(REQUIRED_CONFIGURE):
        cache = _collect_cache(configure, name)
        if cache.get("BAA_ENABLE_WARNINGS") != "ON":
            print(f"build-profiles: {name} must enable BAA_ENABLE_WARNINGS")
            return 1
        if name.endswith("-verify") and cache.get("BAA_WARNINGS_AS_ERRORS") != "ON":
            print(f"build-profiles: {name} must enable BAA_WARNINGS_AS_ERRORS")
            return 1
        if name.endswith("-release") or name.endswith("-verify"):
            if not cache.get("BAA_REPRODUCIBLE_BUILD_DATE"):
                print(f"build-profiles: {name} must set BAA_REPRODUCIBLE_BUILD_DATE")
                return 1

    print("build-profiles: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
