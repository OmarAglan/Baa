# Baa Ecosystem Boundaries

> **Version:** draft-0.1 | **Applies to:** Baa v0.5.8+

This document defines what belongs in each project in the Baa ecosystem.

---

## 1. Purpose

Baa is becoming an ecosystem, not a single repository. The goal of this document is to prevent scope drift and duplicated work.

The ecosystem projects are:

| Project | Role |
|---|---|
| Baa | Arabic-first systems language and reference compiler |
| Nazm | Arabic-first assembler and ELF64/COFF object writer |
| Takween | Arabic-first build workflow for Baa projects |
| Qalam-IDE | Arabic/RTL IDE and editor experience |
| PyramidOS | Future freestanding/OS-development consumer and testbed |

---

## 2. Baa Ownership

Baa owns:

- language syntax and semantics,
- the C reference compiler,
- lexer, parser, semantic analyzer,
- IR, optimizer, backend, target ABI behavior,
- standard-library contracts,
- runtime-safety flags,
- diagnostics and warning behavior,
- compiler CLI flags,
- machine-readable compiler outputs,
- target specifications,
- conformance tests,
- release gates and QA.

Baa does **not** own:

- project-build UX beyond compiler invocations,
- an IDE/editor UI,
- PyramidOS kernel migration,
- a package registry before the language is stable,
- production self-hosting before a staged post-v0.9 decision.

Baa currently owns its GAS/AT&T text emitter and external assembler invocation.
After the `baa-nazm-boundary-v0` admission gates pass, Nazm owns Arabic assembly
syntax, instruction encoding, relocations, and object serialization; Baa keeps
Machine IR, register allocation, ABI lowering, and prologue/epilogue ownership.

---

## 3. Nazm Ownership

Nazm owns:

- canonical Arabic assembly syntax and terminology,
- parsing and operand validation,
- x86-64 instruction encoding,
- symbol and relocation handling,
- ELF64 and PE/COFF object serialization,
- assembler diagnostics, listings, CLI, and future embedding API.

Baa must support Nazm integration by providing:

- a complete inventory of emitted machine forms for Windows and Linux,
- a canonical Arabic Nazm text emitter after register allocation,
- source mapping from generated assembly back to Baa source,
- a shadow mode that never changes production output silently, and
- parity tests for objects, links, runtime behavior, symbols, and relocations.

Nazm must not be made the default assembler until Baa's quick, full, stress,
determinism, and cross-target gates pass through the Nazm path. Unsupported forms
must fail visibly; integration must not guess bytes or silently fall back to GAS.

---

## 4. Takween Ownership

Takween owns:

- project initialization,
- build/run/clean/test workflows,
- project configuration files,
- local package or local library wiring,
- invoking Baa correctly,
- consuming Baa build manifests,
- presenting build errors to users.

Baa must support Takween by providing:

- stable CLI flags,
- stable exit codes,
- deterministic build manifests,
- reliable include/dependency data,
- machine-readable diagnostics.

Takween must not rely on private Baa internals.

---

## 5. Qalam-IDE Ownership

Qalam-IDE owns:

- editor UI/UX,
- RTL layout,
- syntax highlighting,
- completion UI,
- diagnostics display,
- project/file explorer,
- console integration,
- run/build buttons,
- IDE packaging.

Baa must support Qalam by providing:

- fast check mode,
- diagnostics JSON,
- token dump JSON,
- symbol outline JSON,
- completion metadata,
- stable diagnostic codes.

Qalam-facing roadmap entries in Baa must stay limited to compiler/data contracts: check
modes, machine-readable diagnostics, token streams, symbol metadata, and completion metadata.
Baa does not own editor widgets, IDE commands, extension packaging, themes, layout, or UI flows.

Qalam should not parse unstable human-readable compiler text when a machine-readable contract exists.

---

## 6. PyramidOS Ownership

PyramidOS owns:

- bootloader,
- kernel architecture,
- C/Assembly kernel code,
- linker scripts,
- QEMU boot flow,
- hardware tables,
- drivers,
- VFS/storage/shell/userland roadmap.

Baa may later support PyramidOS through:

- `--freestanding`,
- `--no-stdlib`,
- `--target=i386-elf`,
- `--target=i386-pyramidos`,
- object-only output,
- volatile/packed/aligned/section controls,
- mixed C/Baa kernel smoke tests.

PyramidOS kernel core should not move to Baa until the freestanding profile passes dedicated QEMU boot tests.

---

## 7. Breaking Change Policy

A Baa change is considered breaking if it changes:

- accepted valid programs,
- rejected invalid programs in a way that affects conformance tests,
- diagnostic JSON fields,
- diagnostic codes,
- build manifest fields,
- target names,
- ABI behavior,
- stdlib signatures,
- exit codes used by Takween/Qalam.
- the Baa-to-Nazm textual or embedding boundary.

Breaking changes require:

1. roadmap entry,
2. compatibility matrix update,
3. migration note,
4. conformance test update,
5. Takween/Qalam/Nazm impact note when relevant.

---

## 8. Anti-Scope-Drift Rules

- Do not add `baa build` while Takween owns project build UX.
- Do not add editor/IDE roadmap items to Baa while Qalam owns IDE UX.
- Do not add PyramidOS kernel migration tasks to Baa before the freestanding profile exists.
- Do not add production self-hosting work before v0.9 stable beta freeze.
- Do not duplicate Nazm's parser, encoder, or object writers inside Baa.
- Do not make Nazm the production assembler before backend/optimizer/release quality and parity gates are stable.
