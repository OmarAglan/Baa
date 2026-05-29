# Mixed C+Baa Harness

> **Version:** 0.9.1 | [← Stage-0 Manifest](STAGE0_BOOTSTRAP_MANIFEST.md) | [Self-Hosting Pilot →](SELF_HOSTING_PILOT_REPORT.md)

This document defines the v0.9.1 harness used to validate compiler slices built from a mix of C and Baa objects.

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
python scripts/qa_mixed_harness.py --target lexer-diagnostics
```

The legacy pilot command remains valid:

```bash
python scripts/qa_selfhost_pilot.py
```

It delegates to `qa_mixed_harness.py --target token-names`.

---

## 2. Bridge Boundary

The harness owns mixed-unit orchestration for Phase 5 bootstrap work. The compiler driver CLI is unchanged in v0.9.1.

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

The v0.9.1 Baa lexer candidate slot compiles `src/frontend/lexer_candidate_baa0.baa`, links it with a C host harness, and calls `مرشح_المحلل_اللفظي_شغل` for every fixture. The current candidate is a bridge checkpoint: C still owns file I/O, include resolution, baseline token emission, and memory cleanup, while the Baa object proves the reversible C→Baa→C ABI path before lexer state is moved into Baa.

### `lexer-diagnostics`

Runs malformed lexer/preprocessor negative fixtures through the current compiler and compares normalized diagnostics against committed text snapshots under:

```text
tests/snapshots/mixed_harness/lexer_diagnostics/
```

This target freezes source locations, include-cycle reporting, escape diagnostics, and preprocessor EOF diagnostics for the lexer rewrite path.

The token-stream corpus also includes `tests/stress/stress_utf8_identifiers.baa` so the candidate bridge is checked against a stress-sized UTF-8 lexer fixture.

---

## 4. Snapshot Policy

Regenerate snapshots only after an intentional lexer behavior change:

```bash
python scripts/qa_mixed_harness.py --target lexer-token-stream --update-snapshots
python scripts/qa_mixed_harness.py --target lexer-diagnostics --update-snapshots
```

Unexpected snapshot diffs are regressions until reviewed against the frozen Phase 4.5 contracts.

---

*[← Stage-0 Manifest](STAGE0_BOOTSTRAP_MANIFEST.md) | [Self-Hosting Pilot →](SELF_HOSTING_PILOT_REPORT.md)*
