# Baa Tooling Contracts

> **Version:** draft-0.1 | **Applies to:** Baa v0.7.x+

This document defines the compiler surfaces that external tools may rely on.

---

## 1. Principles

- Human-readable Arabic output is for users.
- Machine-readable JSON output is for Takween, Qalam-IDE, CI, and scripts.
- Tools must not parse unstable human text when JSON exists.
- New v1 JSON surfaces use a `schema_version` field; the current deterministic build
  manifest keeps its numeric `schema` field until `build-manifest-v1` is promoted.
- Stable fields are never removed without a schema version bump.

---

## 2. Compiler Modes

| Mode | Purpose | Stable Consumer |
|---|---|---|
| compile/link default | produce executable | users, Takween |
| `-S` | produce assembly | compiler tests, backend tests |
| `-c` | produce object | Takween, future OS experiments |
| `--check-header` | parse + semantic-check header declarations without codegen | Takween, Qalam |
| `--check` | parse + semantic check only | Qalam, Takween |
| `--source-stdin=<file>` | check unsaved UTF-8 source from stdin under a logical file path | Qalam |
| `--emit-build-manifest <file>` | dependency/cache manifest | Takween |
| `--incremental` | reuse cached object files when safe | Takween |
| `--cache-dir <dir>` | select the incremental cache directory | Takween |
| `--diagnostics=json` | machine-readable diagnostics | Qalam, Takween |
| `--dump-tokens=json` | stable token stream | Qalam/debug tools |
| `--dump-structure=json` | tolerant structural editing ranges | Baa-LSP, Qalam |
| `--dump-symbols=json` | symbol outline | Qalam |
| `--completion-data=json` | language-owned keywords, directives, and snippets | Baa-LSP, Qalam |
| `--format=json` | canonical, source-preserving Baa formatting | Baa-LSP, Qalam |
| `--semantic-query=json --position-byte=N` | compiler-owned hover and call-signature data at a UTF-8 byte position | Baa-LSP, Qalam |
| `--semantic-index=json` | compiler-owned symbol identities and occurrences for one translation unit | Baa-LSP |
| `--target=<target>` | target selection | Takween, OS experiments |
| `--target-info=json` | host target, executable suffix, object format, and capabilities | Takween, CI |
| `-I <dir>` / `-I<dir>` | include search path | Takween, users |

---

## 3. Takween Invocation Contract

Takween may rely on these compiler invocation shapes:

```bash
baa --check [-I <dir>...] [--target=<target>] <inputs...>
baa --check --diagnostics=json --source-stdin=<logical-file>
baa --dump-tokens=json [--source-stdin=<logical-file> | <source.baa>]
baa --dump-structure=json [--source-stdin=<logical-file> | <source.baa>]
baa --dump-symbols=json [--source-stdin=<logical-file> | <source.baa>]
baa --completion-data=json
baa --format=json [--source-stdin=<logical-file> | <source.baa>]
baa --semantic-query=json --position-byte=<offset> [--source-stdin=<logical-file> | <source.baa>]
baa --semantic-index=json [-I <dir>...] [--source-stdin=<logical-file> | <source.baa>]
baa --check-header [-I <dir>...] <headers...>
baa [-O0|-O1|-O2] [--verify] [-I <dir>...] <inputs...> -o <executable>
baa -c [-O0|-O1|-O2] [-I <dir>...] <inputs...> -o <object>
baa -S [-O0|-O1|-O2] [--target=<target>] <input> -o <assembly>
baa --incremental --cache-dir <dir> --emit-build-manifest <file> <inputs...> -o <output>
baa [--target=<target>] --target-info=json
```

`--target-info=json` emits the stable `target-info-v1` discovery document. Takween must use
its `host_target`, `selected_target`, `targets[].executable_suffix`, object format, and
capability booleans instead of inferring platform behavior from the host OS or filename.

`--check` is the fast editor/build-tool validation mode: it reads sources and includes,
parses, runs semantic analysis, and stops before IR lowering, optimization, assembly,
object emission, and linking. It may be combined with `--emit-build-manifest`; those
manifests use `"mode": "check"` and record source/include dependencies without an output
artifact.

