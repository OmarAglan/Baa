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
| Baa | `45f69f29c119d8516ec053fda1100f9af7fd2a0a` | C reference compiler, canonical Arabic emitter, driver, runtime |
| Nazm | `235cb3b5ada64db783d8e5a6d9567df34a4e16d4` | Arabic parser, encoder, ELF64/COFF object writers |
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

The decision remains **HOLD** because the Nazm repository's exact-revision CI
is red in its direct non-CMake test path, the Baa Windows full job has not yet
been recorded at terminal state in this receipt, and the hosted
quick/full/stress/release ladder has not yet been signed off for both hosts.

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
| Nazm Windows CMake build + CTest | PASS | 23/23 tests, including CLI, ELF64, COFF, capabilities, and differential encoding |
| Baa Windows release orchestrator | PASS | 75/75 steps |
| Baa 100-source Windows normal/shadow/runtime parity | PASS | `tests/test_nazm_emitter.py`, 25/25 tests |
| Takween Windows ecosystem smoke | PASS | build/run/clean/test, mixed `.baa`/`.نظم`, packages, manifests |
| Baa hosted Linux build/quick/full | PASS | Actions run [`29583330068`](https://github.com/OmarAglan/Baa/actions/runs/29583330068) |
| Baa hosted Linux Nazm shadow + normal corpus | PASS | `nazm-shadow-linux` in run `29583330068` |
| Baa hosted Windows Nazm shadow + normal corpus | PASS | `nazm-shadow-windows` in run `29583330068` |
| Baa hosted Windows full | PENDING RECEIPT | job was still running at the last authenticated metadata read |
| Nazm exact-revision repository CI | FAIL | Actions run [`29583299601`](https://github.com/OmarAglan/Nazm/actions/runs/29583299601), direct test path; its source list omits `src/output/debug_line.c`, reproducing undefined DWARF/CodeView builder references |
| Hosted stress/release on both hosts | MISSING | manual release-candidate receipt not yet recorded for this revision set |

The Nazm failure is not converted into a pass by the green Baa jobs. The
failure remains a production-admission blocker until fixed and rerun.

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
- [ ] Green Nazm exact-revision CI.
- [ ] Terminal green Baa exact-revision CI receipt.
- [ ] Hosted quick/full/stress/release receipts on Windows and Linux.
- [ ] No unresolved current-corpus blocker or gate error.
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

1. Repair and rerun Nazm's direct `build.sh test` CI path without weakening a
   test.
2. Record the terminal result of Baa run `29583330068`.
3. Run the manual quick/full/stress/release workflow on Windows and Linux for
   the exact candidate revisions.
4. Update this receipt with terminal URLs, toolchain provenance, and step
   counts.
5. Review the rollback drill, record owner decisions, and only then consider a
   separate default-selector change.
