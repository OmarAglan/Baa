# Baa Reference Compiler Release Candidate Status

> **Milestone:** v0.5.9 | **Updated:** 2026-06-29

This document records concrete release-candidate gate receipts. A platform is signed off only
after the C reference compiler builds with strict warnings and the quick, full, stress, and
release QA modes pass.

## Windows x86-64

**Status:** signed off locally and in GitHub Actions on 2026-06-29.

| Field | Receipt |
|---|---|
| Final RC implementation commit | `fef76ca` |
| GitHub Actions run | [`28384736088`](https://github.com/OmarAglan/Baa/actions/runs/28384736088) |
| CI host | `windows-latest` x86-64 |
| Local verification host | Windows x86-64 |
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
| `quick` | PASS | 7/7 |
| `full` | PASS | 22/22 |
| `stress` | PASS | 52/52 |
| `release` | PASS | 53/53 |

The final CI receipt retained every mode summary, hidden QA log, and detailed determinism
summary. The determinism sub-gate passed 14/14 checks.

The binary reported `baa version 0.5.9` and `Built on 2026-06-29`. The earlier `9d67e25`
receipt remains historical only because that binary still reported 0.5.6.

## Linux x86-64

**Status:** signed off in GitHub Actions on 2026-06-29.

| Field | Receipt |
|---|---|
| Final RC implementation commit | `fef76ca` |
| GitHub Actions run | [`28384736088`](https://github.com/OmarAglan/Baa/actions/runs/28384736088) |
| CI host | `ubuntu-latest` x86-64 |
| Configure preset | `linux-verify` |
| Warning policy | `BAA_WARNINGS_AS_ERRORS=ON` |
| Reference implementation | C-only root CMake target with `updater_stub.c` |

QA receipts:

| Mode | Result | Steps |
|---|---:|---:|
| `quick` | PASS | 7/7 |
| `full` | PASS | 22/22 |
| `stress` | PASS | 52/52 |
| `release` | PASS | 53/53 |

The manual `Baa Release Candidate` GitHub Actions workflow runs strict Windows and Linux builds,
executes quick/full/stress/release QA on both, and uploads every JSON/log receipt even when a
gate fails.

Local equivalent:

```bash
cmake --preset linux-verify
cmake --build --preset linux-verify --clean-first
python3 scripts/qa_run.py --mode quick
python3 scripts/qa_run.py --mode full
python3 scripts/qa_run.py --mode stress
python3 scripts/qa_run.py --mode release
```

The Linux determinism sub-gate passed 14/14 checks. Both platform receipts explicitly include
version/build-date stability, negative diagnostics, raw/optimized IR, assembly, both cross-target
assembly outputs, manifest byte stability and shape, verifier behavior, and IR snapshots.

Phase 4.5 cross-platform signoff is complete. The post-cut admission and rollback rules are
defined in [RELEASE_PROCESS.md](RELEASE_PROCESS.md).

## Receipt Rules

- Record the exact commit, host, compiler, build preset, and QA step counts.
- Do not reuse a receipt after compiler, build, target, diagnostics, manifest, or QA behavior
  changes; rerun the affected platform ladder.
- A failed gate must remain visible and must not be converted into a signoff through skips.
- Release-branch discipline is active because both supported hosts are green.
