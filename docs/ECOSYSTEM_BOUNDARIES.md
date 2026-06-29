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

---

## 3. Takween Ownership

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

## 4. Qalam-IDE Ownership

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

Qalam should not parse unstable human-readable compiler text when a machine-readable contract exists.

---

## 5. PyramidOS Ownership

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

## 6. Breaking Change Policy

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

Breaking changes require:

1. roadmap entry,
2. compatibility matrix update,
3. migration note,
4. conformance test update,
5. Takween/Qalam impact note when relevant.

---

## 7. Anti-Scope-Drift Rules

- Do not add `baa build` while Takween owns project build UX.
- Do not add editor/IDE roadmap items to Baa while Qalam owns IDE UX.
- Do not add PyramidOS kernel migration tasks to Baa before the freestanding profile exists.
- Do not add production self-hosting work before v0.9 stable beta freeze.
- Do not add assembler/linker independence before backend/optimizer/release quality is stable.
