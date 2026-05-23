# Phase 4.5 Handoff Bundle

> **Version:** 0.9.0.2 | [← Self-Hosting Pilot](SELF_HOSTING_PILOT_REPORT.md) | [Stage-0 Manifest →](STAGE0_BOOTSTRAP_MANIFEST.md)

This document defines the handoff package from Phase 4.5 bootstrap readiness into the Phase 5 / v0.9.0 self-hosting execution baseline.

---

## 1. Frozen Inputs

The v0.9.0 baseline must consume these artifacts as inputs, not redefine them:

- `docs/COMPONENT_OWNERSHIP.md` — component boundaries and allowed dependency directions.
- `docs/BOOTSTRAP_CONTRACT.md` — frozen language, ABI, stdlib, and IR contracts.
- `docs/BAA0_SPEC.md` — conservative bootstrap subset for Baa compiler slices.
- `docs/SELF_HOSTING_PILOT_REPORT.md` — mixed C+Baa pilot result and next harness recommendation.
- `scripts/qa_bootstrap_gate.py` — Baa0 compliance and bootstrap corpus gate.
- `scripts/qa_selfhost_pilot.py` — mixed C+Baa pilot parity gate.
- `tests/bootstrap/` — Baa0 compliance corpus.

---

## 2. Handoff Gate

Run:

```bash
python scripts/qa_phase45_handoff.py
```

The gate creates `.baa_phase45_handoff/` and writes `handoff-summary.json` with:

- compiler, Python, CMake, GCC, OS, and git state,
- release QA summary,
- deterministic sample build manifest,
- bootstrap and self-hosting gate evidence,
- Windows/Linux signoff slots.

For a fast structural check without the full release QA run:

```bash
python scripts/qa_phase45_handoff.py --skip-release-qa
```

Generated handoff outputs are local release evidence and are intentionally ignored by git.

---

## 3. Stage-0 Toolchain Inputs

Stage-0 is the C-built Baa compiler produced from this repository with the current host C toolchain.

Required local inputs:

- CMake configured from this checkout.
- Python 3 for QA orchestration.
- Host C compiler / GCC compatible with the active target object format.
- `BAA_REPRODUCIBLE_BUILD_DATE` when producing reproducible release/verify builds.
- `python scripts/qa_run.py --mode release` passing on the local platform.

The handoff summary records exact tool versions and the current git state. If the worktree is dirty, the summary remains useful but is not a clean release provenance record.

---

## 4. Exit Status

Phase 4.5 handoff bundle readiness is complete when:

- required artifacts exist,
- the handoff gate succeeds,
- the sample build manifest is valid JSON,
- release QA / bootstrap / self-host pilot gates pass on the local platform.

Full Phase 4.5 exit still additionally requires Windows and Linux `quick/full` evidence. If the handoff gate is run on only one platform, `phase45_exit_ready` remains false in the generated summary until the missing platform evidence is attached.

As of 2026-05-22, Windows handoff evidence is present and Linux signoff is explicitly deferred. Phase 5 planning may consume the frozen artifacts above, but Phase 5 implementation checkpoints must continue to treat Linux `quick/full` evidence as a required gate before claiming full Phase 4.5 exit or a cross-platform release candidate.

---

*[← Self-Hosting Pilot](SELF_HOSTING_PILOT_REPORT.md) | [Stage-0 Manifest →](STAGE0_BOOTSTRAP_MANIFEST.md)*
