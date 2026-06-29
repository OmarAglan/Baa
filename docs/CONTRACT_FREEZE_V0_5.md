# Baa v0.5.9 Contract Freeze Index

> **Version:** 0.5.9 | **Freeze ID:** `baa-core-contracts-v0.5.9`

This index publishes the core contract baseline for the v0.5.x C reference compiler. It names
the authoritative documents, implementation handles, tests, and change rules used to evaluate
the Phase 4.5 exit criterion.

The freeze is a release-candidate baseline, not a promise that Baa has reached 1.0 stability.
Roadmap-approved v0.6–v0.9 changes may revise these contracts only with synchronized tests,
documentation, compatibility notes, and version metadata.

## 1. Language Contract

**Contract ID:** `baa-language-v0.5.9`

Authoritative specification:

- `docs/LANGUAGE.md`
- `docs/KNOWN_LIMITATIONS.md`

Implementation handles:

- `src/frontend/lexer.c` and `src/frontend/lexer.h`
- `src/frontend/parser*.c` and parser headers
- `src/frontend/analysis*.c` and semantic-analysis headers

Evidence:

- `tests/integration/**/*.baa`
- `tests/neg/*.baa`
- `tests/test.py`
- `tests/regress.py`

The accepted grammar, type behavior, scope rules, and documented rejections form the baseline.
Diagnostic wording may improve before the diagnostics freeze only when negative anchors and
documentation are updated together.

## 2. Standard Library Contract

**Contract ID:** `baa-stdlib-v0.5.9`

Authoritative declarations:

- `stdlib/baalib.baahd`
- the ownership and failure rules in `docs/BOOTSTRAP_CONTRACT.md`
- the user-facing behavior in `docs/LANGUAGE.md`

Implementation handles:

- compiler-recognized builtins under `src/frontend/analysis_builtins.c`
- lowering under `src/middleend/ir_lower_*`

Evidence:

- standard-library integration tests under `tests/integration/`
- builtin arity/type failures under `tests/neg/`

Callable names, parameter/return types, ownership, and documented failure behavior are frozen
for this baseline.

## 3. Hosted Target ABI Contract

**Contract ID:** `baa-hosted-abi-v0.5.9`

Supported targets:

- `x86_64-windows` using COFF and the Windows x64 ABI
- `x86_64-linux` using ELF and the SystemV AMD64 ABI

Authoritative specification:

- ABI section of `docs/BOOTSTRAP_CONTRACT.md`
- `docs/BAA_IR_SPECIFICATION.md` data-layout rules

Implementation handles:

- `src/backend/target.h` and `src/backend/target.c`
- `src/support/target_contract.h`
- `src/backend/isel*.c`, `regalloc*.c`, and `emit*.c`

Evidence:

- `tests/integration/backend/`
- cross-target assembly checks in `scripts/test_determinism.py`
- Windows/Linux strict builds and release receipts

Calling conventions, register roles, pointer width, stack alignment, object format, and
cross-target `-S` behavior are frozen. Freestanding/i386 profiles are explicitly excluded.

## 4. IR Contract

**Contract ID:** `baa-ir-v0.5.9`

Authoritative specification:

- `docs/BAA_IR_SPECIFICATION.md`
- `docs/IR_DEVELOPER_GUIDE.md`

Implementation handles:

- `src/middleend/ir.h`
- `src/middleend/ir_verify_ir.c`
- `src/middleend/ir_verify_ssa.c`
- IR mutation, data-layout, text, optimizer, and lowering modules under `src/middleend/`

Evidence:

- `tests/integration/ir/`
- `tests/snapshots/ir_hashes.json`
- `--verify-ir`, `--verify-ssa`, and `--verify-gate`
- release determinism checks for raw and optimized IR

Opcode/type meaning, CFG and terminator rules, SSA invariants, verifier requirements, and the
documented Arabic text form are frozen for this baseline.

## 5. Excluded Draft Contracts

The following are planning documents and are not part of this freeze because their compiler
surfaces are not implemented in v0.5.9:

- `docs/TOOLING_CONTRACTS.md`
- `docs/DIAGNOSTICS_JSON_SCHEMA.md`
- `docs/TARGET_SPECIFICATION.md`
- `docs/CONFORMANCE_SUITE.md`

In particular, `--check`, JSON diagnostics/tokens/symbols, stable diagnostic codes, target-spec
printing, and freestanding targets remain future roadmap work.

## 6. Change Control

A change to a frozen core contract requires:

1. an explicit roadmap item,
2. implementation and focused positive/negative tests in the same change,
3. updates to every affected authoritative document,
4. a compatibility and ecosystem impact note,
5. a version-sync clean result, and
6. fresh affected-platform QA receipts before release.

The eventual v0.5.9 release tag pins the exact file revisions. Until that tag exists, this index
and the current C reference implementation are the canonical release-candidate baseline.
