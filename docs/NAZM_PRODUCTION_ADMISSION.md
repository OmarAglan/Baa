# Baa/Nazm Production Admission and Rollback

> **Version:** 1.0
> **Receipt:** `baa-nazm-production-admission-v1`
> **Updated:** 2026-07-19
> **Decision:** APPROVED — Nazm is the production default; GAS is the explicit rollback.

This document is the decision record for moving Nazm into Baa's normal
assembler position. The exact candidate below passed every gate and received
the three required owner approvals. The default-selector change is part of the
approved Baa revision rather than a later unverified commit.

## 1. Candidate Revision Set

| Component | Exact revision | Role |
|---|---|---|
| Baa | `661edd9b05ecdda7fca75905263dc7dfa365693b` | C reference compiler, Nazm-default driver, canonical Arabic emitter, runtime, direct long-Unicode artifact pipeline, PIC/PIE parity gate |
| Nazm | `7236491528567832d25eb0908cec9eac39831779` | Arabic parser, encoder, long-Unicode file I/O, ELF64/COFF object writers |
| Takween | `da8378e097ef0f98acd19bfc76da7acf445547af` | Nazm-default ecosystem build/run/test consumer pinned to the exact Baa/Nazm candidates |

Any behavior change in the emitter, assembler, object writer, startup bridge,
link path, manifest, or parity tests creates a new candidate revision set.
Receipts from an older set cannot approve a newer one. A later documentation-
only receipt commit may still name the last behavior commit explicitly.

## 2. Current Decision

Nazm occupies the ordinary C-like assembler slot by default:

```text
Baa source
  -> Machine IR
  -> canonical Arabic Nazm source
  -> Nazm
  -> ELF64 or COFF object
  -> ordinary host linker
  -> executable
```

The default resolves Nazm from `--nazm-path`, `BAA_NAZM`, then the Arabic
`نظم` command on `PATH`. `--assembler=nazm` may still make that choice explicit.
`--assembler=gas` remains the explicit measured rollback and no Nazm failure
silently activates it.

For comparison work, `--nazm-shadow=<path>` selects an explicit GAS production
leg when no assembler selector was supplied, then builds the Nazm shadow.
Combining an explicit Nazm selector with shadow mode is rejected instead of
pretending to compare Nazm with itself.

The automated technical gate is green for the exact candidate set and the Baa,
Nazm, and Takween owner decisions are recorded below. The decision is
**APPROVED**.

## 3. Parity Surface

The admission corpus contains 100 assembly-producing Baa sources for each of
`x86_64-windows` and `x86_64-linux`. The checked contracts are:

- `baa-assembly-surface-v1`;
- `nazm-capabilities-v1`;
- `baa-nazm-coverage-v1`;
- `baa-nazm-shadow-corpus-v1`; and
- `baa-nazm-source-map-v1`.

Parity means:

1. generated `.نظم` contains no Latin source identifiers;
2. the public entry remains `الرئيسية`, with hosted entry
   `الرئيسية_بدء`;
3. normalized loaded sections and public symbols are compatible;
4. focused fixtures pin required ELF64 and COFF relocation kinds;
5. real host linkers accept the objects;
6. runnable GAS, shadow-Nazm, and selected-Nazm programs have identical exit
   status, stdout, and stderr;
7. assembler failures map back to Baa source locations;
8. unsupported forms and missing tools remain visible non-zero failures;
9. no failed Nazm invocation retries through GAS; and
10. repeated canonical source, Nazm object, and build manifest output is
    byte-stable.

Raw object bytes are not required to match GAS because both assemblers may make
different valid choices for local relocation resolution. Nazm output must be
deterministic against itself.

## 4. Candidate Receipts

