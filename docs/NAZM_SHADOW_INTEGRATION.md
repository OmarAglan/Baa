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

The 2026-07-16 baseline contains 100 sources per target with zero omissions:

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
| `x86_64-linux` | 71 / 2 / 34 | 10 / 1 / 3 | 3 / 1 | 5 / 2 / 0 |
| `x86_64-windows` | 70 / 2 / 31 | 10 / 1 / 1 | 3 / 0 | 5 / 2 / 0 |

Each supported row names the checked Nazm acceptance fixture that exercises
its canonical Arabic lowering. Baa emits the entry label as `الرئيسية`; Nazm
preserves that exported Arabic symbol as `الرئيسية` in ELF64 and COFF. The
production and shadow linkers both select the Arabic hosted startup symbol
`الرئيسية_بدء` without an ASCII alias. On Linux, the shared startup object
enters libc correctly and calls the returning `الرئيسية`; on Windows the
runtime bridge dispatches through `بدء_ويندوز`. Production remains GAS by
default and exports `الرئيسية` unchanged.
Compiler-owned platform calls are translated through an explicit Arabic runtime
ABI bridge; arbitrary Latin source and external identifiers remain visible
rejections. Read-only string tables, zero-initialized data, explicit alignment,
MOV/LEA PC-relative global memory, base/displacement memory-source IMUL, and
spilled SETcc destinations are supported. The eight scalar SSE2 forms map to
Arabic-only `سجل_عشري_*` operands; the debug-line contract adds the fifth
focused acceptance fixture.
Immediate-to-symbol stores remain a documented producer lowering, while later
PIC/addressing forms remain partial or unsupported where applicable.

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

The current source-level baseline is target-specific and has no unsupported
rows or gate errors:

| Target | Arabic-only emitted | Visible unsupported | Gate errors |
|---|---:|---:|---:|
| `x86_64-linux` | 100 | 0 | 0 |
| `x86_64-windows` | 100 | 0 | 0 |

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

The PC-relative slice removed 21 global-value blockers per target. The following
memory-arithmetic slice accepted all 262 emitted IMUL memory-source sites per
target plus 38 Linux and 4 Windows spilled SETcc sites. The scalar-decimal slice
then admitted every XMM move and all eight SSE2 inventory forms. The next slice
renamed four include-path fixture functions to Arabic-only identities, admitted
32-to-64 zero extension of a PC-relative global through a 32-bit destination
view, and lowered spilled unary bitwise-NOT through a scratch register, raising
both targets to 95 sources. The conversion-configuration slice then admitted
all three `--startup=custom` sources: that option controls the inspectable GAS
`-S` presentation and does not own hosted startup in canonical Nazm output.
The structured architecture slice then replaced raw inline text with typed
`لا_تفعل()` and `اقرأ_عداد_الزمن()` operations backed by IR/Machine IR and
added the matching Nazm instruction, raising both targets to 99 sources. The
debug-information slice then added Arabic-only `.ملف_بايتات` and `.موضع`
directives. Nazm decodes the exact UTF-8 source path and emits a DWARF v4 line
table for ELF64 or CodeView C13 line data for COFF, admitting the 100th source
on both targets. Stack protection still has an explicit `حماية_المكدس`
blocker when requested outside this corpus. No failed Nazm path falls back to
GAS.

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

## Selectable normal assembler path

`--assembler=nazm` promotes the same canonical emitter from comparison-only use
to the normal C-like assembler position:

```text
Baa source -> Machine IR -> Arabic Nazm source -> nazm -> object -> host linker
```

The executable is selected by `--nazm-path=<path>`, then `BAA_NAZM`, then the
primary Arabic command `نظم` from `PATH`. `-S --assembler=nazm` stops at inspectable `.نظم` source;
`-c` writes the requested object directly, including cross-target ELF64/COFF
objects because Nazm owns both writers; a normal host full build links Nazm
objects with the Baa runtime. Cross-target linking remains deferred. Linux uses
a small Arabic-only Nazm startup object
that calls the compiler-owned Arabic hosted-runtime adapter. Missing tools,
unsupported emission, assembly errors, or link errors remain terminal and
never retry through GAS. `--assembler=gas` stays the default rollback until the
production-admission and rollback gates are signed off.

