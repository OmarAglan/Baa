# Baa Reference Compiler Release Candidate Status

> **Milestone:** v0.6.0 | **Updated:** 2026-10-10

This document records concrete release-candidate gate receipts. A platform is signed off only
after the C reference compiler builds with strict warnings and the quick, full, stress, and
release QA modes pass, and after the artifact a user installs is proven on a clean machine.

## Windows x86-64

**Status:** signed off in GitHub Actions on 2026-10-10.

| Field | Receipt |
|---|---|
| RC implementation commit | `f73fa4b` |
| Release-candidate run | [`38059645752`](https://github.com/OmarAglan/Baa/actions/runs/38059645752) |
| CI host | `windows-latest` x86-64 |
| C toolchain | MSYS2 UCRT64 GCC 15.2.0 |
| Python | 3.11 |
| Configure preset | `windows-verify` with `BAA_ENABLE_EMBEDDED_NAZM=ON` and `BAA_EMBEDDED_NAZM_DEFAULT=ON` |
| Warning policy | `BAA_WARNINGS_AS_ERRORS=ON` |
| Assembler | Nazm `14c6cf4`, linked into `baa` as the embedded default; `BAA_NAZM` unset and no `نظم` on `PATH`, enforced by a guard step |
| Reference implementation | C/RC-only root CMake target |

Build receipt:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake --preset windows-verify -DBAA_ENABLE_EMBEDDED_NAZM=ON -DBAA_EMBEDDED_NAZM_DEFAULT=ON -DBAA_NAZM_SOURCE_DIR=<Nazm checkout>
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

The run retains every mode summary and hidden QA log as the `qa-release-windows-38059645752`
artifact.

## Linux x86-64

**Status:** signed off in GitHub Actions on 2026-10-10.

| Field | Receipt |
|---|---|
| RC implementation commit | `f73fa4b` |
| Release-candidate run | [`38059645752`](https://github.com/OmarAglan/Baa/actions/runs/38059645752) |
| CI host | `ubuntu-latest` x86-64 |
| C toolchain | GCC 13.3.0 |
| Python | 3.11 |
| Configure preset | `linux-verify` with `BAA_ENABLE_EMBEDDED_NAZM=ON` and `BAA_EMBEDDED_NAZM_DEFAULT=ON` |
| Warning policy | `BAA_WARNINGS_AS_ERRORS=ON` |
| Assembler | Nazm `14c6cf4`, linked into `baa` as the embedded default; `BAA_NAZM` unset and no `نظم` on `PATH`, enforced by a guard step |
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
cmake --preset linux-verify -DBAA_ENABLE_EMBEDDED_NAZM=ON -DBAA_EMBEDDED_NAZM_DEFAULT=ON -DBAA_NAZM_SOURCE_DIR=<Nazm checkout>
cmake --build --preset linux-verify --clean-first
export BAA="$PWD/build-linux/presets/verify/baa"
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
| `baa-setup-0.6.0-x64.exe` | fresh `windows-latest` runner, per-user install | `scripts/test_installer.ps1` | [`38059638256`](https://github.com/OmarAglan/Baa/actions/runs/38059638256) |
| `baa-0.6.0-Linux-x86_64.deb` | fresh `ubuntu:24.04` container with no C toolchain | `scripts/test_linux_package.sh` | [`38059638256`](https://github.com/OmarAglan/Baa/actions/runs/38059638256) |
| `baa-0.6.0-Linux-x86_64.tar.gz` | same container, unpacked under an Arabic path | `scripts/test_linux_package.sh` | [`38059638256`](https://github.com/OmarAglan/Baa/actions/runs/38059638256) |

Each contract checks the published digest, installs, and requires `baa --version` to report
`Embedded Nazm 0.4.0 (14c6cf4…), default`. With no Nazm executable reachable it compiles, links
and runs a plain program and a standard-library program with no assembler flag, and again
through `--نظم-داخل-العملية`. It then assembles through the separate process with `--nazm-path`,
requires exit code 4 and no output for a `--nazm-path` that does not exist, exercises the
explicit `--assembler=gas` rollback, removes the package and confirms nothing is left behind.
No contract sets `BAA_HOME` or `BAA_STDLIB` on Linux, so a pass also proves the installed
compiler finds its own standard library.

The Linux gate found two defects that the build-tree ladder could not see, both fixed in
`b5f4383`: an installed compiler could not find `stdlib/` without `BAA_HOME`, and the `.deb`
did not declare the host linker it needs.

The same gate found that objects assembled by Nazm `4238099` carried no `.note.GNU-stack`
section, so `ld` warned and implied an executable stack for programs built through the default
assembler. Nazm `14c6cf4` ends every ELF64 object with the empty marker, and the Linux package
contract now fails on that linker warning and requires an `RW` `GNU_STACK` segment for the Nazm
and GAS paths. Both changes are in `388b539`, which every receipt above includes.

## History

- v0.5.9: run [`28384736088`](https://github.com/OmarAglan/Baa/actions/runs/28384736088) on
  `fef76ca` (2026-06-29) — quick 7/7, full 22/22, stress 52/52, release 53/53 on both hosts.
- v0.6.0, before the packaging fixes: run
  [`38045139928`](https://github.com/OmarAglan/Baa/actions/runs/38045139928) on `276a594`
  (2026-10-10) — the same 34/52/82/83 ladder, green on both hosts. Superseded because
  `b5f4383` changes compiler sources.
- v0.6.0, before the non-executable stack fix: run
  [`38045945587`](https://github.com/OmarAglan/Baa/actions/runs/38045945587) on `b5f4383`
  (2026-10-10) with Nazm `4238099` — the same ladder, green on both hosts, and the
  clean-machine contracts green in run
  [`38045707261`](https://github.com/OmarAglan/Baa/actions/runs/38045707261). Superseded because
  `388b539` changes the assembler revision and the Linux package contract.
- v0.6.0, before Nazm was linked into the release binary: run
  [`38049343387`](https://github.com/OmarAglan/Baa/actions/runs/38049343387) on `388b539`
  (2026-10-10) — the same ladder, green on both hosts, and the clean-machine contracts green
  in run [`38049326563`](https://github.com/OmarAglan/Baa/actions/runs/38049326563). Superseded
  because `31a1978` links `nazm-api-v1` into `baa` and changes the driver and both
  clean-machine contracts.
- v0.6.0, with Nazm linked as opt-in: run
  [`38058141056`](https://github.com/OmarAglan/Baa/actions/runs/38058141056) on `31a1978`
  (2026-10-10) — the same ladder, green on both hosts through the separate Nazm process, and
  the clean-machine contracts green in run
  [`38058131936`](https://github.com/OmarAglan/Baa/actions/runs/38058131936). Superseded because
  `f73fa4b` makes the embedded Nazm the default of release builds and changes both clean-machine
  contracts.

The post-cut admission and rollback rules are defined in
[RELEASE_PROCESS.md](RELEASE_PROCESS.md).

## Receipt Rules

- Record the exact commit, host, compiler, build preset, and QA step counts.
- Do not reuse a receipt after compiler, build, target, diagnostics, manifest, or QA behavior
  changes; rerun the affected platform ladder.
- A failed gate must remain visible and must not be converted into a signoff through skips.
- Release-branch discipline is active because both supported hosts are green.
