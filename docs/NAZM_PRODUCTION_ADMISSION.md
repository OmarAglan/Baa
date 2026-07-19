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
| Baa | `9efbcc417a7a67bfb6928921f2257a872c25160a` | C reference compiler, Nazm-default driver, canonical Arabic emitter, runtime, PIC/PIE parity gate |
| Nazm | `7be5799f88bf70da781499dd35ccc4c4eda12e6f` | Arabic parser, encoder, ELF64/COFF object writers |
| Takween | `bc2eccc7b126adbfaa6cb472d61daf4607c6c59a` | Nazm-default ecosystem build/run/test consumer pinned to the exact Baa candidate |

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
| Nazm exact-revision repository CI | PASS | Run [`29637594387`](https://github.com/OmarAglan/Nazm/actions/runs/29637594387): Release build, full CTest/direct path, Arabic ELF link/run |
| Baa exact-revision repository CI | PASS | Run [`29679921655`](https://github.com/OmarAglan/Baa/actions/runs/29679921655): all eight Windows/Linux build, quick/full, and normal/shadow jobs |
| Baa 100-source normal/shadow/runtime parity | PASS | `nazm-shadow-windows` and `nazm-shadow-linux` in run `29679921655`; every selected-Nazm runnable matches GAS |
| Linux PIC/PIE producer contract | PASS | Run `29679921655` compiles global/string/runtime-call objects through default Nazm under `-fPIC` and `-fPIE`, compares normalized sections/symbols/relocation presence with GAS, links an `ET_DYN` executable, and matches runtime behavior |
| Hosted quick | PASS | 27/27 on Windows and 27/27 on Linux in run [`29680127124`](https://github.com/OmarAglan/Baa/actions/runs/29680127124) |
| Hosted full | PASS | 44/44 on Windows and 44/44 on Linux in run `29680127124` |
| Hosted stress | PASS | 74/74 on Windows and 74/74 on Linux in run `29680127124` |
| Hosted release + determinism | PASS | 75/75 on Windows and 75/75 on Linux in run `29680127124` |
| Exact revision artifacts | PASS | `baa-nazm-admission-revisions-v1` records Baa `9efbcc4...` and Nazm `7be5799...` for both hosts; all eight QA summaries report zero failures |
| Explicit GAS rollback drill | PASS | Exact Baa candidate returns `4` and creates no object for a missing selected Nazm; a separate `--assembler=gas` invocation succeeds and records `assembler: gas` for the build and unit |
| Takween ecosystem smoke | PASS | Run [`29681669191`](https://github.com/OmarAglan/Takween/actions/runs/29681669191): exact Baa `9efbcc4...`, Nazm `7be5799...`, and Takween `bc2eccc...` pass build/run/clean/test, mixed `.baa`/`.نظم`, packages, plans, cache, and manifests on Windows/Linux |

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
- the future in-process `nazm_assemble_buffer()` boundary;
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
2. Keep assembler identity in the build manifest and invalidate any artifact
   whose recorded assembler differs. Nazm incremental cache reuse remains
   disabled until its cache-key contract is admitted.
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
| Baa compiler | approved | `9efbcc417a7a67bfb6928921f2257a872c25160a`, 2026-07-19 |
| Nazm assembler | approved | `7be5799f88bf70da781499dd35ccc4c4eda12e6f`, 2026-07-18 |
| Takween consumer | approved | `bc2eccc7b126adbfaa6cb472d61daf4607c6c59a`, 2026-07-19 |

The ecosystem owner approved all three roles against this exact candidate set
after the hosted runs reached terminal success.

## 9. Next Actions

1. Keep the exact-SHA Baa/Nazm admission workflow and Takween cross-platform
   smoke mandatory for emitter, assembler, object-writer, startup, link, or
   default-policy changes.
2. Add a Nazm version fingerprint to cache keys before enabling Nazm object
   reuse.
3. Continue the optional in-process buffer API without changing the inspected
   Arabic textual contract or the explicit GAS rollback.