`--source-stdin=<logical-file>` is the unsaved-editor form of `--check`. The compiler reads
exactly one UTF-8 Baa source from standard input while using `<logical-file>` as the source
identity for diagnostics and source-relative includes. It accepts no positional source,
code-generation mode, incremental cache, or build manifest. The logical file need not contain
the same bytes on disk. This prevents an editor from writing shadow sources into the project
tree and ensures diagnostic byte offsets describe the supplied buffer.

`--dump-tokens=json` emits the compiler-owned, source-preserving lexical
contract `tokens-json-v1`. It accepts saved or unsaved source, validates UTF-8,
and remains usable while the buffer is syntactically incomplete. It does not
expand includes or macros, so comments and directives remain available at
their original byte spans. The complete field and token-kind contract is in
[`TOKENS_JSON_SCHEMA.md`](TOKENS_JSON_SCHEMA.md).

`--dump-structure=json` emits the compiler-owned, tolerant
`structure-json-v1` contract used for folding and semantic selection. It uses
the same raw source scanner as `tokens-json-v1`, so delimiters inside comments
or literals never become structural ranges. A temporarily incomplete buffer
still exits successfully with `complete: false` and every safely recovered
range. See [`STRUCTURE_JSON_SCHEMA.md`](STRUCTURE_JSON_SCHEMA.md).

Stable invocation inputs are:

- source/input file paths,
- `-I` include directories in command-line order,
- output path from `-o`,
- target from `--target=<target>`,
- optimization level,
- validation flags such as `--verify`, `--verify-ir`, `--verify-ssa`, and `--verify-gate`,
- source fast-check mode (`--check`),
- header declaration check mode (`--check-header`),
- runtime-check flags and runtime-check mask,
- incremental cache directory,
- build-manifest path.

Run and clean are Takween workflow operations, not Baa compiler subcommands. Takween runs the
produced executable when a build succeeds, and Takween deletes its own build/cache directories
for clean workflows. `compiler-cli-v1` does not include `baa build`, `baa run`, or `baa clean`.

Hosted Baa build tools should use the structured stdlib process API (`ابدأ_عملية` plus
poll/wait/cancel/exit/free) for compiler and executable invocations. The API preserves argv
boundaries and supports explicit cwd, environment, and stdout/stderr routing. Shell command
strings through `نفذ_أمر` are outside the Takween integration contract. Directory initialization
and cleanup use `انشئ_مجلدات` and guarded `احذف_شجرة` rather than host shell utilities.

---

## 4. Exit Codes

| Code | Meaning |
|---:|---|
| 0 | success |
| 1 | user/source error: syntax, semantic, include, diagnostic failure |
| 2 | invalid compiler invocation |
| 3 | unsupported target or unsupported feature for selected mode |
| 4 | toolchain/backend failure |
| 5 | internal compiler error |

Exit-code meanings are part of `compiler-cli-v1`.
The driver enforces the table at its owning boundaries: CLI parse failures return `2`,
unsupported target/mode combinations return `3`, external assembler/linker and output
failures return `4`, and compiler invariant/allocation failures return `5`. Source and include
diagnostics remain `1`, including when `--diagnostics=json` is enabled. Takween and other
process consumers must preserve the numeric status and must not reclassify failures by parsing
human-readable output.

---

## 5. Build Manifest Contract

`--emit-build-manifest <file>` currently writes deterministic UTF-8 JSON with these stable
top-level fields:

```json
{
  "schema": 1,
  "compiler_version": "0.6.0",
  "target": "x86_64-linux",
  "mode": "link",
  "assembler": "nazm",
  "assembler_fingerprint": "nazm-api-v1;version=0.4.0;capabilities=nazm-capabilities-v1:<sha256>",
  "opt_level": 2,
  "runtime_checks": false,
  "runtime_check_mask": 0,
  "incremental": true,
  "units": [
    {
      "source": "/abs/path/src/main.baa",
      "output": "/abs/path/build/main.o",
      "source_kind": "baa",
      "assembler": "nazm",
      "cache": {
        "enabled": false,
        "hit": false,
        "slot": "...",
        "reason": "bypass"
      },
      "dependencies": [
        { "path": "/abs/path/src/main.baa", "hash": "..." },
        { "path": "/abs/path/include/lib.baahd", "hash": "..." }
      ]
    }
  ]
}
```

