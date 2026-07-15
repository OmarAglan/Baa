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

The 2026-07-15 baseline contains 100 sources per target with zero omissions:

| Target | Instruction forms | Directive forms | Sections | Registers | Relocation candidates |
|---|---:|---:|---:|---:|---:|
| `x86_64-linux` | 107 | 14 | 4 | 41 | 7 |
| `x86_64-windows` | 103 | 12 | 3 | 42 | 7 |

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

| Target | Instruction forms S/P/U | Directive forms S/P/U | Sections S/U | Relocations S/P/U |
|---|---:|---:|---:|---:|
| `x86_64-linux` | 60 / 2 / 45 | 8 / 1 / 5 | 3 / 1 | 5 / 2 / 0 |
| `x86_64-windows` | 59 / 2 / 42 | 8 / 1 / 3 | 3 / 0 | 5 / 2 / 0 |

Each supported row names the checked Nazm acceptance fixture that exercises
its canonical Arabic lowering. Baa emits the entry label as `الرئيسية`; Nazm
preserves that exported Arabic symbol as `الرئيسية` in ELF64 and COFF. The
shadow linker selects it explicitly as the process entry without an ASCII
alias. Production remains GAS by default, but now also exports `الرئيسية`
unchanged and links through the Arabic hosted startup symbol `الرئيسية_بدء`.
Compiler-owned platform calls are translated through an explicit Arabic runtime
ABI bridge; arbitrary Latin source and external identifiers remain visible
rejections. Read-only string tables, zero-initialized data, explicit alignment,
and MOV/LEA PC-relative global memory are supported. Immediate-to-symbol stores
remain a documented producer lowering, while remaining memory forms and scalar
SSE2 remain partial or unsupported where applicable.

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
SHA-256 digest. The matrix records the stable source and rejection reason;
every unsupported row also records an Arabic `blocker.kind` and optional
`blocker.detail`, while each target aggregates the same data under
`unsupported_blockers` to drive the next admission slice without parsing prose.
each emitted artifact separately carries the exact versioned source mapping
described below rather than duplicating per-instruction spans in the corpus matrix.

The current source-level baseline is target-specific and has no gate errors:

| Target | Arabic-only emitted | Visible unsupported | Gate errors |
|---|---:|---:|---:|
| `x86_64-linux` | 57 | 43 | 0 |
| `x86_64-windows` | 58 | 42 | 0 |

The admitted sources now include string-heavy examples, runtime diagnostics,
dynamic memory, file and error helpers, integer-width and callee-saved-register
cases, stack arguments, tail calls, arrays, structures, unions, function
pointers, and the earlier integer/control corpus. Every admitted source uses an
Arabic-only Nazm artifact and passes production/shadow object, link, and runtime
comparison on its host gate. Nazm may retain relocations for same-object symbols
that GAS resolves while assembling; the gate therefore compares normalized
section/symbol structure, requires every production relocation site to remain
represented, and proves the retained relocations through real linking and
runtime behavior.

The PC-relative slice removes 21 global-value blockers per target and admits 11
additional sources. The remaining visible blockers are now led by later memory
forms (21 Linux, 24 Windows); six value-operand cases are SSE/aggregate follow-on
work. Four conversion configurations and four Latin-spelled source functions
remain unsupported on each target, with isolated width mismatch, scalar-SSE,
and legacy inline-assembly cases. None falls back to GAS inside the shadow
result.

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
   separately versioned assembler cutover, while both paths preserve the Arabic entry ABI.
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

## Source-map and assembler-diagnostic contract

Every successful `--emit-nazm` source `<output>` is accompanied by
`<output>.خريطة-باء.json` using `baa-nazm-source-map-v1`. Each entry maps an
inclusive generated Nazm line range to the originating Baa UTF-8 file, line,
and column. File paths are stored as lowercase hexadecimal UTF-8 bytes so
Windows separators, Arabic paths, quotes, and other valid path bytes require no
host-specific JSON escaping. The generated `.نظم` source itself remains
Arabic-only.

The shadow driver redirects Nazm stderr without a shell, replays it unchanged,
extracts the reported generated `file:line:column`, and resolves that line
through the sidecar. When a mapping exists it adds a stable
`موضع باء الأصلي: <file>:<line>:<column>` diagnostic. A malformed or missing
map never turns an assembler failure into success. Focused tests use a failing
assembler adapter to prove the mapping on Windows and Linux and also verify
that unsupported emission leaves neither source nor sidecar output.

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
- `<output>.ظل-نظم.نظم.خريطة-باء.json` — `baa-nazm-source-map-v1` spans;
- `<output>.ظل-نظم.obj` or `.o` — Nazm object;
- `<output>.ظل-نظم.exe` or the suffixless Linux equivalent — shadow executable.

Missing Nazm, an unsupported emitter form, assembler failure, or shadow-link
failure makes the command fail; assembler diagnostics map back to Baa source
locations, and production GAS success never hides it. The
ecosystem test compares the synthetic minimal slice and every admitted
100-source corpus member for Arabic source, `.text`, exported entry symbol,
relocation structure, link success, exit status, stdout, and stderr.
Dedicated `nazm-shadow-windows` and `nazm-shadow-linux` CI jobs build both
repositories and run this real parity path on every Baa change.

The full corpus is classified automatically, and object/link/runtime comparison
covers all 57 Linux and 58 Windows emitted members. Nazm and Baa now share the
required integer widths, condition-code writes, extension and division forms,
indirect calls, callee-saved frames, integer globals, Arabic external symbols,
absolute data relocations, PC-relative global loads/stores/address formation,
read-only string tables, BSS, and alignment. Baa maps
compiler-owned platform ABI names to Arabic runtime adapters and rejects any
unmapped Latin symbol.

On Windows, both production and shadow executables enter through the strong
Arabic runtime symbol `الرئيسية_بدء`, which dispatches to `بدء_ويندوز`; no
`main`, `wmain`, or mojibaked linker alias is involved. Baa routes the UTF-8
entry spelling through a linker-owned response file so GNU `ld` consumes the
symbol bytes directly. The remaining production-admission work is additional
PIC/base-index memory forms, scalar SSE2, and the complete hosted gate set. GAS
therefore remains the default assembler.
