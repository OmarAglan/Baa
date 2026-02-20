# IR Developer Guide

> **Version:** 0.3.5 | [← Internals](INTERNALS.md) | [IR Specification →](BAA_IR_SPECIFICATION.md)

This guide is for contributors working on the Baa IR and mid-end.

## Core Rules

- **IR is the contract** between frontend and backend.
- **Arena ownership:** IR objects are module-owned (`IRModule.arena`). Do not `free()` individual IR objects.
- **Verifier-first development:** any new IR feature should be validated by `--verify-ir` and (when SSA-related) `--verify-ssa`.

## Adding a New IR Opcode

1) Add enum value in `src/ir.h` (`IROp`).
2) Update Arabic printer name mapping (`ir_op_to_arabic`) so dumps remain Arabic-first.
3) Update verification rules in `src/ir_verify_ir.c`:
   - operand count
   - operand types
   - terminator rules if applicable
4) Add builder helper in `src/ir_builder.c` (or reuse `ir_inst_new` + `ir_inst_add_operand`).
5) Add lowering in `src/ir_lower.c` (AST -> IR).
6) Add backend support:
   - ISel (`src/isel.c`) mapping IR -> Machine IR
   - Emit (`src/emit.c`) if needed
7) Add tests:
   - C unit test under `tests/ir_*_test.c` for the new invariant
   - Integration `.baa` test if user-visible
8) Update docs: `docs/BAA_IR_SPECIFICATION.md`.

## Adding / Modifying an Optimization Pass

- Prefer correctness and verifier clarity over aggressive transforms.
- Run pass under `--verify-gate` during development.

Checklist:

- Pass preserves IR well-formedness.
- Pass preserves SSA invariants (if it runs before Out-of-SSA).
- Pass updates or rebuilds CFG metadata if it changes edges.

## Debugging Workflow

- Use `--dump-ir` / `--dump-ir-opt` and `--verify`.
- Use `--time-phases` when diagnosing compile-time regressions.
