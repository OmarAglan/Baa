# Baa Source Tokens JSON Schema

> **Schema:** `tokens-json-v1`  
> **Producer:** Baa reference compiler  
> **Position encoding:** UTF-8 byte offsets

`--dump-tokens=json` exposes the compiler-owned lexical view used by Baa-LSP and
Qalam for stable semantic highlighting. It accepts either one saved Baa source
or one unsaved UTF-8 buffer:

```text
baa --dump-tokens=json source.baa
baa --dump-tokens=json --source-stdin=source.baa
```

The mode is deliberately tolerant of incomplete editor text. It validates
UTF-8 and scans the supplied source without parsing, semantic analysis, include
expansion, macro expansion, assembly, or linking. Comments and directives
therefore remain visible to editor tooling exactly where the author wrote
them.

## Document shape

```json
{
  "schema_version": "tokens-json-v1",
  "compiler_version": "0.6.0",
  "language": "baa",
  "file": "source.baa",
  "position_encoding": "utf-8-bytes",
  "source_bytes": 42,
  "tokens": [
    {
      "kind": "type",
      "span": {
        "start": {"line": 1, "column": 1, "byte": 0},
        "end": {"line": 1, "column": 9, "byte": 8}
      }
    }
  ]
}
```

`line` and `column` are one-based UTF-8 byte coordinates. `byte` is a
zero-based offset in the exact input buffer. Spans are half-open:
`start.byte <= position < end.byte`. `source_bytes` must equal the input byte
length.

Stable token kinds are:

| Kind | Meaning |
|---|---|
| `type` | primitive Baa type keyword |
| `modifier` | declaration or storage modifier |
| `keyword` | other compiler-owned Baa keyword |
| `identifier` | identifier not classified as a keyword |
| `number` | numeric literal |
| `string` | string literal |
| `character` | character literal |
| `comment` | line or block comment |
| `directive` | source directive beginning with `#` |
| `operator` | operator or separator |

Whitespace is not emitted. A multiline string, character, comment, or
directive remains one source token; LSP adapters must split it into
single-line semantic tokens when required by the protocol.

Invalid UTF-8 or another source error returns compiler exit code `1`. Invalid
mode combinations or a non-Baa input return exit code `2`. The compiler writes
one JSON document to standard output only on success and never silently falls
back to a different token source.
