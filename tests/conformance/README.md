# Baa Conformance Tests

This folder is the seed location for the formal Baa conformance suite.

## Folders

| Folder | Purpose |
|---|---|
| `syntax/` | grammar acceptance/rejection |
| `semantics/` | type/scope/const/pointer rules |
| `diagnostics/` | stable diagnostic codes/spans/hints |
| `stdlib/` | hosted standard library behavior |
| `abi/` | calls, returns, layout, varargs, target ABI |
| `targets/` | target/freestanding/hosted behavior |

## Metadata

Use the same style already supported by the Baa test runners where possible:

```baa
// RUN: expect-pass
// FLAGS: -O2 --verify
// EXPECT-OUT: ٣٠
// EXPECT-EXIT: 0
```

Negative diagnostics:

```baa
// RUN: expect-fail
// FLAGS: --check --diagnostics=json
// EXPECT: B0001
// EXPECT-DIAG-COUNT: 1
```

## Initial rule

Every new language feature after v0.5.9 must add or update conformance tests.