Until `build-manifest-v1` is promoted, Takween should treat the numeric `schema` plus the
fields above as the compatibility contract. Adding optional fields is allowed. Removing,
renaming, or changing the meaning of these fields requires a compatibility-matrix note and a
schema bump.

The top-level `assembler` is the policy for generated Baa units. Each unit also
records its actual `source_kind` (`baa` or `nazm`) and `assembler` (`gas` or
`nazm`). Direct `.نظم` units always report `nazm`; generated Baa units report
the production default `nazm` unless the caller explicitly selects GAS.
For Nazm builds, `assembler_fingerprint` is the exact stable value reported by
`nazm-api-v1`: API schema, Nazm version, capability schema, and SHA-256 digest.
It is empty for GAS. Baa includes that whole value in generated-Baa and direct
`.نظم` cache slots, so incremental reuse is enabled only after the fingerprint
has been resolved. A missing or invalid fingerprint is a visible toolchain
failure, never permission to reuse an object from an unknown assembler.

---

## 6. Include and Dependency Contract

The manifest `units[].dependencies[]` list is the canonical invalidation surface for Takween.

Dependency rules:

- Each compiled source unit records its root source and resolved include files.
- Dependency `path` values are canonical paths as resolved by the compiler.
- Dependency `hash` values are content hashes used by incremental cache validation.
- Duplicate include paths are recorded once per source unit after resolution.
- Include diagnostics remain source errors and use exit code `1`.

Takween must invalidate cached build results when any of these values change:

- compiler version,
- manifest `schema`,
- target,
- mode,
- optimization level,
- runtime-check mask,
- ordered input file list,
- ordered `-I` include directory list,
- source or dependency hash,
- output kind when switching between link, `-c`, and `-S`.
- exact assembler fingerprint for every Nazm-produced object.

---

## 7. JSON Output Rules

New v1 JSON outputs must:

- be UTF-8,
- use LF line endings,
- include `schema_version`,
- include compiler `version`,
- use byte offsets and line/column spans where applicable,
- preserve Arabic identifiers as UTF-8 strings,
- avoid localized field names; field names stay English for tool stability,
- put Arabic user-facing messages in values, not keys.

`diagnostics-json-v1` additionally exposes a `fixes` array on every diagnostic.
Machine edits are explicit `quickfix` objects with stable IDs, Arabic titles,
applicability, exact file/span byte ranges, and replacement text. Consumers must not
derive edits by parsing diagnostic messages or hints. The admitted first slice is
non-destructive insertion of missing parser delimiters; stale, overlapping, or
out-of-workspace edits must be refused by the applying tool.

### 7.1 `symbols-json-v1`

`--dump-symbols=json` implies check-only analysis and accepts exactly one Baa
source. It may consume an unsaved buffer through `--source-stdin=<logical-file>`.
It cannot be combined with `--diagnostics=json` because each mode owns one
complete JSON document on stdout.

The output is a document-local declaration tree:

```json
{
  "schema_version": "symbols-json-v1",
  "compiler_version": "0.6.0",
  "file": "/logical/path/main.baa",
  "position_encoding": "utf-8-bytes",
  "symbols": [
    {
      "name": "اجمع",
      "kind": "function",
      "scope": "global",
      "span": {
        "start": { "line": 1, "column": 7, "byte": 11 },
        "end": { "line": 1, "column": 15, "byte": 19 }
      },
      "return_type": { "kind": "int", "display": "صحيح" },
      "modifiers": {
        "const": false,
        "static": false,
        "extern": false,
        "prototype": false
      },
      "children": []
    }
  ]
}
```

`line` and `column` are one-based UTF-8 byte coordinates, while `byte` is a
zero-based absolute UTF-8 offset. Consumers should use the absolute byte spans
when converting to LSP UTF-16 positions. Included-header declarations are not
copied into the root document's outline. Stable `kind` values are `function`,
`parameter`, `variable`, `array`, `type-alias`, `enum`, `enum-member`, `struct`,
`union`, and `field`. Type objects use stable English `kind` values and Arabic
`display` values. Removing or changing these required fields requires a schema
version bump.

### 7.2 `completion-data-json-v1`

`--completion-data=json` accepts no source file and emits the static editing
surface owned by the Baa compiler. Its keyword records come from the same table
used by the lexer, so an editor copy cannot become a second language grammar.

