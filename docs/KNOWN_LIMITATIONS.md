# Baa Known Limitations

> **Baseline:** v0.5.8 reference compiler / v0.5.9 release-candidate preparation

This page describes unsupported or intentionally deferred behavior in the current C reference
compiler. Draft roadmap and tooling documents describe future contracts; they do not imply that
those features are implemented today.

## Platforms and Toolchain

- Supported targets are `x86_64-windows` (COFF/Windows x64 ABI) and `x86_64-linux`
  (ELF/SystemV AMD64 ABI).
- Cross-target compilation is supported for assembly output (`-S`) only. Object generation and
  linking require a matching host or cross toolchain.
- Baa emits GAS/AT&T assembly and relies on host GCC/Clang for assembly and linking. It does not
  ship an independent assembler or linker.
- macOS, 32-bit targets, ARM, WebAssembly, and freestanding targets are not supported.
- `i386-elf`, `i386-pyramidos`, `--freestanding`, and `--no-stdlib` are planning surfaces only.
- The `baa update` command is implemented on Windows only.
- Building the reference compiler requires a C toolchain. It never requires a Baa-built compiler.

## Language and Type System

### Aggregates

- Struct/union initialization with `=` is not supported; initialize individual fields through
  member access.
- Whole-aggregate assignment and using aggregate array elements as first-class values are not
  supported.
- Aggregate array initializer lists are not supported by the IR path.
- Array fields inside structs/unions and recursive/cyclic aggregate definitions are not supported.
- User-defined aggregate types are not supported as function parameter or return types.

These restrictions are scheduled for explicit policy and ABI work rather than being silently
treated as C-compatible behavior.

### Function pointers

- Higher-order function-pointer signatures (function pointers taking or returning function
  pointers) are not supported.
- Variadic calls through function pointers are not supported.
- Explicit casts to or from function-pointer types are not supported.
- Arrays of function pointers are not supported.
- Indirect calls use a function-pointer identifier as the callee; arbitrary callee expressions
  are not supported.

### Numeric and text behavior

- `عشري٣٢` is currently a source-level alias of `عشري`; both use the current f64
  representation. SIMD/vector types are not supported.
- Floating-point remainder and floating-point increment/decrement are not supported.
- `نص` elements cannot be modified through indexing.
- Text storage is UTF-8, but the current string helpers do not promise Unicode normalization or
  grapheme-cluster-aware indexing/length behavior.
- Runtime checks are optional and currently cover selected array-bounds paths. Baa is not a
  memory-safe language; raw pointers and inline assembly remain unsafe operations.

### Other syntax and runtime limits

- `ساكن` cannot qualify a function definition.
- Inline assembly supports only the documented `a`, `c`, and `d` input constraints and `=a`,
  `=c`, and `=d` output constraints.
- Formatted input does not support `%ح`, `%م`, or dynamic `*` precision.

## Tooling Contracts Not Yet Implemented

The following surfaces appear in draft v0.7+ contract documents but are not accepted by the
current driver:

- `--check`
- `--diagnostics=json`
- stable diagnostic codes and `--explain <CODE>`
- JSON token dumps
- JSON symbol outlines
- completion metadata export
- `--print-targets` and `--print-target-spec`

The files under `tests/conformance/` are a seed layout. Cases that mention these future flags do
not yet form an executable release gate.

Implemented machine-facing surfaces currently include deterministic build manifests,
incremental-cache metadata, IR dumps, verifier flags, phase timing, and target selection for the
two hosted x86-64 targets.

## Release Status

- v0.5.8 establishes the C reference compiler policy; it is not a self-hosting milestone.
- v0.5.9 Windows and Linux full RC signoffs remain pending until all
  `quick/full/stress/release` gates are recorded green on both hosts.
- The language, diagnostics, ABI, standard library, and external tooling contracts are not
  stable-beta frozen until the v0.9 gates are completed.

When a limitation is removed, the implementation, focused tests, language/tooling documentation,
this page, the compatibility matrix, roadmap, and changelog must be updated together.
