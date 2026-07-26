# Baa Formatting JSON Schema

> **Version:** draft-0.1 | **Schema:** `format-json-v1`

This document defines Baa's compiler-owned source-formatting contract for
language servers and editors.

---

## 1. CLI

```bash
baa --format=json source.baa
baa --format=json --source-stdin=<logical-file>
```

The command accepts exactly one UTF-8 Baa source, either as a positional path
or through standard input with a logical source identity. It writes one JSON
object to standard output and performs no preprocessing, semantic analysis, IR
lowering, assembly, or linking.

`--format=json` is an exclusive machine-readable mode. Compilation modes,
output paths, build manifests, caches, and Nazm inputs are rejected with
`compiler-cli-v1` invocation status `2`.

---

## 2. Top-Level Shape

```json
{
  "schema_version": "format-json-v1",
  "compiler_version": "0.7.2",
  "language": "baa",
  "file": "/project/رئيسي.baa",
  "position_encoding": "utf-8-bytes",
  "line_ending": "lf",
  "indent_width": 4,
  "insert_spaces": true,
  "source_bytes": 35,
  "formatted_bytes": 43,
  "changed": true,
  "formatted_text": "صحيح الرئيسية() {\n    إرجع ٠.\n}\n"
}
```

All fields are required.

| Field | Meaning |
|---|---|
| `schema_version` | Exact value `format-json-v1`. |
| `compiler_version` | Version of the Baa reference compiler that formatted the source. |
| `language` | Exact value `baa`. |
| `file` | Positional path or logical path supplied by `--source-stdin`. |
| `position_encoding` | Exact value `utf-8-bytes`; byte counts refer to encoded source bytes. |
| `line_ending` | Exact value `lf`. |
| `indent_width` | Canonical indentation width, currently `4`. |
| `insert_spaces` | Always `true` in v1. |
| `source_bytes` | Byte length of the input source. |
| `formatted_bytes` | Byte length of `formatted_text`. |
| `changed` | Whether the formatted UTF-8 bytes differ from the input bytes. |
| `formatted_text` | Complete canonical replacement text. |

---

## 3. Canonical Behavior

The v1 formatter:

- validates UTF-8 and removes an initial UTF-8 BOM;
- normalizes line endings to LF and gives a nonempty file one final newline;
- uses four spaces per block level;
- normalizes safe whitespace around Baa punctuation, operators, delimiters,
  declarations, directives, and initializers;
- preserves string literals, character literals, comments, and their contents;
- accepts incomplete editor buffers without requiring a successful parse; and
- is idempotent: formatting `formatted_text` again returns the same text with
  `changed: false`.

The formatter is deliberately lexical and source preserving. It does not
expand includes or macros, reorder declarations, rewrite identifiers, or
change literal/comment content. Future AST transformations require a new or
explicitly extended contract; they must not be introduced silently.

---

## 4. Errors and Consumer Rules

Invalid UTF-8 returns source status `1` and emits no partial JSON. Invalid CLI
combinations return invocation status `2`. Allocation or output failures return
internal status `5`.

An LSP adapter should snapshot the document version, request formatting through
`--source-stdin`, and convert the complete `formatted_text` into one
full-document UTF-16 `TextEdit`. It must reject the result if the document
version changes. Client-provided tab size and space preferences do not override
Baa's canonical v1 style.