```json
{
  "schema_version": "completion-data-json-v1",
  "compiler_version": "0.6.0",
  "language": "baa",
  "items": [
    {
      "label": "صحيح",
      "kind": "type",
      "detail": "نوع عدد صحيح",
      "documentation": "نوع العدد الصحيح الافتراضي في باء.",
      "filter_text": "صحيح",
      "insert_text": "صحيح",
      "insert_text_format": "plain"
    },
    {
      "label": "الرئيسية (دالة)",
      "kind": "snippet",
      "detail": "قالب نقطة بداية البرنامج",
      "filter_text": "الرئيسية",
      "insert_text": "صحيح الرئيسية() {\n\t${0}\n\tإرجع ٠.\n}",
      "insert_text_format": "snippet"
    }
  ]
}
```

Required item fields are `label`, `kind`, `detail`, `documentation`, `filter_text`,
`insert_text`, and `insert_text_format`. Stable `kind` values in this first
slice are `keyword`, `type`, `value`, `directive`, `snippet`, and `function`.
`insert_text_format` is `plain` or `snippet`; snippets use the standard
`${1:placeholder}` and `${0}` tab-stop notation. The optional `contextual`
boolean marks words such as `نوع` that the parser interprets contextually
rather than reserving lexically.

This static document includes the compiler's canonical callable builtin
inventory and signatures. Scope-aware program symbols are cursor-dependent and
therefore come from the `completion.items` member of
`semantic-query-json-v1`; Baa-LSP does not infer them from the document outline.

### 7.3 `format-json-v1`

`--format=json` accepts exactly one positional Baa source or one unsaved buffer
through `--source-stdin=<logical-file>`. It performs tolerant lexical formatting
without preprocessing or semantic analysis and emits the complete replacement
text. The canonical v1 style uses LF line endings, four-space indentation, and
one final newline for nonempty files while preserving literal, character, and
comment contents.

The required fields are `schema_version`, `compiler_version`, `language`,
`file`, `position_encoding`, `line_ending`, `indent_width`, `insert_spaces`,
`source_bytes`, `formatted_bytes`, `changed`, and `formatted_text`.
Formatting the result again must be idempotent. See
[`FORMAT_JSON_SCHEMA.md`](FORMAT_JSON_SCHEMA.md) for the normative shape,
error mapping, and consumer rules.

### 7.4 `semantic-query-json-v1`

`--semantic-query=json --position-byte=<offset>` accepts exactly one Baa source
and implies check-only frontend analysis. It may consume the current unsaved
buffer through `--source-stdin=<logical-file>`. The position is a zero-based
absolute UTF-8 byte offset; values past the end of the buffer are clamped to
the buffer length.

The compiler resolves the node at the cursor to its analyzed declaration, so
block shadowing, fields, enum members, function pointers, and included
prototypes use Baa semantics rather than an editor-side text search. It also
finds the enclosing call and calculates the active argument while respecting
nested delimiters, strings, character literals, comments, and both supported
comma forms.

The root file identity in semantic query and index output preserves the exact
logical path supplied through `--source-stdin`. Resolved filesystem files such
as includes and dependencies use compiler-canonical paths; on Windows, those
paths expand 8.3 aliases to their stable long Unicode form.

