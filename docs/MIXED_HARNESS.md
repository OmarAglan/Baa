# Mixed C+Baa Harness

> **Version:** 0.9.1.5 | [← Stage-0 Manifest](STAGE0_BOOTSTRAP_MANIFEST.md) | [Self-Hosting Pilot →](SELF_HOSTING_PILOT_REPORT.md)

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
python scripts/qa_mixed_harness.py --target lexer-dependencies
python scripts/qa_mixed_harness.py --target lexer-diagnostics
python scripts/qa_mixed_harness.py --target lexer-transition
python scripts/qa_mixed_harness.py --target lexer-state
```

The legacy pilot command remains valid:

```bash
python scripts/qa_selfhost_pilot.py
```

It delegates to `qa_mixed_harness.py --target token-names`.

The production Baa-backed lexer bridge is built through CMake, not the mixed harness:

```powershell
cmake -B build -G "MinGW Makefiles" -DBAA_BOOTSTRAP_COMPILER="path\to\baa.exe"
cmake --build build
```

The production compiler no longer contains a C lexer fallback. `BAA_BOOTSTRAP_COMPILER` must point at an existing `baa` compiler so CMake can compile `src/frontend/lexer_state_baa0.baa` into the final compiler.

Normal QA owns the promoted production lexer signoff in `tests/integration/backend/backend_lexer_production_signoff_test.baa` and focused negative preprocessor diagnostics under `tests/neg/`. The mixed harness now keeps the remaining lexer migration checks stable through committed token-stream, dependency, and diagnostic snapshots.

---

## 2. Bridge Boundary

The harness owns mixed-unit orchestration for Phase 5 bootstrap work. The compiler driver CLI is unchanged in v0.9.1.

Current bridge contract:

- Baa compiler slices are compiled to native objects with `baa -O2 --verify-ir --verify-ssa -c`.
- C harness units are compiled with the host C compiler using GNU11.
- C and Baa objects are linked by the host C compiler into a parity executable.
- Baa symbols exposed to C use the platform object ABI produced by the current backend.
- Baa `نص` values crossing into C are decoded from the current packed `حرف` cell representation before comparison.
- Baa-owned lexer-state slices accept caller-owned `ط٨*` buffers and return scalar token metadata through C-owned `صحيح*` out parameters.
- By default, `src/frontend/lexer_baa_bridge.c` links `src/frontend/lexer_state_baa0.baa` into the real compiler and preserves the existing parser-facing `lexer_init`/`lexer_next_token`/cleanup contract.

---

## 3. Targets

### `token-names`

Compiles `src/frontend/lexer_token_names_baa0.baa`, links it with a generated C parity harness and `src/frontend/lexer_debug.c`, and compares every `BaaTokenType` name against the C baseline.

### `lexer-token-stream`

Compiles `src/frontend/lexer_state_baa0.baa`, links it with a C host harness, and compares fixed fixtures against committed JSONL snapshots under:

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

The harness-facing declarations live in `src/frontend/lexer_state_baa0.baahd`. C still owns fixture file reads, snapshot comparison, and token-name formatting, but cursor movement, token classification, conditional preprocessing, include-source stack switching, macro value substitution, and Arabic-Indic numeric value-mode parity are exercised through Baa before comparison against the committed snapshots.

### `lexer-dependencies`

Compiles the Baa scanner-state candidate and compares normalized dependency JSONL rows for the token-stream fixtures against committed snapshots under:

```text
tests/snapshots/mixed_harness/lexer_dependencies/
```

This locks the migration boundary for root source paths and discovered include paths without changing the production `lexer_get_dependencies` API.

### `lexer-diagnostics`

Runs malformed lexer/preprocessor negative fixtures through the current compiler and compares normalized diagnostics against committed text snapshots under:

```text
tests/snapshots/mixed_harness/lexer_diagnostics/
```

This target freezes source locations, include-cycle reporting, malformed literal diagnostics, unknown-byte diagnostics, escape diagnostics, and preprocessor EOF diagnostics for the lexer rewrite path.

The target also compiles the Baa scanner-state candidate and runs structured diagnostic smoke checks for bad string escapes, bad char escapes, unclosed string literals, unclosed char literals, unknown bytes, unclosed `#إذا_عرف`, unclosed `#إذا_لم_يعرف`, and include-cycle detection. The Baa side returns diagnostic code and source location through a mixed-harness-only ABI; the C harness maps host include failures separately where path resolution owns the include-chain decision.