| Gate | Result | Evidence |
|---|---:|---|
| Nazm exact-revision repository CI | PASS | Run [`29685356936`](https://github.com/OmarAglan/Nazm/actions/runs/29685356936): strict Release build, full CTest/direct path, Arabic ELF link/run, and long-Unicode Windows I/O |
| Baa exact-revision repository CI | PASS | Run [`29685512987`](https://github.com/OmarAglan/Baa/actions/runs/29685512987): all eight Windows/Linux build, quick/full, and normal/shadow jobs |
| Baa 100-source normal/shadow/runtime parity | PASS | `nazm-shadow-windows` and `nazm-shadow-linux` in run `29685512987`; every selected-Nazm runnable matches GAS |
| Direct Unicode artifact pipeline | PASS | Run `29685512987` covers Arabic paths, spaces, a Windows path beyond 260 UTF-16 units, multi-file and concurrent builds, phase timings, determinism, direct default-Nazm objects, and the explicit GAS rollback without `baa_stage` or artifact copies |
| Linux PIC/PIE producer contract | PASS | Run `29685512987` compiles global/string/runtime-call objects through default Nazm under `-fPIC` and `-fPIE`, compares normalized sections/symbols/relocation presence with GAS, links an `ET_DYN` executable, and matches runtime behavior |
| Hosted quick | PASS | 27/27 on Windows and 27/27 on Linux in run [`29687846586`](https://github.com/OmarAglan/Baa/actions/runs/29687846586) |
| Hosted full | PASS | 44/44 on Windows and 44/44 on Linux in run `29687846586` |
| Hosted stress | PASS | 74/74 on Windows and 74/74 on Linux in run `29687846586` |
| Hosted release + determinism | PASS | 75/75 on Windows and 75/75 on Linux in run `29687846586` |
| Exact revision artifacts | PASS | `baa-nazm-admission-revisions-v1` records Baa `661edd9...` and Nazm `7236491...` for both hosts; all eight QA summaries report zero failures |
| Explicit GAS rollback drill | PASS | Exact Baa candidate returns `4` and creates no object for a missing selected Nazm; a separate `--assembler=gas` invocation succeeds and records `assembler: gas` for the build and unit |
| Takween ecosystem smoke | PASS | Run [`29689709002`](https://github.com/OmarAglan/Takween/actions/runs/29689709002): exact Baa `661edd9...`, Nazm `7236491...`, and Takween `da8378e...` pass build/run/clean/test, mixed `.baa`/`.نظم`, packages, plans, cache, and manifests on Windows/Linux |

The hosted ladder used GitHub-hosted `windows-latest` and `ubuntu-latest`.
Windows configured Baa and Nazm with MinGW Makefiles; Linux used the native
CMake toolchain. Baa used its warnings-as-errors verify preset, Nazm used a
Release build, and each host uploaded its revision receipt, per-mode summaries,
and per-mode logs.

## 5. Known Exclusions

The following work is not silently included in this candidate:

- stack-protector lowering through Nazm;
- GOT/PLT or additional base-index-scale forms not emitted by the current
  checked corpus; producer-required direct-symbol `-fPIC`/`-fPIE` references
  are admitted;
- cross-target executable linking;
- making the in-process `nazm_assemble_buffer()` boundary the production
  default (a later opt-in extension and its admission as the release default
  are recorded in sections 10 and 11);
- changing Baa's reference implementation away from C;
- removing the GAS selector; and
- enabling a public package registry or lifecycle scripts in Takween.

If Baa begins emitting a new assembly form, the inventory must classify it and
the candidate returns to HOLD until both targets have focused acceptance and
host parity.

## 6. Rollback Procedure

Rollback is an explicit build decision, never an automatic reaction to a
failed Nazm process.

1. Switch the project/tool invocation to `--assembler=gas`.
2. Keep assembler identity and exact fingerprint in the build manifest and
   invalidate any artifact whose recorded assembler or fingerprint differs.
3. If the default itself is defective, revert only the default-selector commit;
   do not remove the Arabic emitter, shadow gate, source maps, or Nazm tests.
4. Run the Baa release orchestrator and Takween smoke suite through GAS.
5. Run the Nazm shadow gate separately so the original defect remains
   reproducible and visible.
6. Do not resume the default cutover until replacement Windows/Linux receipts
   are attached to a new exact revision set.

Direct user-authored `.نظم` roots have no GAS translation and therefore cannot
fall back. Their assembly failure remains a source/toolchain failure.

## 7. Default-Cutover Requirements

Every item must be complete:

- [x] Complete two-target 100-source coverage and blocker matrices.
- [x] Arabic-only public symbols and hosted entry ABI.
- [x] Normal selectable Nazm assembler path.
- [x] Windows local object/link/runtime and release gates.
- [x] Hosted Linux normal/shadow object/link/runtime parity for the candidate.
- [x] Deterministic generated source, object, and manifest identity.
- [x] Explicit GAS rollback procedure.
- [x] Green Nazm exact-revision CI.
- [x] Terminal green Baa exact-revision CI receipt.
- [x] Hosted quick/full/stress/release receipts on Windows and Linux.
- [x] No unresolved current-corpus blocker or gate error.
- [x] Baa compiler owner approval.
- [x] Nazm assembler owner approval.
- [x] Takween consumer owner approval.

## 8. Approval Record

| Owner | Decision | Revision/date |
|---|---|---|
| Baa compiler | approved | `661edd9b05ecdda7fca75905263dc7dfa365693b`, 2026-07-19 |
| Nazm assembler | approved | `7236491528567832d25eb0908cec9eac39831779`, 2026-07-19 |
| Takween consumer | approved | `da8378e097ef0f98acd19bfc76da7acf445547af`, 2026-07-19 |

The ecosystem owner approved all three roles against this exact candidate set
after the hosted runs reached terminal success.

## 9. Next Actions

1. Keep the exact-SHA Baa/Nazm admission workflow and Takween cross-platform
   smoke mandatory for emitter, assembler, object-writer, startup, link, or
   default-policy changes.
2. Keep the admitted Nazm API/version/capability fingerprint in every Nazm
   object-cache key.
3. Keep the in-process buffer API behind its build options and Arabic
   invocation selector without changing the inspected textual contract or the
   explicit subprocess/GAS rollback paths.
4. Keep the embedded default of release builds under the gates of section 11.

## 10. Post-Admission API and Cache Extension

The default-cutover receipts and approval table above remain historical and
unchanged. A later guarded extension pins Nazm
`f7fcf8f6d2bf629daf708b3b6028e22c74683ce6` (the stable API plus its Linux
`-Werror=clobbered` portability correction) and adds:

- stable `nazm-api-v1` result ownership, structured diagnostic, OOM-recovery,
  ELF64/COFF option, and object-byte contracts;
- exact CLI/API object-byte equivalence and primary-failure parity tests;
- the complete
  `nazm-api-v1;version=...;capabilities=nazm-capabilities-v1:<sha256>` value in
  Baa manifests and cache slots before any Nazm reuse;
- cache reuse for both compiler-generated and direct `.نظم` units; and
- an opt-in `BAA_ENABLE_EMBEDDED_NAZM` build plus
  `--نظم-داخل-العملية` invocation selector.

This extension did not approve an embedded default; section 11 does. The
separate Nazm process remains the direct operational rollback and GAS remains
the explicit compiler-level rollback. No failure silently switches between
these paths.

## 11. Embedded-Default Admission

Making the linked assembler the default is admitted in two steps, so that the
binaries users already run carry the code before the default changes.

**Step 1: ship it, leave the default alone.** Release installers and packages
are built with `BAA_ENABLE_EMBEDDED_NAZM=ON` from the pinned Nazm checkout and
report `Embedded Nazm <version> (<revision>), opt-in` in `baa --version`. The
default stays the separate Nazm process, and a missing Nazm is still exit 4.
The evidence required before step 2 is:

| Evidence | Where it runs |
|---|---|
| Version, revision and mode identity; selection rules; exit 4 for a missing explicit executable; object-byte parity with the subprocess on a minimal program and on the integration corpus at `-O0`/`-O2` | `tests/test_nazm_api_integration.py`, against an `opt-in` and a `default` build on Windows and Linux in the `nazm-api-v1` CI job |
| Quick, full, stress and release QA against a `default` build with `BAA_NAZM` empty and no `نظم` on `PATH`, so an accidental subprocess selection fails instead of passing | `nazm-production-admission.yml` with `assembler_mode=embedded-default`, both hosts, exact Baa and Nazm SHAs |
| An installed compiler with no Nazm executable reachable compiles, links and runs a program through `--نظم-داخل-العملية`, and reports the pinned Nazm revision | `scripts/test_installer.ps1` and `scripts/test_linux_package.sh` in the clean-machine CI jobs |
| The release candidate ladder still passes on the binary configuration that ships | `release-candidate.yml`, built with `BAA_ENABLE_EMBEDDED_NAZM=ON` |

Step 1 receipts, all on Baa `31a1978` with Nazm `14c6cf4` (2026-10-10):

| Evidence | Result | Receipt |
|---|---|---|
| Integration tests against an `opt-in` and a `default` build, Windows and Linux | 5/5 on each build and host; corpus objects byte-identical to the subprocess at `-O0` and `-O2` | [`38058131936`](https://github.com/OmarAglan/Baa/actions/runs/38058131936), `Nazm API v1` jobs |
| Embedded-default ladder, `BAA_NAZM` empty and no `نظم` on `PATH`, Windows and Linux | quick 34/34, full 52/52, stress 82/82, release 83/83 on both hosts | [`38058138839`](https://github.com/OmarAglan/Baa/actions/runs/38058138839), artifacts `baa-nazm-admission-windows-38058138839` and `baa-nazm-admission-linux-38058138839` |
| Clean-machine installer, `.deb` and `.tar.gz` with no Nazm executable reachable | `Embedded Nazm 0.4.0 (14c6cf4565067e9e32af72c2c1af6b9ef2456744), opt-in`; compile, link and run through `--نظم-داخل-العملية`; plain compile still exit 4 | [`38058131936`](https://github.com/OmarAglan/Baa/actions/runs/38058131936), `Standalone Windows installer` and `Linux packages on a clean Ubuntu` jobs |
| Release-candidate ladder on the shipping configuration | quick 34/34, full 52/52, stress 82/82, release 83/83 on both hosts | [`38058141056`](https://github.com/OmarAglan/Baa/actions/runs/38058141056) |

Step 1 closed with the default unchanged in every shipped binary.

**Step 2: flip the default.** Approved on 2026-10-10 after the step 1
receipts. Release builds set `BAA_EMBEDDED_NAZM_DEFAULT=ON`:
`scripts/build_installer.ps1` and `scripts/package_linux.sh` build the
`default` mode unless told otherwise, and `baa --version` reports
`Embedded Nazm <version> (<revision>), default`. The Windows installer no
longer asks for a separate Nazm. The gates on that configuration are:

| Evidence | Where it runs |
|---|---|
| An installed compiler with no Nazm executable reachable compiles, links and runs programs without any assembler flag; `--nazm-path` still assembles through the separate process, and a missing `--nazm-path` is exit 4 with no output | `scripts/test_installer.ps1 -EmbeddedNazmMode default` and `scripts/test_linux_package.sh --embedded-mode default` in the clean-machine CI jobs |
| Quick, full, stress and release QA on the strict `default` build with `BAA_NAZM` unset and no `نظم` on `PATH`; a guard step fails the run otherwise | `release-candidate.yml` |
| Integration tests and corpus object-byte parity against the subprocess on an `opt-in` and a `default` build | `nazm-api-v1` CI job, both hosts |
| The subprocess default of a build without the option keeps passing its own QA | push CI quick/full jobs and `nazm-production-admission.yml` with `assembler_mode=subprocess` |

Step 2 receipts, all on Baa `f73fa4b` with Nazm `14c6cf4` (2026-10-10):

| Evidence | Result | Receipt |
|---|---|---|
| Clean-machine installer, `.deb` and `.tar.gz` in `default` mode with no Nazm executable reachable | `Embedded Nazm 0.4.0 (14c6cf4565067e9e32af72c2c1af6b9ef2456744), default`; compile, link and run with no assembler flag; `--nazm-path` assembles through the separate process; a missing `--nazm-path` is exit 4 with no output | [`38059638256`](https://github.com/OmarAglan/Baa/actions/runs/38059638256), `Standalone Windows installer` and `Linux packages on a clean Ubuntu` jobs |
| Release-candidate ladder on the strict `default` build, `BAA_NAZM` unset and no `نظم` on `PATH` | quick 34/34, full 52/52, stress 82/82, release 83/83 on both hosts | [`38059645752`](https://github.com/OmarAglan/Baa/actions/runs/38059645752), artifacts `qa-release-windows-38059645752` and `qa-release-linux-38059645752` |
| Integration tests against an `opt-in` and a `default` build, Windows and Linux | 5/5 on each build and host; corpus objects byte-identical to the subprocess at `-O0` and `-O2` | [`38059638256`](https://github.com/OmarAglan/Baa/actions/runs/38059638256), `Nazm API v1` jobs |
| Subprocess path of the same commit, Windows and Linux | push CI quick and full jobs; quick 34/34, full 52/52, stress 82/82, release 83/83 on both hosts with `assembler_mode=subprocess` | [`38059638256`](https://github.com/OmarAglan/Baa/actions/runs/38059638256) and [`38059648700`](https://github.com/OmarAglan/Baa/actions/runs/38059648700) |

Step 2 closed: the embedded Nazm is the default of every release installer
and package.

Selection in a `default` build: `--assembler=gas` and `--nazm-shadow` are
unchanged; `--nazm-path` or a non-empty `BAA_NAZM` selects that executable in
a separate process and a missing one is exit 4, never a fallback; otherwise
the embedded assembler runs and `نظم` on `PATH` is not consulted. Rollback is
therefore one flag or one environment variable per invocation, or
`--assembler=gas`, without rebuilding Baa. The standalone `نظم` command keeps
shipping as its own tool; the embedded API writes no listing.