```json
{
  "schema_version": "semantic-query-json-v1",
  "compiler_version": "0.6.0",
  "file": "/logical/path/main.baa",
  "position_encoding": "utf-8-bytes",
  "position_byte": 42,
  "symbol": {
    "domain": "external",
    "kind": "function",
    "name": "اجمع"
  },
  "hover": {
    "name": "اجمع",
    "kind": "function",
    "display": "صحيح اجمع(صحيح أول، صحيح ثان)",
    "description": "دالة",
    "range": {
      "start": { "line": 5, "column": 11, "byte": 37 },
      "end": { "line": 5, "column": 15, "byte": 45 }
    },
    "declaration": {
      "file": "/logical/path/main.baa",
      "line": 1,
      "column": 6
    }
  },
  "signature_help": {
    "name": "اجمع",
    "label": "صحيح اجمع(صحيح أول، صحيح ثان)",
    "active_parameter": 1,
    "variadic": false,
    "parameters": [
      { "label": "صحيح أول" },
      { "label": "صحيح ثان" }
    ],
    "declaration": {
      "file": "/logical/path/main.baa",
      "line": 1,
      "column": 6
    }
  },
  "definition": {
    "file": "/logical/path/main.baa",
    "name": "اجمع",
    "kind": "function",
    "range": {
      "start": { "line": 1, "column": 6, "byte": 5 },
      "end": { "line": 1, "column": 10, "byte": 13 }
    }
  },
  "references": [
    {
      "file": "/logical/path/main.baa",
      "name": "اجمع",
      "kind": "function",
      "role": "declaration",
      "range": {
        "start": { "line": 1, "column": 6, "byte": 5 },
        "end": { "line": 1, "column": 10, "byte": 13 }
      }
    }
  ],
  "completion": {
    "items": [
      {
        "label": "قيمة",
        "kind": "variable",
        "detail": "صحيح قيمة",
        "documentation": "متغير في باء",
        "filter_text": "قيمة",
        "insert_text": "قيمة",
        "insert_text_format": "plain",
        "scope": "local"
      }
    ]
  }
}
```

`symbol`, `hover`, `signature_help`, and `definition` are independently `null`
when no valid result exists. `symbol` is the compiler-owned structured identity
of the selected declaration. `references` is always an array and contains only AST
declarations and uses bound to the selected compiler declaration; equal text in
comments, strings, or a shadowing scope is not a reference. The current
contract returns references from the analyzed translation unit, including
explicitly included headers. Project fan-out compares this identity only with
identities emitted by `semantic-index-json-v1`; consumers must not fall back to
identifier text matching.

`completion` is always an object with an `items` array. It contains declarations
visible at `position_byte`: parameters, declarations that precede the cursor in
the active lexical scope, root globals and types, and declarations from
explicitly included headers. Inner declarations shadow outer declarations;
future declarations and sibling-block locals are excluded. Each item requires
`label`, `kind`, `detail`, `documentation`, `filter_text`, `insert_text`,
`insert_text_format`, and `scope`. Stable scope values are `local`,
`parameter`, `global`, and `included`.

Root-buffer ranges use one-based byte line/column coordinates plus zero-based
absolute byte offsets; consumers should use the byte offsets when converting
to LSP UTF-16. Included-file ranges omit absolute `byte` members because their
offsets belong to a different source buffer; consumers convert their one-based
byte line/column coordinates using that file's UTF-8 contents.

Cursor queries are editing operations, so a query may still return structured
data and scope-aware completion when the current buffer has an incomplete call
or a temporary semantic error such as the empty call inserted by automatic
parenthesis pairing.
Unsupported or unresolved results remain explicit `null` values; the compiler
does not invent declarations or parse identifier text outside its frontend.

### 7.5 `semantic-index-json-v1`

`--semantic-index=json` accepts exactly one successfully analyzed Baa
translation unit and implies check-only frontend analysis. `-I` and
`--source-stdin=<logical-file>` have the same meaning as in the other editor
contracts. The output lists only AST declarations and bound uses:

```json
{
  "schema_version": "semantic-index-json-v1",
  "compiler_version": "0.6.0",
  "file": "/project/source/main.baa",
  "position_encoding": "utf-8-bytes",
  "occurrences": [
    {
      "symbol": {
        "domain": "external",
        "kind": "function",
        "name": "اجمع"
      },
      "role": "reference",
      "location": {
        "file": "/project/source/main.baa",
        "name": "اجمع",
        "kind": "function",
        "range": {
          "start": { "line": 5, "column": 11, "byte": 37 },
          "end": { "line": 5, "column": 15, "byte": 45 }
        }
      }
    }
  ]
}
```

Roles are `definition`, `declaration`, or `reference`. External functions and
non-static globals use an identity stable across translation units. File-local
and local identities include their compiler declaration location; type-like
declarations use their defining location. Baa-LSP may aggregate equal
structured identities across the exact source closure supplied by Takween.
The compiler-owned `kind` distinguishes `function`, `variable`, `constant`,
`array`, `parameter`, `field`, `enum-member`, `type-alias`, `enum`, `struct`,
and `union`. The same kind is emitted in `symbol` and `location`, including for
each bound parameter or field reference; adapters must not infer these roles
from spelling or punctuation.
Duplicate header occurrences are deduplicated by location. Invalid source
returns source exit code `1` rather than a partial index.

