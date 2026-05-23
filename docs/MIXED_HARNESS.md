# Mixed C+Baa Harness

> **Version:** 0.9.0.2 | [← Stage-0 Manifest](STAGE0_BOOTSTRAP_MANIFEST.md) | [Self-Hosting Pilot →](SELF_HOSTING_PILOT_REPORT.md)

This document defines the v0.9.0.2 harness used to validate compiler slices built from a mix of C and Baa objects.

---

## 1. Command

Run all mixed-harness targets:

```bash
python scripts/qa_mixed_harness.py --target all
```

Useful focused targets:

```bash
python scripts/qa_mixed_harness.py --target token-names
python scripts/qa_mixed_harness.py --target lexer-token-stream
```

The legacy pilot command remains valid:

```bash
python scripts/qa_selfhost_pilot.py
```

It delegates to `qa_mixed_harness.py --target token-names`.

---

## 2. Bridge Boundary

The harness owns mixed-unit orchestration for Phase 5 bootstrap work. The compiler driver CLI is unchanged in v0.9.0.2.

Current bridge contract:

- Baa compiler slices are compiled to native objects with `baa -O2 --verify-ir --verify-ssa -c`.
- C harness units are compiled with the host C compiler using GNU11.
- C and Baa objects are linked by the host C compiler into a parity executable.
- Baa symbols exposed to C use the platform object ABI produced by the current backend.
- Baa `نص` values crossing into C are decoded from the current packed `حرف` cell representation before comparison.

---

## 3. Targets

### `token-names`

Compiles `src/frontend/lexer_token_names_baa0.baa`, links it with a generated C parity harness and `src/frontend/lexer_debug.c`, and compares every `BaaTokenType` name against the C baseline.

### `lexer-token-stream`

Builds a C lexer-token dump harness from the current C lexer and compares fixed fixtures against committed JSONL snapshots under:

```text
tests/snapshots/mixed_harness/lexer_token_stream/
```

Each row records:

- fixture path,
- token index,
- token numeric type and visible name,
- token value,
- line, column, and byte length,
- normalized token filename.

The Baa lexer candidate slot is intentionally reported as `not-ready` in v0.9.0.2. v0.9.1 will add the first Baa lexer-token-stream candidate.

---

## 4. Snapshot Policy

Regenerate snapshots only after an intentional lexer behavior change:

```bash
python scripts/qa_mixed_harness.py --target lexer-token-stream --update-snapshots
```

Unexpected snapshot diffs are regressions until reviewed against the frozen Phase 4.5 contracts.

---

*[← Stage-0 Manifest](STAGE0_BOOTSTRAP_MANIFEST.md) | [Self-Hosting Pilot →](SELF_HOSTING_PILOT_REPORT.md)*
