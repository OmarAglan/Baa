# Nazm Shadow Integration

This document records the evidence and admission rules for
`baa-nazm-boundary-v0`. Production Baa still emits AT&T/GAS assembly and uses
the host toolchain. Nazm is not a default or fallback assembler.

## Stage B inventory

The canonical machine-readable artifact is
[`generated/assembly_surface_v1.json`](generated/assembly_surface_v1.json).
It is generated from every `.baa` source under `tests/integration`,
`tests/stress`, and `examples`, while honoring each source's `// FLAGS:`
metadata. The inventory command compiles every source for both supported
x86-64 targets and fails if any source cannot produce assembly.

The 2026-07-13 baseline contains 100 sources per target with zero omissions:

| Target | Instruction forms | Directive forms | Sections | Registers | Relocation candidates |
|---|---:|---:|---:|---:|---:|
| `x86_64-linux` | 108 | 15 | 4 | 41 | 7 |
| `x86_64-windows` | 105 | 13 | 3 | 42 | 7 |

The section surface observed in the corpus is `.text`, `.data`, and
target-specific read-only data (`.rodata` or `.rdata`), plus
`.note.GNU-stack` on Linux. The instruction inventory includes the integer
width, extension, condition-code, division, and scalar SSE2 families that are
known Nazm coverage work.

`relocation_candidates` are syntactic sites in emitted text. They are not a
claim about final ELF/COFF relocation records; object-level relocation
comparison remains a Stage C gate.

Regenerate the artifact after a backend or corpus change:

```text
python scripts/inventory_assembly_surface.py \
  --compiler build/baa \
  --output docs/generated/assembly_surface_v1.json
```

Verify it without rewriting:

```text
python scripts/inventory_assembly_surface.py \
  --compiler build/baa \
  --output docs/generated/assembly_surface_v1.json \
  --check
```

Baa's QA runner executes the inventory parser tests, the full inventory
`--check` gate, and the Nazm coverage contract tests. A stale artifact, a
missing source, a failed source compilation, an unclassified emitted form, or
a supported form without a named fixture fails QA visibly.

## Stage B.1 coverage contract

The generated comparison artifact is
[`generated/baa_nazm_coverage_v1.json`](generated/baa_nazm_coverage_v1.json).
`scripts/generate_nazm_coverage.py` compares the checked Baa inventory with
Nazm's versioned `nazm-capabilities-v1` document. Every inventory form is
classified as:

- `supported`: the complete form is accepted and has a focused Nazm fixture;
- `partial`: only the stated subset is accepted and is not shadow-ready; or
- `unsupported`: the shadow path must fail visibly and must not fall back.

The initial comparison covers all 100 sources on both targets with zero
inventory omissions:

| Target | Instruction forms S/P/U | Directive forms S/P/U | Sections S/U | Relocations P/U |
|---|---:|---:|---:|---:|
| `x86_64-linux` | 30 / 1 / 77 | 6 / 3 / 6 | 2 / 2 | 1 / 6 |
| `x86_64-windows` | 29 / 1 / 75 | 6 / 2 / 5 | 2 / 1 | 1 / 6 |

Each supported row names the checked Nazm acceptance fixture that exercises
its canonical Arabic lowering. Baa emits the entry label as `الرئيسية`; Nazm
maps that exported Arabic symbol internally to the platform ABI name `main`.
Arbitrary ASCII source identifiers, external calls, RIP-relative references,
read-only sections, byte/32-bit forms, `setcc`, extensions, and scalar SSE2
remain partial or unsupported where applicable.

Regenerate or verify the comparison from an ecosystem checkout:

```text
python scripts/generate_nazm_coverage.py
python scripts/generate_nazm_coverage.py --check
```

## Stage B.2 source-level shadow matrix

