# Baa Ecosystem Compatibility Matrix

> **Version:** draft-0.1 | **Applies to:** Baa v0.5.8+

This document records which versions of Baa, Takween, Qalam-IDE, and PyramidOS-facing contracts are expected to work together.

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
| `tokens-json-v1` | Baa | Qalam |
| `target-spec-v1` | Baa | Takween, PyramidOS experiments |
| `conformance-v1` | Baa | Baa, future compilers/tools |
| `freestanding-v0` | Baa | PyramidOS experiments |

---

## 3. Planned Compatibility Table

| Baa Version | Takween | Qalam-IDE | PyramidOS Use | Required Contracts |
|---|---|---|---|---|
| v0.5.8 | manual/experimental | manual/experimental | none | C reference reset |
| v0.5.9 | manual/experimental | manual/experimental | none | RC QA baseline |
| v0.6.x | Takween prototype | Qalam prototype | host tools only | compiler-cli-v1 draft |
| v0.7.0 | Takween integration | Qalam not required | host tools only | build-manifest-v1, compiler-cli-v1 |
| v0.7.2 | Takween integration | Qalam integration | host tools only | diagnostics-json-v1, tokens-json-v1, symbols-json-v1 |
| v0.8.x | stable integration | stable integration | host tools only | conformance-v1 draft, target-spec-v1 |
| v0.9.0 | stable through 1.0 review | stable through 1.0 review | freestanding plan only | all v1 hosted contracts frozen |
| v0.10.x/post-v0.9 | stable | stable | tiny mixed-link experiments | freestanding-v0, i386 target drafts |

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
- [ ] State whether PyramidOS experiments are affected.
- [ ] Add migration notes for breaking changes.
- [ ] Follow the RC cut, admission, receipt, and rollback rules in
      [RELEASE_PROCESS.md](RELEASE_PROCESS.md).

---

## 6. Current Rule

Until v0.9.0, Baa may change language behavior only when the roadmap explicitly calls for it and conformance tests are updated in the same change.
