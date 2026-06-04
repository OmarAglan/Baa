# Baa Self-Hosting Pilot Report

> **Version:** 0.9.1.5 | [Bootstrap Contract →](BOOTSTRAP_CONTRACT.md)

This report records the v0.5.8 self-hosting pilot result. It proves that a small Baa0 compiler slice can be compiled as an object, linked with C compiler code, and checked for behavior parity against the current C implementation.

> Note: the pilot artifacts and mixed-harness scripts were retired in v0.9.1.5 after the production lexer moved to `src/frontend/lexer.baa` and normal QA took over lexer coverage. This report is retained as historical evidence for the bootstrap path.

---

## 1. Pilot Slice

Pilot source at the time:

- A small Baa0 token-name slice under the frontend lexer migration area, retired in v0.9.1.5.

Validated behavior:

- Maps numeric `BaaTokenType` values to the same visible token names returned by the C baseline `token_type_to_str()`.
- Returns `"UNKNOWN"` for values outside the known token enum range.
- Uses only Baa0-approved control flow, integer comparison, direct returns, and string literals.

This is intentionally not a full lexer port. It is the smallest useful frontend slice that exercises mixed C+Baa object linking and UTF-8 data parity.

---

## 2. Mixed Build Gate

Gate at the time: the retired mixed harness token-name target.

The gate:

- Compiles the Baa pilot source with `baa -O2 --verify-ir --verify-ssa -c`.
- Generates a C parity harness from `src/frontend/lexer.h`.
- Linked the C baseline helper, the generated C harness, and the Baa object with the host C compiler.
- Decodes Baa `نص` values from the current packed `حرف` representation before comparing them with C UTF-8 strings.
- Fails if the Baa pilot source uses Baa0-banned features outside comments and literals.

The compatibility wrapper was later retired with the mixed harness.

---

## 3. Go/No-Go Result

Result: **Go for Phase 5 mixed-harness work, no-go for full lexer replacement yet.**

Rationale:

- Mixed C+Baa object linking works for a narrow Baa0 frontend slice.
- Token-name parity can be checked automatically and is now part of the release QA gate.
- Full lexer migration still needs token-stream dump/parity tooling, include/preprocessor fixture coverage, and a stable bridge boundary for lexer state before replacing C lexer logic.

Next recommended step:

- Build a token-stream parity harness that can compare C lexer output and a Baa lexer slice on fixed UTF-8/include/macro fixtures without requiring parser or IR behavior.

---

*[Bootstrap Contract →](BOOTSTRAP_CONTRACT.md)*
