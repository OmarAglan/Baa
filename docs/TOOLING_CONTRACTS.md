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
| `--emit-build-manifest <file>` | dependency/cache manifest | Takween |
| `--incremental` | reuse cached object files when safe | Takween |
| `--cache-dir <dir>` | select the incremental cache directory | Takween |
| `--diagnostics=json` | machine-readable diagnostics | Qalam, Takween |
| `--dump-tokens=json` | stable token stream | Qalam/debug tools |
| `--dump-symbols=json` | symbol outline | Qalam |
| `--target=<target>` | target selection | Takween, OS experiments |
| `--target-info=json` | host target, executable suffix, object format, and capabilities | Takween, CI |
| `-I <dir>` / `-I<dir>` | include search path | Takween, users |

---

## 3. Takween Invocation Contract

Takween may rely on these compiler invocation shapes:

```bash
baa --check [-I <dir>...] [--target=<target>] <inputs...>
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
  "assembler": "gas",
  "opt_level": 2,
  "runtime_checks": false,
  "runtime_check_mask": 0,
  "incremental": true,
  "units": [
    {
      "source": "/abs/path/src/main.baa",
      "output": "/abs/path/build/main.o",
      "source_kind": "baa",
      "assembler": "gas",
      "cache": {
        "enabled": true,
        "hit": false,
        "slot": "...",
        "reason": "miss"
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
`nazm`). Direct `.نظم` units always report `nazm`, even when Baa units in the
same link retain the default GAS policy. Generated and direct Nazm objects
currently bypass the incremental object cache until the cache key records a
verified Nazm version fingerprint; the manifest therefore reports
`cache.enabled: false` rather than reusing an object produced by a different
assembler.

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
- completion metadata.

Nazm integration should consume:

- canonical Arabic assembly text emitted after Baa register allocation,
- explicit target and object-format selection,
- source-map metadata for generated assembly diagnostics, and
- a complete generated-form inventory from Baa's Windows/Linux test corpus,
- `baa-nazm-shadow-corpus-v1`, with one emitted/unsupported/error result for
  every inventoried source and target.

Baa also accepts direct Arabic `.نظم` source roots in `-c` and normal hosted
link invocations. Those roots bypass Baa parsing and Machine IR, invoke Nazm
through structured argv, and join the same object/link plan as `.baa` roots.
Direct Nazm source diagnostics return source status `1`; missing or failed
process/tool execution returns `4`. `--check`, `-S`, `--emit-nazm`, and
`--nazm-shadow` reject direct `.نظم` roots explicitly until Nazm exposes the
required JSON validation/source-map contracts. No failure retries through GAS.

For compiler-generated Nazm sources, the selected Nazm CLI must accept the
Arabic `--اسم-المصدر` option. Baa passes a stable logical identity while the
physical generated source remains process-unique; this prevents temporary
paths from entering deterministic COFF object metadata. Direct `.نظم` roots
retain their user-provided identity.

Until `baa-nazm-boundary-v0` is admitted, Baa's public `-S` output remains
GAS/AT&T and production object generation continues through the external host
assembler. A Nazm shadow path must be opt-in, non-default, and unable to hide an
unsupported form behind silent fallback.

PyramidOS experiments should consume:

- target specs,
- freestanding mode,
- object-only output,
- ABI/layout test outputs.