The token-stream corpus also includes `tests/stress/stress_utf8_identifiers.baa` so the candidate bridge is checked against a stress-sized UTF-8 lexer fixture.

### `lexer-transition`

Compiles `src/frontend/lexer_transition_baa0.baa`, links it with a generated C harness, and runs a small non-production transition slice. This target proves that Baa0 can now consume the lexer-readability surface required before the full v0.9.1.5 migration: `ط٨*`, `خام"..."`, `طول_خام`, `قارن_خام`, `قارن_خام_بطول`, pointer arithmetic, and `هيكل* -> عضو`.

### `lexer-state`

Compiles `src/frontend/lexer_state_baa0.baa`, links it with a generated C harness, and drives a Baa-owned scanner state over a caller-owned UTF-8 byte buffer. This is the first v0.9.1.5 path where Baa owns cursor, line, and column movement and returns token metadata through C-owned out parameters. It currently covers EOF, whitespace/newline skipping, simple punctuation, Arabic semicolon, and multi-character operator tokens; the production `lexer_next_token` path is unchanged.

The target also drives snapshot-backed real fixtures through the Baa-owned scanner-state path and verifies token type, byte start, byte length, line, column, and token value parity. It currently covers `basic_utf8.baa`, `tests/fixtures/mixed_harness/lexer/char_raw_literals.baa`, `tests/stress/stress_utf8_identifiers.baa`, and `conditional_macros.baa`, including line comments, Arabic keywords/identifiers, Arabic-Indic digits, strings, char literals, raw byte strings, UTF-8 byte accounting, and source-local conditional skipping. The promoted `lexer-token-stream` Baa-state candidate covers `#تضمين`, macro value substitution from included headers, and `#إذا_لم_يعرف` parity against JSONL snapshots. Mixed-harness-only scanner ABIs return either source-relative value spans or raw value pointers so the C harness can reconstruct snapshot values, including Arabic-Indic digit normalization for direct integer literals. The opt-in production bridge now copies those Baa spans into heap-owned `Token.value` strings for the existing parser API.

## 4. Ownership Boundary

The mixed-harness Baa scanner-state boundary is intentionally non-production. The default production bridge uses the same scanner behind the parser-facing C API and takes C ownership of parser-visible heap strings, included source buffers, dependency paths, diagnostics, and cleanup. Included source buffers must also be registered with the diagnostic subsystem so source-line rendering matches production diagnostics.

| Data | Owner | Contract |
| --- | --- | --- |
| Root source buffer | C harness | Allocated by the harness, passed to `محللباءهيئ`, released after `محللباءنظف`. |
| Baa lexer-state header | Baa source | `src/frontend/lexer_state_baa0.baahd` declares the scanner-state ABI consumed by the production build and mixed harness. |
| Included source buffers | C harness | Resolved, dependency-recorded, and released by the host include service. |
| Token value text | C harness | Reconstructed from Baa-returned spans/pointers; Baa does not allocate token strings. |
| Macro names/values | Baa scanner state | Stored as borrowed spans into active source buffers; valid until scanner cleanup. |
| Dependency paths | C harness / production bridge | Copied into C-owned arrays for comparison and freed by the owning C side. |
| Diagnostic text | C harness / compiler diagnostics | Baa returns structured code/location only; Arabic message text stays in the C diagnostic layer until production replacement. |

---

## 5. Snapshot Policy

Regenerate snapshots only after an intentional lexer behavior change:

```bash
python scripts/qa_mixed_harness.py --target lexer-token-stream --update-snapshots
python scripts/qa_mixed_harness.py --target lexer-dependencies --update-snapshots
python scripts/qa_mixed_harness.py --target lexer-diagnostics --update-snapshots
```

Unexpected snapshot diffs are regressions until reviewed against the frozen Phase 4.5 contracts.

---

*[← Stage-0 Manifest](STAGE0_BOOTSTRAP_MANIFEST.md) | [Self-Hosting Pilot →](SELF_HOSTING_PILOT_REPORT.md)*