The normal driver also accepts mixed source roots:

```text
module.baa  -> Baa -> selected generated-source assembler -> object
helper.نظم -> Nazm directly                              -> object
both objects -> the same hosted linker and Arabic startup ABI
```

The direct `.نظم` route resolves the executable through the same
`--nazm-path`, `BAA_NAZM`, and `PATH` order. It is valid for `-c` and normal
host links, including cross-target object-only output. It never parses the file
as Baa and never falls back to GAS. Source diagnostics return code `1`;
tool/process failures return code `4`.

## Source-map and assembler-diagnostic contract

Every successful `--emit-nazm` source `<output>` is accompanied by
`<output>.خريطة-باء.json` using `baa-nazm-source-map-v1`. Each entry maps an
inclusive generated Nazm line range to the originating Baa UTF-8 file, line,
and column. File paths are stored as lowercase hexadecimal UTF-8 bytes so
Windows separators, Arabic paths, quotes, and other valid path bytes require no
host-specific JSON escaping. When `--debug-info` is active, the generated
source additionally declares each original path through `.ملف_بايتات` as
Arabic decimal UTF-8 bytes and selects instruction locations through `.موضع`.
Nazm reconstructs the original path only inside the object debug metadata, so
the generated `.نظم` source itself remains Arabic-only.

The shadow driver redirects Nazm stderr without a shell, replays it unchanged,
extracts the reported generated `file:line:column`, and resolves that line
through the sidecar. When a mapping exists it adds a stable
`موضع باء الأصلي: <file>:<line>:<column>` diagnostic. A malformed or missing
map never turns an assembler failure into success. Focused tests use a failing
assembler adapter to prove the mapping on Windows and Linux and also verify
that unsupported emission leaves neither source nor sidecar output.

## Current admission status

Stage B, the Stage B.1 comparison/fixture gate, the structured architecture
slice, and the debug-information slice are complete. `--emit-nazm` emits
Arabic-only `.نظم` for all 100 corpus sources on either target. It retains Baa
line/column spans in Arabic comments and structured `.موضع` directives, uses
`الرئيسية` rather than an ASCII ABI name, and never hides an unsupported
configuration behind GAS.

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

The full corpus is classified automatically, and the host
object/link/runtime comparison is configured to cover all 100 emitted members
on both Linux and Windows. The complete Windows run is green locally; the
matching Linux result remains an exact-SHA hosted-CI receipt. Nazm and Baa now share the
required integer widths, condition-code writes, extension and division forms,
indirect calls, callee-saved frames, memory-source multiplication, spilled
condition-code writes, integer globals, Arabic external symbols,
absolute data relocations, PC-relative global loads/stores/address formation,
read-only string tables, BSS, alignment, and the complete scalar-decimal
instruction surface emitted by the current corpus. Baa maps
compiler-owned platform ABI names to Arabic runtime adapters and rejects any
unmapped Latin symbol. Linux Nazm calls also preserve the machine-level
System V decimal-register count in `سجل_المركم_٣٢` before `ناد`, including
variadic decimal printing; the focused emitter test checks this cross-target
even when it runs on Windows. The former debug source now assembles to
`.debug_line` plus `.rela.debug_line` on ELF64 and `.debug$S` plus
`IMAGE_REL_AMD64_SECREL`/`IMAGE_REL_AMD64_SECTION` relocations on COFF.

On both targets, production and shadow executables enter through the strong
Arabic symbol `الرئيسية_بدء`; no `main`, `wmain`, direct-function process entry,
or mojibaked linker alias is involved. Linux reuses the compiler-generated
startup object that calls `__libc_start_main`, so buffered I/O, arguments, and
normal return semantics match production. Windows dispatches to `بدء_ويندوز`
and routes the UTF-8 entry spelling through a linker-owned response file so GNU
`ld` consumes the symbol bytes directly. Raw relocation counts are not compared
across the full corpus because Nazm resolves same-object references that GAS may
leave to the linker; focused fixtures verify relocation kinds, while successful
linking and identical runtime behavior prove required external relocations. The
remaining production-admission work is additional PIC/base-index memory forms
when the producer requires them, stack-protector lowering, and the complete
hosted quick/full/stress/determinism/release gate set. GAS therefore remains
the default assembler.
