# Baa Ecosystem Compatibility Matrix

> **Version:** draft-0.1 | **Applies to:** Baa v0.5.8+

This document records which versions of Baa, Nazm, Takween, Qalam-IDE, and
PyramidOS-facing contracts are expected to work together.

Draft contract names in this matrix are planning commitments, not claims of current
implementation. See [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md) for the implemented v0.5.x
boundary.

---

## 1. Compatibility Policy

The Baa ecosystem uses contract compatibility, not repository lockstep.

A project may release independently as long as it stays compatible with the contract versions listed here.

---

## 2. Contract Names

| Contract | Owner | Consumers |
|---|---|---|
| `compiler-cli-v1` | Baa | Takween, Qalam, scripts |
| `build-manifest-v1` | Baa | Takween |
| `diagnostics-json-v1` | Baa | Qalam, Takween |
| `symbols-json-v1` | Baa | Qalam |
| `completion-data-json-v1` | Baa | Baa-LSP, Qalam |
| `format-json-v1` | Baa | Baa-LSP, Qalam |
| `semantic-query-json-v1` | Baa | Baa-LSP, Qalam |
| `semantic-index-json-v1` | Baa | Baa-LSP |
| `tokens-json-v1` | Baa | Qalam |
| `target-spec-v1` | Baa | Takween, PyramidOS experiments |
| `conformance-v1` | Baa | Baa, future compilers/tools |
| `freestanding-v0` | Baa | PyramidOS experiments |
| `baa-nazm-boundary-v0` | Baa + Nazm | Baa backend, Nazm CLI/API |
| `nazm-api-v1` | Nazm | opt-in embedded Baa path, API/CLI parity gates |
| `nazm-capabilities-v1` | Nazm | Baa and Takween cache fingerprints |
| `nazm-source-v0.4` | Nazm | humans, Baa canonical Arabic assembly emitter |
| `elf64-object-v0` / `coff-object-v0` | Nazm | system linkers, Baa builds |
| `baa-language-v0.5.9` | Baa | C reference compiler, tests |
| `baa-stdlib-v0.5.9` | Baa | hosted Baa programs |
| `baa-hosted-abi-v0.5.9` | Baa | Windows/Linux backends |
| `baa-ir-v0.5.9` | Baa | C reference compiler, verifier tooling |

---

## 3. Planned Compatibility Table

| Baa Version | Nazm | Takween | Qalam-IDE | PyramidOS Use | Required Contracts |
|---|---|---|---|---|---|
| v0.5.8 | independent; no integration | manual/experimental | manual/experimental | none | C reference reset |
| v0.5.9 | independent; no integration | manual/experimental | manual/experimental | none | `baa-language-v0.5.9`, `baa-stdlib-v0.5.9`, `baa-hosted-abi-v0.5.9`, `baa-ir-v0.5.9` |
| v0.6.x | Nazm 0.4 independent; boundary planning | Takween prototype | Qalam prototype | host tools only | compiler-cli-v1 draft, diagnostics-json-v1 implemented draft |
| v0.7.0 | Baa emission inventory | Takween integration | Qalam not required | host tools only | build-manifest-v1, compiler-cli-v1, baa-nazm-boundary-v0 draft |
| v0.7.2 | non-default shadow subprocess | Takween integration | Qalam integration | host tools only | diagnostics-json-v1, tokens-json-v1, symbols-json-v1, completion-data-json-v1, semantic-query-json-v1 |
| v0.8.x | parity hardening; still gated | stable integration | stable integration | host tools only | conformance-v1 draft, target-spec-v1 |
| v0.9.0 | default decision only after parity signoff | stable through 1.0 review | stable through 1.0 review | freestanding plan only | all v1 hosted contracts frozen |
| v0.10.x/post-v0.9 | admitted path or explicit external-assembler fallback | stable | stable | tiny mixed-link experiments | freestanding-v0, architecture target decision |

### Current admitted ecosystem snapshot

The implementation has advanced beyond the original version forecast above:
Nazm is the production assembler default with explicit GAS rollback, while the
new in-process path remains opt-in. Baa consumes `nazm-api-v1` only when built
with embedding enabled and records the exact `nazm-capabilities-v1` digest in
its full assembler fingerprint. Takween treats that value as required cache-key
evidence for Nazm artifacts. CLI/API equivalence is gated on ELF64 and COFF;
the default process boundary cannot change without a separate admission.

For Qalam's Baa-first live analysis, `compiler-cli-v1` now includes the
check-only `--source-stdin=<logical-file>` input shape. It preserves
`diagnostics-json-v1`, source-relative includes, Arabic paths, and exit codes
while accepting the editor buffer through stdin. This is an additive Qalam
contract; it does not change Takween, Nazm, or PyramidOS behavior.

The compiler also owns `completion-data-json-v1`: Baa-LSP loads its Arabic
keywords, directives, snippets, and canonical builtin signatures directly from
Baa. Cursor-sensitive `semantic-query-json-v1` completion adds visible
parameters and locals plus declarations from explicitly included headers,
while excluding future and sibling-scope declarations. Baa-LSP only filters the
Arabic prefix, converts the replacement range, resolves documentation, and
rejects obsolete versions.

Cursor-sensitive hover, call signatures, definitions, and translation-unit
references use `semantic-query-json-v1`. Baa resolves the active expression,
declaration, shadowed local, included prototype, bound reference set, and
parameter index; Baa-LSP only converts UTF-8 byte positions and paths to LSP
UTF-16 locations and rejects obsolete document versions.

Canonical source formatting uses `format-json-v1`. Baa owns the tolerant,
idempotent four-space/LF style and preserves literal and comment contents;
Baa-LSP converts the complete replacement to a versioned LSP edit, while Qalam
only applies an edit that still matches its open document.

Project-aware navigation uses `semantic-index-json-v1` together with the
structured source and include closure in `takween-build-plan-v1`. Baa owns
symbol identity and occurrence binding; Takween owns project membership;
Baa-LSP performs only version-safe fan-out, location conversion, sorting, and
deduplication. The same index owns identifier roles for semantic highlighting,
including parameters and fields; raw `tokens-json-v1` remains the tolerant
lexical fallback while a buffer is incomplete.

---

## 4. Stability Levels

| Level | Meaning |
|---|---|
| experimental | may change without migration support |
| draft | should be tested, but minor schema changes allowed |
| stable | breaking changes require compatibility note and version bump |
| frozen | no breaking changes until next major review |

---

## 5. Release Checklist

Before every Baa release:

- [ ] Update this matrix.
- [ ] State which contracts changed.
- [ ] State whether Takween is affected.
- [ ] State whether Qalam-IDE is affected.
- [ ] State whether Nazm or the assembly boundary is affected.
- [ ] State whether PyramidOS experiments are affected.
- [ ] Add migration notes for breaking changes.
- [ ] Follow the RC cut, admission, receipt, and rollback rules in
      [RELEASE_PROCESS.md](RELEASE_PROCESS.md).

---

## 6. Current Rule

Until v0.9.0, Baa may change language behavior only when the roadmap explicitly calls for it and conformance tests are updated in the same change.
