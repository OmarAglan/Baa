# IR Developer Guide

> **Version:** 0.3.10.6 | [← Internals](INTERNALS.md) | [IR Specification →](BAA_IR_SPECIFICATION.md)

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
   - Integration `.baa` coverage under `tests/integration/` for user-visible behavior
   - Negative test under `tests/neg/` if the change adds/changes diagnostics
8) Update docs: `docs/BAA_IR_SPECIFICATION.md`.

## IR Builder API

The IR Builder (`src/ir_builder.h`) is the primary way to construct IR. It handles basic block management and ensures instructions are appended correctly.

Example:
```c
// Create a function and entry block
IRFunc* func = ir_builder_create_func(builder, "مضاعفة", IR_TYPE_I64_T);
IRBlock* entry = ir_builder_create_block(builder, "بداية");
ir_builder_set_insert_point(builder, entry);

// Emit: %م٠ = ضرب ص٦٤ %معامل٠، ٢
IRValue* param0 = ir_value_reg(0, IR_TYPE_I64_T);
int res = ir_builder_emit_mul(builder, IR_TYPE_I64_T, param0, ir_builder_const_i64(2));

// Emit: رجوع ص٦٤ %م٠
ir_builder_emit_ret(builder, ir_value_reg(res, IR_TYPE_I64_T));
```

New builder functions in `v0.3.10.6`:
- `ir_builder_emit_ptr_offset`: For pointer arithmetic (`إزاحة_مؤشر`).
- `ir_builder_emit_cast`: For explicit type conversion (`تحويل`).
- `ir_builder_emit_call_indirect`: For calling function pointers.

## Adding / Modifying an Optimization Pass

- Prefer correctness and verifier clarity over aggressive transforms.
- Run pass under `--verify-gate` during development.

Checklist:

- Pass preserves IR well-formedness.
- Pass preserves SSA invariants (if it runs before Out-of-SSA).
- Pass updates or rebuilds CFG metadata if it changes edges.

**Optimization Pipeline Flags:**
- `--verify-ssa`: Validates SSA invariants (dominance, single-def).
- `--verify-ir`: Validates general IR well-formedness.
- `--verify-gate`: Runs IR/SSA verification after each pass iteration.

**New Passes (v0.3.10.6):**
- **GVN** (`src/ir_gvn.c`): Global Value Numbering.
- **LICM** (`src/ir_licm.c`): Loop Invariant Code Motion.
- **InstCombine** (`src/ir_instcombine.c`): Local instruction simplifications.
- **SCCP** (`src/ir_sccp.c`): Sparse Conditional Constant Propagation.

## IR Infrastructure Modules

| Module | Purpose |
|--------|---------|
| `src/ir.c` | Core IR data structures, types, and Arabic name mappings. |
| `src/ir_builder.c` | High-level API for emitting IR instructions and blocks. |
| `src/ir_arena.c` | Chunk-based arena allocator for bulk free performance. |
| `src/ir_verify_ir.c` | Verifies instruction well-formedness (counts/types/terminators). |
| `src/ir_verify_ssa.c` | Verifies SSA invariants (dominance/single-def/phi). |
| `src/ir_optimizer.c` | Fixed-point optimization pipeline and pass manager. |
| `src/ir_analysis.c` | Dominance tree and basic CFG analysis. |
| `src/ir_lower.c` | Large dispatcher converting AST nodes to Baa IR. |
| `src/ir_text.c` | Machine-readable text serialization and deserialization. |
| `src/ir_outssa.c` | Eliminates `فاي` nodes by inserting edge copies. |
| `src/ir_loop.c` | Natural loop discovery using back-edges and dominators. |
| `src/ir_unroll.c` | Conservative full unrolling for small constant-trip loops. |

## Debugging Workflow

- Use `--dump-ir` / `--dump-ir-opt` and `--verify`.
- Use `--time-phases` when diagnosing compile-time regressions.
- Use `--trace-lexer` or `--trace-parser` for frontend issues.
