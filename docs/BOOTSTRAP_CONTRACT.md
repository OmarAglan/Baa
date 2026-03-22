# Baa Bootstrap Contract

> **Version:** v0.5.1 freeze contract (bootstrap-facing) | [Language Spec →](LANGUAGE.md)

This document freezes the current Baa language, stdlib, ABI, and IR contracts that later
bootstrap work must treat as stable inputs.

It is intentionally conservative:

- no new syntax is introduced here,
- no ABI behavior is widened here,
- no IR guarantees are promised beyond what the current compiler already implements and verifies.

## Canonical Sources

The freeze is defined by these sources of truth:

- Language surface: `docs/LANGUAGE.md`
- Standard library callable signatures: `stdlib/baalib.baahd`
- IR semantics and verifier-backed invariants: `docs/BAA_IR_SPECIFICATION.md`
- Target/ABI implementation contract: `src/backend/target.h` and `src/backend/target.c`

If detailed docs drift from these surfaces, this document and the files above win for bootstrap
planning.

## Frozen Language Surface

The following source-language surface is frozen for bootstrap readiness:

- UTF-8 source files with `.baa` extension
- `الرئيسية` as the only program entry-point name
- current preprocessor directives:
  - `#تضمين`
  - `#تعريف`
  - `#إذا_عرف`
  - `#وإلا`
  - `#نهاية`
  - `#الغاء_تعريف`
  - `#خطأ`
- current core type spellings, including:
  - signed/unsigned integer families
  - `منطقي`
  - `حرف`
  - `نص`
  - `عشري`
  - `عشري٣٢` as a source-level alias that still lowers as `f64`
  - `عدم`
  - function-pointer syntax `دالة(...) -> ...`
- current control-flow and declaration syntax
- current function-pointer syntax and semantics
- current variadic function syntax with `...`
- current inline-assembly syntax through `مجمع`

The only frozen entry-point signatures are:

```baa
صحيح الرئيسية()
صحيح الرئيسية(صحيح عدد، نص[] معاملات)
```

Bootstrap work must not assume additional accepted entry-point shapes.

## Frozen Standard Library Surface

`stdlib/baalib.baahd` is the canonical stdlib signature surface for bootstrap work.

The current frozen modules are:

- string APIs
- dynamic memory APIs
- file I/O APIs
- math APIs
- system APIs
- time APIs
- error-handling APIs
- variadic intrinsics

Freeze rules:

- exact callable signatures come from `stdlib/baalib.baahd`
- ownership rules documented there are part of the contract
- heap-returning APIs remain heap-returning APIs and must be freed by the caller as documented
- no new stdlib signatures should be introduced under the bootstrap freeze without explicitly
  revising this contract

## Frozen Target / ABI Surface

Only these targets are in scope for the freeze:

- `x86_64-windows`
- `x86_64-linux`

Windows x64 ABI contract:

- integer/pointer argument registers: `RCX`, `RDX`, `R8`, `R9`
- return register: `RAX`
- 32-byte shadow space at call boundaries
- object format: COFF/PE
- executable suffix default: `.exe`

SystemV AMD64 ABI contract:

- integer/pointer argument registers: `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9`
- return register: `RAX`
- no shadow space
- varargs path currently emits `AL=0` when no XMM arguments are used
- object format: ELF
- executable suffix default: empty

Shared target limits:

- x86-64 only
- current data layout stays 64-bit and identical across the two supported x86-64 targets
- cross-target assembly generation is supported; bootstrap work must not assume broader host/toolchain
  guarantees than the current compiler documents

## Frozen IR / Verifier Surface

The bootstrap freeze relies on the current Baa IR contract, not a future generalized IR.

Frozen guarantees:

- SSA-form IR before Out-of-SSA
- verifier-backed single-definition SSA discipline
- verifier-backed dominance and phi incoming-edge checks
- verifier-backed well-formedness for:
  - operand counts
  - type consistency
  - terminator rules
  - phi placement
  - intra-module call signature checks
- current x86-64 data-layout assumptions
- current typed-memory model for `حجز` / `حمل` / `خزن`

Non-guarantees:

- no promise of new IR opcodes for bootstrap
- no promise of new alias-analysis precision
- no promise of non-x86-64 data layouts in this freeze

## Frozen Unsupported Surface

The following remain intentionally unsupported under the freeze and must stay unsupported unless a
later milestone explicitly reopens them:

- variadic function-pointer calls
- advanced inline-assembly constraints and clobber lists beyond the currently documented subset
- additional entry-point signatures beyond the two frozen forms
- new bootstrap-relevant syntax churn in grammar/type spelling
- new target families or ABI contracts beyond current Windows x64 and SystemV AMD64 support

## Enforcement Expectations

The freeze is locked by:

- documentation synchronization against the canonical sources above
- existing verifier paths: `--verify-ir`, `--verify-ssa`, and `--verify`
- focused integration tests that lock:
  - stdlib short include behavior
  - entry-point argument lowering
  - SysV register-argument behavior
  - Windows shadow-space/register-call behavior
  - verifier-safe CFG/phi compilation

This document is the bootstrap-facing summary, but it does not replace the detailed language,
stdlib, ABI, and IR references that define exact syntax and declarations.
