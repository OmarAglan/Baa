# Compiler Migration Artifact Audit

> **Audit date:** 2026-06-29 | **Mainline:** `master` | **Experimental branch:** `origin/moving-to-baa`

This audit closes the v0.5.8 migration-isolation work without resuming the C-to-Baa compiler
rewrite. The C implementation remains the official reference compiler through v0.9.

## 1. Snapshot

At the audit point:

- `master` was four commits ahead of the common branch history.
- `origin/moving-to-baa` was 45 commits ahead of the common branch history.
- Mainline tracked no compiler implementation files written in `.baa` or `.baahd`.
- Mainline CMake declared `LANGUAGES C` and built `src/frontend/lexer.c` and
  `src/frontend/parser.c`.
- The experimental branch contained Baa lexer/parser slices and required
  `BAA_BOOTSTRAP_COMPILER` to generate their object files.

The experimental branch therefore preserves the migration work without making it part of the
mainline release path.

## 2. Artifact Disposition

| Artifact family | Experimental examples | Disposition |
|---|---|---|
| Baa frontend implementation | `src/frontend/lexer.baa`, `parser.baa`, `.baahd` bridge headers | Keep only on the experimental branch until a post-v0.9 staged decision |
| Bootstrap build wiring | `BAA_BOOTSTRAP_COMPILER`, Baa object-generation commands, bootstrap CI preparation | Do not merge into the normal C-reference build |
| Stage/Baa0 planning | `docs/BAA0_SPEC.md`, Stage-0 manifests, bootstrap gate scripts | Preserve for post-v0.9 reassessment; not a Phase 4.5 gate |
| Bootstrap parity corpus | `tests/bootstrap/`, parser/lexer bridge and parity tests | Preserve with the experiment; do not make release QA depend on it |
| General compiler fixes and tests | diagnostics, backend emission, raw bytes, pointer-member and negative-test work | Re-evaluate individually against the current roadmap; port only with focused C-reference tests and documentation |

No migration commit should be bulk-merged. A feature-neutral fix may be recreated or
cherry-picked only after confirming that it:

1. advances a current roadmap item,
2. does not require a Baa-built compiler,
3. retains the C lexer/parser and normal CMake build,
4. carries focused tests, and
5. passes the normal quick/full gates.

## 3. Mainline Retained Artifacts

Mainline intentionally retains only:

- `docs/BOOTSTRAP_CONTRACT.md`, which defines the deferred staged policy;
- `scripts/check_reference_compiler_policy.py`, which rejects bootstrap requirements in normal
  build entrypoints; and
- `tests/test_reference_compiler_policy.py`, which locks the guard behavior.

These artifacts protect the C reference implementation. They do not implement or activate
self-hosting.

## 4. Closure

The migration work is isolated on a dedicated experimental branch, and mainline contains no
stale migration-only build wiring. v0.5.8 work must continue on the C reference compiler.
Future self-hosting may be reconsidered only after v0.9 under explicit parity, determinism,
rollback, provenance, and cross-platform gates.
