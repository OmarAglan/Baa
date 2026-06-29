# Baa Conformance Suite

> **Version:** draft-0.1 | **Contract:** `conformance-v1`

This document defines the formal test suite that locks the meaning of Baa.

---

## 1. Purpose

Compiler tests prove one implementation works. A conformance suite defines what the language is.

The suite is used by:

- the C reference compiler,
- future compiler rewrites or bootstrap experiments,
- Takween build validation,
- Qalam editor diagnostics validation,
- target/backend validation,
- PyramidOS freestanding experiments later.

---

## 2. Directory Layout

```text
tests/conformance/
  syntax/
  semantics/
  diagnostics/
  stdlib/
  abi/
  targets/
  runtime/
  negative/
  README.md
```

---

## 3. Test Metadata

Each `.baa` test may include metadata comments:

```baa
// RUN: expect-pass
// FLAGS: -O2 --verify
// EXPECT-OUT: ٣٠
// EXPECT-EXIT: 0

صحيح الرئيسية() {
    اطبع ١٠ + ٢٠.
    إرجع ٠.
}
```

Negative test example:

```baa
// RUN: expect-fail
// FLAGS: --check --diagnostics=json
// EXPECT: B0001
// EXPECT-DIAG-COUNT: 1

صحيح الرئيسية() {
    صحيح س = ١٠
    إرجع ٠.
}
```

---

## 4. Required Categories

| Category | Purpose |
|---|---|
| syntax | accepted/rejected grammar |
| semantics | type, scope, const, pointer, aggregate rules |
| diagnostics | stable diagnostic IDs, spans, hints, cascade control |
| stdlib | string, memory, file, math, error behavior |
| abi | calls, returns, varargs, stack args, struct layout |
| targets | hosted/freestanding target behavior |
| runtime | optional runtime checks and panic behavior |
| negative | intentionally invalid programs |

---

## 5. Versioning

Conformance suite versions track language contracts, not compiler implementation versions.

| Suite | Meaning |
|---|---|
| `conformance-v0` | current tests, not fully stable |
| `conformance-v1` | v0.9 stable beta language baseline |
| `freestanding-v0` | post-v0.9 experimental OS profile tests |

---

## 6. Release Gate

A Baa release cannot be called stable unless:

- conformance tests pass on Windows and Linux,
- `-O0` and `-O2` agree where applicable,
- diagnostic tests pass with stable codes,
- ABI tests pass for supported targets,
- stdlib tests pass for hosted targets,
- freestanding-only tests are skipped unless the target supports them.

---

## 7. Initial Seed Tests

Start with a small but complete seed:

- minimal program,
- arithmetic and conversions,
- control flow,
- arrays,
- structs/unions/enums,
- pointers,
- function pointers,
- stdlib string/memory/file basics,
- invalid syntax missing `.`,
- invalid type assignment,
- invalid include cycle,
- target rejects unsupported mode.

---

## 8. Rule

When a language behavior changes, update the conformance test in the same commit as the compiler change.
