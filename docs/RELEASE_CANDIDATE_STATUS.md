# Baa Reference Compiler Release Candidate Status

> **Milestone:** v0.6.0 | **Updated:** 2026-10-10

This document records concrete release-candidate gate receipts. A platform is signed off only
after the C reference compiler builds with strict warnings and the quick, full, stress, and
release QA modes pass, and after the artifact a user installs is proven on a clean machine.

## Windows x86-64

**Status:** signed off in GitHub Actions on 2026-10-10.

| Field | Receipt |
|---|---|
| RC implementation commit | `b5f4383` |
| Release-candidate run | [`38045945587`](https://github.com/OmarAglan/Baa/actions/runs/38045945587) |
| CI host | `windows-latest` x86-64 |
| C toolchain | MSYS2 UCRT64 GCC 15.2.0 |
| Python | 3.11 |
| Configure preset | `windows-verify` |
| Warning policy | `BAA_WARNINGS_AS_ERRORS=ON` |
| Assembler | Nazm `4238099`, built in the same job |
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
| `quick` | PASS | 34/34 |
| `full` | PASS | 52/52 |
| `stress` | PASS | 82/82 |
| `release` | PASS | 83/83 |

The run retains every mode summary and hidden QA log as the `qa-release-windows-38045945587`
artifact.

## Linux x86-64

**Status:** signed off in GitHub Actions on 2026-10-10.

| Field | Receipt |
|---|---|
| RC implementation commit | `b5f4383` |
| Release-candidate run | [`38045945587`](https://github.com/OmarAglan/Baa/actions/runs/38045945587) |
| CI host | `ubuntu-latest` x86-64 |
| C toolchain | GCC 13.3.0 |
| Python | 3.11 |
| Configure preset | `linux-verify` |
| Warning policy | `BAA_WARNINGS_AS_ERRORS=ON` |
| Assembler | Nazm `4238099`, built in the same job |
| Reference implementation | C-only root CMake target with `updater_stub.c` |

QA receipts:

| Mode | Result | Steps |
|---|---:|---:|
| `quick` | PASS | 34/34 |
| `full` | PASS | 52/52 |
| `stress` | PASS | 82/82 |
| `release` | PASS | 83/83 |

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

Both platform receipts include the determinism gate: version/build-date stability, negative
diagnostics, raw/optimized IR, assembly, both cross-target assembly outputs, manifest byte
stability and shape, verifier behavior, and IR snapshots.

## Clean-Machine Artifacts

**Status:** signed off in GitHub Actions on 2026-10-10.

The QA ladder proves the compiler in its build tree. These gates prove what a user installs, on
a machine that has never had Baa. Both run on every push in the `Baa CI` workflow; the receipt
below is the run for the RC implementation commit.

| Artifact | Clean machine | Contract | Receipt |
|---|---|---|---|
| `baa-setup-0.6.0-x64.exe` | fresh `windows-latest` runner, per-user install | `scripts/test_installer.ps1` | [`38045707261`](https://github.com/OmarAglan/Baa/actions/runs/38045707261) |
| `baa-0.6.0-Linux-x86_64.deb` | fresh `ubuntu:24.04` container with no C toolchain | `scripts/test_linux_package.sh` | [`38045707261`](https://github.com/OmarAglan/Baa/actions/runs/38045707261) |
| `baa-0.6.0-Linux-x86_64.tar.gz` | same container, unpacked under an Arabic path | `scripts/test_linux_package.sh` | [`38045707261`](https://github.com/OmarAglan/Baa/actions/runs/38045707261) |

Each contract checks the published digest, installs, reports a missing Nazm with exit code 4
instead of working around it, compiles and runs a plain program and a standard-library program
through PATH Nazm, exercises the explicit `--assembler=gas` rollback, then removes the package
and confirms nothing is left behind. No contract sets `BAA_HOME` or `BAA_STDLIB` on Linux, so a
pass also proves the installed compiler finds its own standard library.

The Linux gate found two defects that the build-tree ladder could not see, both fixed in
`b5f4383`: an installed compiler could not find `stdlib/` without `BAA_HOME`, and the `.deb`
did not declare the host linker it needs.

Open finding from the same gate: on Linux, objects assembled by Nazm carry no `.note.GNU-stack`
section, so `ld` warns that an executable stack is implied for programs built through the
default assembler. The GAS rollback path emits the note. Closing this needs either the note from
Nazm or `-z noexecstack` at link time, and it is tracked as the next Linux hardening gate.

## History

- v0.5.9: run [`28384736088`](https://github.com/OmarAglan/Baa/actions/runs/28384736088) on
  `fef76ca` (2026-06-29) — quick 7/7, full 22/22, stress 52/52, release 53/53 on both hosts.
- v0.6.0, before the packaging fixes: run
  [`38045139928`](https://github.com/OmarAglan/Baa/actions/runs/38045139928) on `276a594`
  (2026-10-10) — the same 34/52/82/83 ladder, green on both hosts. Superseded because
  `b5f4383` changes compiler sources.

The post-cut admission and rollback rules are defined in
[RELEASE_PROCESS.md](RELEASE_PROCESS.md).

## Receipt Rules

- Record the exact commit, host, compiler, build preset, and QA step counts.
- Do not reuse a receipt after compiler, build, target, diagnostics, manifest, or QA behavior
  changes; rerun the affected platform ladder.
- A failed gate must remain visible and must not be converted into a signoff through skips.
- Release-branch discipline is active because both supported hosts are green.
