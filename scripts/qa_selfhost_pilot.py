#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LOG_DIR = ROOT / ".baa_selfhost_logs"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Compatibility wrapper for the mixed C+Baa token-name pilot")
    parser.add_argument("--baa", default="")
    parser.add_argument("--cc", default="")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR))
    parser.add_argument("--summary-json", default="")
    args = parser.parse_args(argv)

    cmd = [
        sys.executable,
        str(ROOT / "scripts" / "qa_mixed_harness.py"),
        "--target",
        "token-names",
        "--log-dir",
        args.log_dir,
    ]
    if args.baa:
        cmd.extend(["--baa", args.baa])
    if args.cc:
        cmd.extend(["--cc", args.cc])
    if args.summary_json:
        cmd.extend(["--summary-json", args.summary_json])

    return subprocess.call(cmd, cwd=str(ROOT))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