The generated [`baa-nazm-shadow-corpus-v1`](generated/baa_nazm_shadow_corpus_v1.json)
artifact runs `--emit-nazm` against the same 100 sources for both targets. It
records one explicit result per source: Arabic-only emission, visible
unsupported status `3`, or a gate error. Unsupported results must leave no
partial output. `baa-nazm-coverage-v1` embeds this complete matrix and pins its
SHA-256 digest. The v1 matrix records the stable source and rejection reason,
but deliberately does not pin an include-level line/column until Nazm source
paths have an owned lifetime contract.

The initial source-level baseline is identical on Linux and Windows:

| Target | Arabic-only emitted | Visible unsupported | Gate errors |
|---|---:|---:|---:|
| `x86_64-linux` | 1 | 99 | 0 |
| `x86_64-windows` | 1 | 99 | 0 |

The admitted corpus source is
`tests/integration/backend/backend_pp_nested_test.baa`. It now passes the real
GAS/Nazm object, link, and runtime comparison on both CI hosts. The 99 visible
rejections quantify the next backend work: 48 first encounter a string table,
26 a global, 12 an unsupported machine instruction, 5 a callee-saved register,
4 a function whose Baa spelling still contains Latin letters, and 4 an
unsupported startup/debug/stack-protection mode.

Regenerate or verify the matrix with the current compiler:

```text
python scripts/inventory_nazm_shadow_corpus.py --compiler build/baa
python scripts/inventory_nazm_shadow_corpus.py --compiler build/baa --check
```

## Non-default shadow path

The shadow path is admitted in ordered increments:

1. The checked Stage B inventory is the required input to Nazm coverage work.
2. Baa adds a canonical Arabic emitter after register allocation. Its output is
   inspectable UTF-8 `.نظم` source; the public `-S` contract remains GAS until a
   separately versioned cutover.
3. An explicit, non-default shadow option invokes Nazm beside the successful
   production GAS path. A missing Nazm executable, unsupported form, assembler
   failure, or comparison failure makes the shadow result fail.
4. CI compares normalized sections, public symbols, relocations, diagnostics,
   link results, and runtime results on Windows and Linux. Incidental object
   byte identity is not required.
5. Nazm becomes eligible for a production cutover only after Baa's quick,
   full, stress, determinism, release, and cross-target gates pass through the
   Nazm path.

There is no silent fallback rule at every stage: a production GAS result may
continue to exist for comparison, but it never converts a failed Nazm shadow
result into success. Unsupported forms must be reported and remain visible in
the coverage matrix.

## Current admission status

Stage B, the Stage B.1 comparison/fixture gate, and the first executable
emitter slice are complete. `--emit-nazm` now emits an Arabic-only `.نظم`
source for a minimal integer entry program on either target. It retains Baa
line/column spans in Arabic comments, uses `الرئيسية` rather than an ASCII ABI
name, and fails with status `3` before leaving an output file when any machine
form is unsupported.

The first explicit `--nazm-shadow=<path>` slice is admitted for one input at a time,
where `<path>` is the Nazm executable. Baa still completes its production GAS
build, but also invokes Nazm with structured argv and links these receipts next
to the requested output:

- `<output>.ظل-نظم.نظم` — canonical Arabic source;
- `<output>.ظل-نظم.obj` or `.o` — Nazm object;
- `<output>.ظل-نظم.exe` or the suffixless Linux equivalent — shadow executable.

Missing Nazm, an unsupported emitter form, assembler failure, or shadow-link
failure makes the command fail; production GAS success never hides it. The
ecosystem test compares the synthetic minimal slice and the first admitted
100-source corpus member for Arabic source, `.text`, exported entry symbol,
relocation absence, link success, exit status, stdout, and stderr.
Dedicated `nazm-shadow-windows` and `nazm-shadow-linux` CI jobs build both
repositories and run this real parity path on every Baa change.

The full corpus is now classified automatically, but object/link/runtime
comparison is admitted only for its one currently emitted member. Nazm
still needs support for Baa's observed operand widths, `setcc`, extension and
division forms, scalar SSE2 forms, external symbols, read-only sections, and
the required ELF64/COFF relocations. The production assembler therefore remains
unchanged.
