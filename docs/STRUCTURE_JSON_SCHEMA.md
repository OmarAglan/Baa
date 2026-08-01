# Baa Structure JSON Schema

> **Version:** draft-0.1 | **Schema:** `structure-json-v1`

`structure-json-v1` is Baa's tolerant structural-editing contract. It lets
Baa-LSP provide standard folding and semantic-selection requests without
duplicating the language scanner in the server or IDE.

## Invocation

```text
baa --dump-structure=json <source.baa>
baa --dump-structure=json --source-stdin=<logical-file>
```

The mode accepts exactly one source identity. It cannot be combined with
another machine-readable output mode, a Nazm source, or a compilation mode.
The stdin form reads the exact unsaved UTF-8 buffer and never writes a shadow
source file.

## Document shape

```json
{
  "schema_version": "structure-json-v1",
  "compiler_version": "0.7.2",
  "language": "baa",
  "file": "/project/source/main.baa",
  "position_encoding": "utf-8-bytes",
  "source_bytes": 84,
  "complete": true,
  "folding_ranges": [
    {
      "kind": "region",
      "span": {
        "start": { "line": 1, "column": 24, "byte": 33 },
        "end": { "line": 4, "column": 2, "byte": 83 }
      }
    }
  ],
  "selection_ranges": [
    {
      "kind": "token",
      "span": {
        "start": { "line": 2, "column": 11, "byte": 48 },
        "end": { "line": 2, "column": 13, "byte": 50 }
      }
    }
  ]
}
```

All top-level fields are required. `source_bytes` is the exact byte length of
the UTF-8 input. `file` is the positional path or the logical stdin identity.
`complete` is `false` when the tolerant scanner observes unmatched or
mismatched delimiters, an unterminated block comment, or an unterminated
literal. Incomplete valid UTF-8 is an editing state, so the compiler returns
exit code `0` and preserves every range it can prove.

## Spans

Every range has one required `kind` and one required half-open `span`.

- `line` and `column` are one-based UTF-8 byte coordinates.
- `byte` is a zero-based absolute UTF-8 byte offset.
- `start.byte < end.byte <= source_bytes`.
- Both byte offsets are UTF-8 code-point boundaries.
- The line/column coordinates must describe the same offsets as the byte
  members.

Each array is sorted by `start.byte` ascending, `end.byte` descending, then
`kind` ascending. Identical kind/span records are removed. Consumers must
validate this canonical ordering instead of silently sorting malformed output.

## Folding kinds

`folding_ranges` contains only multiline ranges:

- `region`: a matched multiline `()`, `[]`, or `{}` group.
- `comment`: a multiline block comment or a consecutive run of at least two
  line comments.

The scanner owns comment and literal state. Delimiter-looking characters inside
comments, string literals, or character literals never produce folds.

## Selection kinds

`selection_ranges` is a deterministic candidate inventory:

- `token`: one raw source token.
- `line`: the nonblank, horizontally trimmed contents of one source line.
- `content`: the bytes strictly between a matched delimiter pair.
- `group`: a matched delimiter pair and its contents.
- `construct`: a brace group extended to the first non-horizontal-space byte
  on its opening line.
- `document`: the complete nonempty source buffer.

Candidates can overlap without nesting. Baa-LSP selects only candidates that
contain the requested UTF-16 cursor and builds a strictly expanding parent
chain for `textDocument/selectionRange`.

## Errors and consumer rules

- Exit `0`: a valid contract document was emitted, including partial structure
  with `complete: false`.
- Exit `1`: unreadable or invalid UTF-8 source.
- Exit `2`: invalid or conflicting invocation.
- Exit `5`: allocation or output failure.

Baa-LSP must validate field types, vocabulary, byte bounds, UTF-8 boundaries,
coordinates, ordering, and source length before caching a result. It converts
accepted spans to LSP UTF-16 positions, coalesces folding and selection work for
the same document version, and rejects cancelled or stale results. A contract
failure is visible; the adapter must not parse Baa source to manufacture a
replacement result.
