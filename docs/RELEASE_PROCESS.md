# Baa Release Candidate Process

> **Applies to:** v0.5.9 and later reference-compiler release candidates

This process keeps a release candidate narrow, reproducible, and reversible. It does not create
an RC branch by itself; the branch cut is allowed only after the platform prerequisites below
are satisfied.

## 1. RC Cut Prerequisites

Before creating `release/v<version>`:

1. The root CMake build remains C/RC-only and the reference-compiler policy gate passes.
2. Strict warning-as-error builds pass on Windows and Linux.
3. `quick`, `full`, `stress`, and `release` QA receipts are green on both supported hosts.
4. `docs/RELEASE_CANDIDATE_STATUS.md` records the exact commit and toolchain provenance.
5. Known limitations, compatibility matrix, roadmap, and changelog are current.
6. Compiler version metadata and the reproducible build date are synchronized.

The branch must be cut from the exact commit named by both platform receipts.

## 2. Branch and Tag Rules

- RC branch name: `release/v<major>.<minor>.<patch>`.
- RC tags: `v<major>.<minor>.<patch>-rc.<number>`.
- Final tag: `v<major>.<minor>.<patch>`, created only from the signed-off RC branch.
- Do not merge unrelated development into a release branch.
- Keep the branch until the final tag and immediate rollback window are complete.

## 3. Allowed Changes After the Cut

Allowed:

- fixes for reproduced release blockers,
- regression tests that fail before the fix and pass after it,
- deterministic QA/release-gate corrections,
- documentation and known-limitations corrections,
- version, packaging, installer, and changelog metadata required for the release.

Not allowed:

- new language, standard-library, optimizer, backend, target, or tooling features,
- syntax, ABI, IR, diagnostic-schema, manifest-schema, or exit-code contract expansion,
- broad refactors, module moves, formatting sweeps, or dependency changes,
- compiler-in-Baa migration, bootstrap requirements, assembler/linker independence, or
  PyramidOS kernel work.

Work outside the allowed set returns to `master` and waits for the next development milestone.

## 4. Fix Admission

Every RC fix must:

1. identify the failing gate, test, or user-visible defect;
2. remain a focused, revertable commit;
3. add or update a regression test when behavior changes;
4. update diagnostics snapshots or documentation when their contracts change;
5. rerun the smallest relevant gate immediately;
6. rerun `quick` and `full` on both hosts before merge; and
7. rerun `stress` and `release` on both hosts before a new RC tag.

Any change to compiler C sources, build logic, target behavior, diagnostics, manifests, or QA
invalidates the prior affected receipts. Documentation-only corrections may retain receipts
when they do not alter generated corpora, tests, packaging inputs, or contract meaning.

## 5. Failure and Rollback

- A failed release gate blocks tagging.
- Revert the smallest offending commit when a safe fix is not immediately clear.
- Never weaken, skip, or delete a gate merely to obtain a green receipt.
- Preserve failed JSON/log artifacts alongside the replacement receipt.
- If rollback cannot restore both platforms, abandon the RC number and return fixes to
  `master`; cut a new RC only after the full prerequisites pass again.

## 6. Current Activation State

The policy is active for v0.5.9. Windows and Linux receipts are green in Actions run
`28384736088`, so the documentation-only closure commit may be used to cut `release/v0.5.9`.
Feature development continues on `master`; the release branch accepts only the changes allowed
above.
