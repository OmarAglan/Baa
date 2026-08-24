# Baa Ecosystem Boundaries

> **Version:** draft-0.2 | **Applies to:** Baa v0.6.0+

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
| Baa-LSP | Baa-only LSP adapter between editors and compiler contracts |
| ArbSh | Arabic-first shell and standalone terminal host |
| Baa-Developer-Kit | Offline installer orchestration and release-manifest owner |
| Pyramid-Engine | Arabic-capable native runtime and future hosted Baa scripting consumer |
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

Baa owns its Machine IR, register allocation, ABI lowering, prologue/epilogue,
canonical Arabic Nazm emission, and assembler-path selection. Nazm owns Arabic
assembly syntax, instruction encoding, relocations, object serialization, and
the stable assembler API. The admitted production boundary is a Nazm process;
an explicit Baa build/invocation may use the same API in-process without moving
those ownership lines.

---

## 3. Nazm Ownership

Nazm owns:

- canonical Arabic assembly syntax and terminology,
- parsing and operand validation,
- x86-64 instruction encoding,
- symbol and relocation handling,
- ELF64 and PE/COFF object serialization,
- assembler diagnostics, listings, CLI, and `nazm-api-v1` embedding API.

Baa must support Nazm integration by providing:

- a complete inventory of emitted machine forms for Windows and Linux,
- a canonical Arabic Nazm text emitter after register allocation,
- source mapping from generated assembly back to Baa source,
- a shadow mode that never changes production output silently, and
- parity tests for objects, links, runtime behavior, symbols, and relocations.

Nazm's default admission has passed Baa's quick, full, stress, determinism, and
cross-target gates. Unsupported forms must still fail visibly; integration must
not guess bytes or silently fall back to GAS. Making the in-process API the
default is a separate future admission and cannot be inferred from API parity.

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
- side-effect-free unsaved-source input with a logical path,
- diagnostics JSON,
- token dump JSON,
- symbol outline JSON,
- completion metadata,
- cursor-sensitive semantic query data for hover and call signatures,
- stable diagnostic codes.

Qalam-facing roadmap entries in Baa must stay limited to compiler/data contracts: check
modes, machine-readable diagnostics, token streams, symbol metadata, completion metadata,
and semantic query data.
Baa does not own editor widgets, IDE commands, extension packaging, themes, layout, or UI flows.

Qalam should not parse unstable human-readable compiler text when a machine-readable contract exists.

---

## 6. Baa-LSP Ownership

Baa-LSP owns:

- Language Server Protocol framing and lifecycle,
- editor document synchronization and cancellation,
- translation of versioned Baa data contracts into LSP structures,
- UTF-8 byte to UTF-16 position conversion,
- server recovery and structured local logging contracts.

Baa-LSP must not infer language semantics that belong to Baa or own Qalam UI.

---

## 7. ArbSh Ownership

ArbSh owns:

- the Arabic shell language, parser, command discovery, and object pipeline,
- interactive session state, history, completion, and shell diagnostics,
- structured external-process launch and process-tree cancellation,
- PTY/ConPTY terminal-session behavior for its standalone host,
- terminal rendering, input, selection, clipboard, ANSI/VT handling, and BiDi UX,
- the future `arbsh-host-v1` contract consumed by Qalam and installers.

Baa supports ArbSh through stable UTF-8 CLI, exit-code, diagnostic, and target
contracts. ArbSh must not reimplement the Baa compiler, Takween project model,
Nazm encoding, or Baa-LSP semantic analysis. Baa, Nazm, and Takween must remain
usable without ArbSh.

---

## 8. Pyramid-Engine Ownership

Pyramid-Engine owns its native runtime, graphics, platform, input, asset,
international-text, UI, scene, and future editor behavior. Its C++ runtime and
CMake build remain the behavioral reference.

Baa may later support gameplay scripting through an explicitly admitted hosted
embedding contract covering ABI/FFI, allocation, strings, errors, threads,
module loading, debugger hooks, and hot reload. No engine-core rewrite or build
system replacement is implied by ecosystem membership.

---

## 9. Baa-Developer-Kit Ownership

Baa-Developer-Kit owns:

- the offline installer manifest and component hash verification,
- silent component-installer ordering and health checks,
- coordinated install/upgrade/repair/uninstall acceptance tests,
- release packaging of independently owned installer artifacts.

It does not own component files, PATH entries, versions, or uninstall behavior.
ArbSh joins the default kit only after its independent installer contract passes;
Pyramid-Engine and PyramidOS remain optional, separately released workloads.

---

## 10. PyramidOS Ownership

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

## 11. Breaking Change Policy

A Baa change is considered breaking if it changes:

- accepted valid programs,
- rejected invalid programs in a way that affects conformance tests,
- diagnostic JSON fields,
- diagnostic codes,
- build manifest fields,
- target names,
- ABI behavior,
- stdlib signatures,
- exit codes used by Takween/Qalam/ArbSh.
- the Baa-to-Nazm textual or embedding boundary.
- a future hosted embedding contract consumed by Pyramid-Engine.

Breaking changes require:

1. roadmap entry,
2. compatibility matrix update,
3. migration note,
4. conformance test update,
5. Takween/Qalam/Baa-LSP/ArbSh/Nazm/Pyramid consumer impact note when relevant.

---

## 12. Anti-Scope-Drift Rules

- Do not add `baa build` while Takween owns project build UX.
- Do not add editor/IDE roadmap items to Baa while Qalam owns IDE UX.
- Do not add PyramidOS kernel migration tasks to Baa before the freestanding profile exists.
- Do not add production self-hosting work before v0.9 stable beta freeze.
- Do not duplicate Nazm's parser, encoder, or object writers inside Baa.
- Do not replace the admitted Nazm process boundary with the embedded API by default without a separate parity and production-admission decision.
- Do not put shell parsing or terminal emulation in Baa; ArbSh owns that surface.
- Do not move Pyramid-Engine or PyramidOS runtime code into Baa to demonstrate integration; start with narrow consumer-side modules behind admitted contracts.
- Do not make the developer kit a second owner of independently installed component files.