### 7.6 `structure-json-v1`

`--dump-structure=json` accepts exactly one saved Baa source or one unsaved
buffer through `--source-stdin=<logical-file>`. It performs tolerant raw-source
scanning only: preprocessing, semantic analysis, IR, and code generation do not
run. Valid UTF-8 input returns exit code `0` even when delimiters or literals are
temporarily incomplete; `complete` then becomes `false`.

`folding_ranges` contains multiline delimiter regions and comment regions.
`selection_ranges` contains exact token, trimmed-line, delimiter-content,
delimiter-group, brace-construct, and whole-document candidates. Every span
uses one-based UTF-8 byte line/column coordinates plus zero-based absolute byte
offsets. Arrays are deterministically sorted by start offset, then decreasing
end offset, then kind, with duplicates removed.

Baa-LSP validates the full contract and converts byte positions to UTF-16 for
`textDocument/foldingRange` and `textDocument/selectionRange`. Consumers must
discard stale results and must not recover hidden structure by parsing source
text after a contract failure. The normative shape and kind vocabulary are in
[`STRUCTURE_JSON_SCHEMA.md`](STRUCTURE_JSON_SCHEMA.md).

---

## 8. Contract Versioning

| Change | Required action |
|---|---|
| Add optional field | no version bump required |
| Add required field | minor contract version bump |
| Remove field | major contract version bump |
| Change field meaning | major contract version bump |
| Change diagnostic code meanings | major contract version bump |

---

## 9. Tool Responsibilities

Takween should consume:

- exit codes,
- build manifest,
- include/dependency hashes,
- diagnostic JSON,
- target support list.

Qalam should consume:

- `--check`,
- diagnostic JSON,
- token JSON,
- symbol JSON,
- `completion-data-json-v1`,
- `format-json-v1`,
- `semantic-query-json-v1`.
- `semantic-index-json-v1`.

Nazm integration should consume:

- canonical Arabic assembly text emitted after Baa register allocation,
- explicit target and object-format selection,
- source-map metadata for generated assembly diagnostics, and
- a complete generated-form inventory from Baa's Windows/Linux test corpus,
- `baa-nazm-shadow-corpus-v1`, with one emitted/unsupported/error result for
  every inventoried source and target.

Baa also accepts direct Arabic `.نظم` source roots in `-c` and normal hosted
link invocations. Those roots bypass Baa parsing and Machine IR, invoke Nazm
through structured argv by default, and join the same object/link plan as
`.baa` roots.
Direct Nazm source diagnostics return source status `1`; missing or failed
process/tool execution returns `4`. `--check`, `-S`, `--emit-nazm`, and
`--nazm-shadow` reject direct `.نظم` roots explicitly until Nazm exposes the
required JSON validation/source-map contracts. No failure retries through GAS.

For compiler-generated Nazm sources, the selected Nazm CLI must accept the
Arabic `--اسم-المصدر` option. Baa passes a stable logical identity while the
physical generated source remains process-unique; this prevents temporary
paths from entering deterministic COFF object metadata. Direct `.نظم` roots
retain their user-provided identity.

`baa-nazm-boundary-v0` is admitted for the checked Baa/Nazm/Takween revisions.
Baa's public `-S` output is canonical Arabic `.نظم` by default, and production
object generation invokes Nazm. GAS/AT&T remains available through explicit
`--assembler=gas`; the shadow path selects a measured GAS comparison leg and
cannot hide an unsupported form behind silent fallback.

An opt-in build may link Nazm and expose the Arabic runtime selector
`--نظم-داخل-العملية`. CMake keeps this disabled unless
`BAA_ENABLE_EMBEDDED_NAZM=ON` points at a source tree implementing
`nazm-api-v1`. The API path preserves the canonical emitted Arabic text,
returns owned object bytes and structured diagnostics, and is tested against
the CLI for exact ELF64/COFF object bytes and matching primary failures. It is
not the production default: omitting the selector always retains the separate
Nazm process, and explicit GAS remains the compiler-level rollback.

PyramidOS experiments should consume:

- target specs,
- freestanding mode,
- object-only output,
- ABI/layout test outputs.
