# Baa/Nazm Production Admission and Rollback

> **Version:** draft-0.1
> **Receipt:** `baa-nazm-production-admission-v1`
> **Updated:** 2026-07-17
> **Decision:** HOLD — Nazm is selectable, but GAS remains the default.

This document is the decision record for moving Nazm into Baa's normal
assembler position. It does not authorize a default change by itself. A default
change requires every gate below to be green for one exact revision set and
requires explicit owner approval.

## 1. Candidate Revision Set

| Component | Exact revision | Role |
|---|---|---|
| Baa | `a669e7dd3fa80bc266cb4d0b77cb3e92b1738325` | C reference compiler, canonical Arabic emitter, driver, runtime |
| Nazm | `a4013da1f9ce1d98ea1d2dfe36528c8feb8e2374` | Arabic parser, encoder, ELF64/COFF object writers |
| Takween | `623419e909eb78ed8e09a4836bfc0f6f23d97c73` | Ecosystem build/run/test consumer |

Any behavior change in the emitter, assembler, object writer, startup bridge,
link path, manifest, or parity tests creates a new candidate revision set.
Receipts from an older set cannot approve a newer one. A later documentation-
only receipt commit may still name the last behavior commit explicitly.

## 2. Current Decision

Nazm already occupies the ordinary C-like assembler slot when explicitly
selected:

```text
Baa source
  -> Machine IR
  -> canonical Arabic Nazm source
  -> Nazm
  -> ELF64 or COFF object
  -> ordinary host linker
  -> executable
```

The supported selectors are `--assembler=nazm`, `--nazm-path`, `BAA_NAZM`,
and the Arabic `نظم` command on `PATH`. `--assembler=gas` remains the explicit
rollback, and GAS remains the default.

The automated technical gate is green for the exact candidate set. The
decision remains **HOLD** solely because the Baa, Nazm, and Takween owners have
not yet recorded approval against these revisions. GAS remains the default
until all three decisions are explicit.

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
| Nazm exact-revision repository CI | PASS | Run [`29589635435`](https://github.com/OmarAglan/Nazm/actions/runs/29589635435): Release build, 23/23 CTest, 18-group direct path, Arabic ELF link/run |
| Baa exact-revision repository CI | PASS | Run [`29589188470`](https://github.com/OmarAglan/Baa/actions/runs/29589188470): all eight Windows/Linux build, quick/full, and normal/shadow jobs |
| Baa 100-source normal/shadow/runtime parity | PASS | `nazm-shadow-windows` and `nazm-shadow-linux` in run `29589188470`; every selected-Nazm runnable matches GAS |
| Hosted quick | PASS | 27/27 on Windows and 27/27 on Linux in run [`29590118064`](https://github.com/OmarAglan/Baa/actions/runs/29590118064) |
| Hosted full | PASS | 44/44 on Windows and 44/44 on Linux in run `29590118064` |
| Hosted stress | PASS | 74/74 on Windows and 74/74 on Linux in run `29590118064` |
| Hosted release + determinism | PASS | 75/75 on Windows and 75/75 on Linux in run `29590118064` |
| Exact revision artifacts | PASS | `baa-nazm-admission-revisions-v1` records Baa `a669e7d...` and Nazm `a4013da...` for both hosts; all eight QA summaries report zero failures |
| Explicit GAS rollback drill | PASS | Exact Baa candidate returns `4` and creates no object for a missing selected Nazm; a separate `--assembler=gas` invocation succeeds and records `assembler: gas` for the build and unit |
| Takween Windows ecosystem smoke | PASS | Exact candidate binaries pass build/run/clean/test, mixed `.baa`/`.نظم`, packages, manifests |

The hosted ladder used GitHub-hosted `windows-latest` and `ubuntu-latest`.
Windows configured Baa and Nazm with MinGW Makefiles; Linux used the native
CMake toolchain. Baa used its warnings-as-errors verify preset, Nazm used a
Release build, and each host uploaded its revision receipt, per-mode summaries,
and per-mode logs.

## 5. Known Exclusions

The following work is not silently included in this candidate:

- stack-protector lowering through Nazm;
- GOT/PLT or additional base-index-scale PIC/PIE forms not emitted by the
  current checked corpus;
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

### Before a default cutover

1. Use the current default, or pass `--assembler=gas`.
2. Do not pass `--nazm-path` or direct `.نظم` roots when a GAS-only build is
   required.
3. Retain the failing Nazm diagnostics and exit status; start a separate GAS
   build only when the user or build configuration explicitly requests it.

### After a future default cutover

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
- [ ] Baa compiler owner approval.
- [ ] Nazm assembler owner approval.
- [ ] Takween consumer owner approval.

## 8. Approval Record

| Owner | Decision | Revision/date |
|---|---|---|
| Baa compiler | pending | — |
| Nazm assembler | pending | — |
| Takween consumer | pending | — |

Until all three decisions are approved against the same candidate revision set,
GAS remains the production default.

## 9. Next Actions

1. Review the parity and completed rollback receipts against the exact
   candidate set.
2. Record the Baa compiler, Nazm assembler, and Takween consumer decisions.
3. Only if all three owners approve, prepare a separate default-selector
   change. If any owner rejects or defers, keep GAS as the default without
   weakening the selectable Nazm path or its continuous parity gates.
