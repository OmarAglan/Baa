# Baa Reference Compiler Release Candidate Status

> **Milestone:** v0.5.9 | **Updated:** 2026-06-29

This document records concrete release-candidate gate receipts. A platform is signed off only
after the C reference compiler builds with strict warnings and the quick, full, stress, and
release QA modes pass.

## Windows x86-64

**Status:** revalidation required after the v0.5.9 metadata synchronization.

| Field | Receipt |
|---|---|
| Baseline commit | `9d67e25` (historical pre-version-bump receipt) |
| Host | Windows x86-64 |
| C toolchain | MSYS2 UCRT64 GCC 15.2.0 |
| Build system | CMake 4.3.3 + MinGW Makefiles |
| Python | 3.12.10 |
| Configure preset | `windows-verify` |
| Warning policy | `BAA_WARNINGS_AS_ERRORS=ON` |
| Reference implementation | C/RC-only root CMake target |

Build receipt:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake --preset windows-verify
cmake --build --preset windows-verify --clean-first
```

The clean strict build completed with zero warnings promoted to errors.

QA receipts:

| Mode | Result | Steps |
|---|---:|---:|
| `quick` | PASS | 5/5 |
| `full` | PASS | 19/19 |
| `stress` | PASS | 49/49 |
| `release` | PASS | 50/50 |

The historical baseline was rerun through `--mode release`. Its determinism sub-gate passed
the version/build-date, negative diagnostic, IR, optimized IR, assembly, cross-target assembly,
manifest, verifier, and committed snapshot checks.

This receipt identified the binary as `0.5.6`, so it is invalid as the final v0.5.9 signoff.
After synchronized version metadata lands, Windows must be rebuilt cleanly and the complete
ladder rerun before this section returns to signed-off status.

## Linux x86-64

**Status:** pending.

The manual `Baa Release Candidate` GitHub Actions workflow runs strict Windows and Linux builds,
executes release QA on both, and uploads the JSON/log receipts even when a gate fails.

Local equivalent:

```bash
cmake --preset linux-verify
cmake --build --preset linux-verify --clean-first
python3 scripts/qa_run.py --mode quick
python3 scripts/qa_run.py --mode full
python3 scripts/qa_run.py --mode stress
python3 scripts/qa_run.py --mode release
```

Phase 4.5 cross-platform signoff, the final reproducibility/determinism checkboxes, and the RC
branch cut remain open until the Linux receipt is green. The post-cut admission and rollback
rules are defined in [RELEASE_PROCESS.md](RELEASE_PROCESS.md).

## Receipt Rules

- Record the exact commit, host, compiler, build preset, and QA step counts.
- Do not reuse a receipt after compiler, build, target, diagnostics, manifest, or QA behavior
  changes; rerun the affected platform ladder.
- A failed gate must remain visible and must not be converted into a signoff through skips.
- Release-branch discipline activates only after both supported hosts are green.
