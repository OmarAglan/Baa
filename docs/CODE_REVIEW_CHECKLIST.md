# Code Review Checklist

> **Version:** 0.3.8 | [← Internals](INTERNALS.md)

Use this checklist for any change that touches compiler correctness, IR, or Windows x64 backend behavior.

## Correctness

- No change alters user-visible semantics without an explicit spec update.
- Deterministic output for the same input.
- Error paths are explicit (no silent failure).

## Memory / Ownership

- Every `malloc/calloc/strdup` has an owner and a clear lifetime.
- Arena-owned IR objects are never individually freed.
- NULL checks exist before dereference.

## IR / Verifiers

- New invariants are enforced by `--verify-ir`.
- SSA-related changes are enforced by `--verify-ssa`.
- CFG changes rebuild metadata (`pred/succ`, dominance) when required.

## Backend (Windows x64 ABI)

- Stack alignment and shadow space rules preserved.
- Calls follow ABI register/stack argument conventions.
- No illegal mem-to-mem forms introduced by regalloc/spills.

## Tests

- Add at least one test that fails before the change and passes after.
- `python3 scripts/qa_run.py --mode full` passes.

## Docs

- If a flag/behavior changes: update `CHANGELOG.md` + relevant docs.
