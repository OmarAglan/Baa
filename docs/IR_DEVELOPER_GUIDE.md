# IR Developer Guide

> **Version:** 0.3.10.6 | [← Internals](INTERNALS.md) | [IR Specification →](BAA_IR_SPECIFICATION.md)

This guide is for contributors working on the Baa IR and mid-end.

## Core Rules

- **IR is the contract** between frontend and backend.
- **Arena ownership:** IR objects are module-owned ([`IRModule.arena`](src/ir.h:424)). Do not `free()` individual IR objects.
- **Verifier-first development:** any new IR feature should be validated by `--verify-ir` and (when SSA-related) `--verify-ssa`.

---

## IR System Architecture

### Module Structure

The [`IRModule`](src/ir.h:419) is the top-level container for a translation unit:

```c
typedef struct IRModule {
    char* name;                    // Module name (source file)
    IRArena arena;                 // Arena allocator for all IR objects
    IRType* cached_i8_ptr_type;    // Cache for common types
    IRGlobal* globals;             // Linked list of global variables
    int global_count;
    IRFunc* funcs;                 // Linked list of functions
    int func_count;
    IRStringEntry* strings;        // String literal table (.Lstr_<n>)
    int string_count;
    IRBaaStringEntry* baa_strings; // Baa strings (.Lbs_<n>)
    int baa_string_count;
} IRModule;
```

### Function Structure

The [`IRFunc`](src/ir.h:333) represents a function in the IR:

```c
typedef struct IRFunc {
    char* name;                 // Function name
    IRType* ret_type;           // Return type
    IRParam* params;            // Parameters (each gets %معامل<n>)
    int param_count;
    IRBlock* entry;             // Entry block
    IRBlock* blocks;            // All blocks (linked list)
    int block_count;
    int next_reg;               // Next virtual register number
    int next_inst_id;           // Instruction ID counter
    uint32_t ir_epoch;          // Change counter for cache invalidation
    IRDefUse* def_use;          // Def-use analysis cache
    int next_block_id;          // Block ID counter
    bool is_prototype;          // Declaration without body
    struct IRFunc* next;
} IRFunc;
```

### Block Structure

The [`IRBlock`](src/ir.h:285) represents a basic block in the CFG:

```c
typedef struct IRBlock {
    char* label;                // Block label (Arabic name like "بداية")
    int id;                     // Numeric ID
    struct IRFunc* parent;      // Containing function
    IRInst* first;              // First instruction
    IRInst* last;               // Last instruction
    int inst_count;
    struct IRBlock* succs[2];   // Successors (0-2 for br/br_cond)
    int succ_count;
    struct IRBlock** preds;     // Dynamic array of predecessors
    int pred_count;
    int pred_capacity;
    struct IRBlock* idom;       // Immediate dominator
    struct IRBlock** dom_frontier;
    int dom_frontier_count;
    struct IRBlock* next;
} IRBlock;
```

### Instruction Structure

The [`IRInst`](src/ir.h:229) represents a single IR instruction:

```c
typedef struct IRInst {
    IROp op;                    // Opcode (e.g., IR_OP_ADD)
    IRType* type;               // Result type
    int id;                     // Unique instruction ID
    int dest;                   // Destination register (-1 if none)
    IRValue* operands[4];       // Up to 4 operands
    int operand_count;
    IRCmpPred cmp_pred;         // For comparisons
    IRPhiEntry* phi_entries;    // Linked list for PHI nodes
    char* call_target;          // Function name for calls
    IRValue* call_callee;       // Indirect call value
    IRValue** call_args;        // Call arguments
    int call_arg_count;
    const char* src_file;       // Debug info
    int src_line;
    int src_col;
    const char* dbg_name;       // Optional symbol name
    struct IRBlock* parent;     // Containing block
    struct IRInst* prev;        // Linked list
    struct IRInst* next;
} IRInst;
```

### Value System

The [`IRValue`](src/ir.h:192) represents values and operands:

```c
typedef struct IRValue {
    IRValueKind kind;           // Discriminator
    IRType* type;
    union {
        int64_t const_int;      // IR_VAL_CONST_INT
        struct { char* data; int id; } const_str;  // IR_VAL_CONST_STR
        int reg_num;            // IR_VAL_REG (%م<reg_num>)
        char* global_name;      // IR_VAL_GLOBAL, IR_VAL_FUNC
        struct IRBlock* block;  // IR_VAL_BLOCK
    } data;
} IRValue;
```

---

## Arabic Naming in IR

### Register Naming

| Register Type | Format | Example |
|--------------|--------|---------|
| Virtual registers | `%م<n>` (Arabic numerals) | `%م٠`, `%م١`, `%م٢` |
| Parameters | `%معامل<n>` | `%معامل٠`, `%معامل١` |
| Globals | `@<name>` | `@العداد` |
| Functions | `@<name>` | `@الرئيسية` |

### Special Virtual Registers (Backend)

These are used in the instruction selection and register allocation phases:

| VReg | Meaning |
|------|---------|
| `-1` | RBP (frame pointer) |
| `-2` | RAX (return value) |
| `-3` | RSP (stack pointer) |
| `-10`.. | ABI argument registers (target-dependent) |

### Arabic Opcodes in Text Format

```
Arithmetic:    جمع، طرح، ضرب، قسم، باقي، سالب
Memory:        حجز، حمل، خزن، إزاحة_مؤشر
Comparison:    قارن (مع: يساوي، لا_يساوي، أكبر، أصغر، ...)
Logical:       و، أو، أو_حصري، نفي، ازاحة_يسار، ازاحة_يمين
Control:       قفز، قفز_شرط، رجوع، نداء
SSA:           فاي، نسخ
Types:         فراغ، ص١، ص٨، ص١٦، ص٣٢، ص٦٤، ط٨، ط١٦، ط٣٢، ط٦٤، حرف، ع٦٤، مؤشر
```

---

## Key Files

### Core IR

| File | Purpose |
|------|---------|
| [`src/ir.h`](src/ir.h) / [`src/ir.c`](src/ir.c) | Core data structures, types, opcodes, and Arabic name mappings |
| [`src/ir_builder.h`](src/ir_builder.h) / [`src/ir_builder.c`](src/ir_builder.c) | High-level API for constructing IR instructions and blocks |
| [`src/ir_arena.h`](src/ir_arena.h) / [`src/ir_arena.c`](src/ir_arena.c) | Chunk-based arena allocator for bulk free performance |
| [`src/ir_data_layout.h`](src/ir_data_layout.h) | Type sizes and alignment for target layout |
| [`src/ir_text.c`](src/ir_text.c) | Machine-readable text serialization and deserialization |

### Analysis

| File | Purpose |
|------|---------|
| [`src/ir_analysis.c`](src/ir_analysis.c) | Dominance tree and basic CFG analysis |
| [`src/ir_defuse.c`](src/ir_defuse.c) | Def-use chain construction |
| [`src/ir_loop.c`](src/ir_loop.c) | Natural loop discovery using back-edges and dominators |

### Lowering and Transformation

| File | Purpose |
|------|---------|
| [`src/ir_lower.c`](src/ir_lower.c) | AST to IR lowering dispatcher |
| [`src/ir_outssa.c`](src/ir_outssa.c) | Eliminates `فاي` nodes by inserting edge copies |
| [`src/ir_clone.c`](src/ir_clone.c) | IR cloning utilities |
| [`src/ir_mutate.c`](src/ir_mutate.c) | Safe IR mutation utilities |
| [`src/ir_canon.c`](src/ir_canon.c) | Canonicalization utilities |

---

## Optimization Passes

All passes follow the [`IRPass`](src/ir_pass.h) interface and are registered in [`src/ir_optimizer.c`](src/ir_optimizer.c).

### Main Optimizer

| File | Description |
|------|-------------|
| [`src/ir_optimizer.c`](src/ir_optimizer.c) | Fixed-point optimization pipeline and pass manager |

### Individual Passes

| File | Pass (Arabic) | Description |
|------|---------------|-------------|
| [`src/ir_mem2reg.c`](src/ir_mem2reg.c) | **ترقية الذاكرة إلى سجلات** | Promotes stack allocations (`حجز`) to SSA registers using `فاي` insertion |
| [`src/ir_constfold.c`](src/ir_constfold.c) | **طي_الثوابت** | Constant folding for arithmetic and comparisons |
| [`src/ir_copyprop.c`](src/ir_copyprop.c) | **نشر_النسخ** | Copy propagation for `نسخ` instructions |
| [`src/ir_cse.c`](src/ir_cse.c) | **حذف_المكرر** | Common subexpression elimination |
| [`src/ir_dce.c`](src/ir_dce.c) | **حذف_الميت** | Dead code elimination (instructions and unreachable blocks) |
| [`src/ir_gvn.c`](src/ir_gvn.c) | **ترقيم القيم العالمية** | Global Value Numbering |
| [`src/ir_sccp.c`](src/ir_sccp.c) | **نشر الثوابت المتناثر** | Sparse Conditional Constant Propagation |
| [`src/ir_instcombine.c`](src/ir_instcombine.c) | **دمج التعليمات** | Instruction combining and local simplifications |
| [`src/ir_licm.c`](src/ir_licm.c) | **حركة التعليمات غير المتغيرة** | Loop Invariant Code Motion |
| [`src/ir_unroll.c`](src/ir_unroll.c) | **فك الحلقات** | Conservative loop unrolling for small constant-trip loops |
| [`src/ir_inline.c`](src/ir_inline.c) | **تضمين الدوال** | Function inlining for small single-call sites |
| [`src/ir_cfg_simplify.c`](src/ir_cfg_simplify.c) | **تبسيط مخطط التدفق** | CFG simplification: merge trivial blocks, split critical edges |

### Pass Levels

- **O0**: No optimizations (debug mode)
- **O1**: Basic optimizations (mem2reg, constfold, copyprop, dce)
- **O2**: Full optimizations (+ CSE, GVN, SCCP, LICM, InstCombine, unrolling, inlining)

---

## Verification Systems

### IR Well-Formedness Verification

[`src/ir_verify_ir.c`](src/ir_verify_ir.c) validates general IR structure:

- Operand count for each opcode
- Type consistency within operands and results
- Terminator rules (each block must end with branch/return)
- `فاي` placement (only at block start) and incoming edge validity
- Call instruction validity

```c
bool ir_func_verify_ir(IRFunc* func, FILE* out);
bool ir_module_verify_ir(IRModule* module, FILE* out);
```

### SSA Form Verification

[`src/ir_verify_ssa.c`](src/ir_verify_ssa.c) validates SSA invariants:

- Single definition per register (%م<n> defined once)
- Dominance: every use is dominated by its definition
- `فاي` nodes: one entry per predecessor, no duplicates
- `فاي` nodes: no entries from non-predecessor blocks

```c
bool ir_func_verify_ssa(IRFunc* func, FILE* out);
bool ir_module_verify_ssa(IRModule* module, FILE* out);
```

### Verification Flags

| Flag | Purpose |
|------|---------|
| `--verify-ir` | Run IR well-formedness verification |
| `--verify-ssa` | Run SSA invariants verification |
| `--verify-gate` | Run verification after each pass iteration |

---

## Adding a New IR Opcode

1. Add enum value in [`src/ir.h`](src/ir.h) ([`IROp`](src/ir.h:34)).
2. Update Arabic printer name mapping ([`ir_op_to_arabic()`](src/ir.h:479)) so dumps remain Arabic-first.
3. Update verification rules in [`src/ir_verify_ir.c`](src/ir_verify_ir.c):
   - Operand count
   - Operand types
   - Terminator rules if applicable
4. Add builder helper in [`src/ir_builder.c`](src/ir_builder.c) (or reuse [`ir_inst_new()`](src/ir.h:535) + [`ir_inst_add_operand()`](src/ir.h:536)).
5. Add lowering in [`src/ir_lower.c`](src/ir_lower.c) (AST -> IR).
6. Add backend support:
   - ISel ([`src/isel.c`](src/isel.c)) mapping IR -> Machine IR
   - Emit ([`src/emit.c`](src/emit.c)) if needed
7. Add tests:
   - Integration `.baa` coverage under `tests/integration/` for user-visible behavior
   - Negative test under `tests/neg/` if the change adds/changes diagnostics
8. Update docs: [`docs/BAA_IR_SPECIFICATION.md`](docs/BAA_IR_SPECIFICATION.md).

---

## IR Builder API

The IR Builder ([`src/ir_builder.h`](src/ir_builder.h)) is the primary way to construct IR. It handles basic block management and ensures instructions are appended correctly.

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

### Builder Functions

#### Arithmetic
- [`ir_builder_emit_add()`](src/ir_builder.h:207) - جمع
- [`ir_builder_emit_sub()`](src/ir_builder.h:213) - طرح
- [`ir_builder_emit_mul()`](src/ir_builder.h:219) - ضرب
- [`ir_builder_emit_div()`](src/ir_builder.h:225) - قسم
- [`ir_builder_emit_mod()`](src/ir_builder.h:231) - باقي
- [`ir_builder_emit_neg()`](src/ir_builder.h:237) - سالب

#### Memory
- [`ir_builder_emit_alloca()`](src/ir_builder.h:249) - حجز
- [`ir_builder_emit_load()`](src/ir_builder.h:258) - حمل
- [`ir_builder_emit_store()`](src/ir_builder.h:267) - خزن
- [`ir_builder_emit_ptr_offset()`](src/ir_builder.h:274) - إزاحة_مؤشر

#### Comparison
- [`ir_builder_emit_cmp()`](src/ir_builder.h:288) - قارن
- [`ir_builder_emit_cmp_eq()`](src/ir_builder.h:293) - يساوي
- [`ir_builder_emit_cmp_ne()`](src/ir_builder.h:298) - لا_يساوي
- [`ir_builder_emit_cmp_gt()`](src/ir_builder.h:303) - أكبر
- [`ir_builder_emit_cmp_lt()`](src/ir_builder.h:308) - أصغر
- [`ir_builder_emit_cmp_ugt()`](src/ir_builder.h:321) - أكبر (unsigned)
- [`ir_builder_emit_cmp_ult()`](src/ir_builder.h:322) - أصغر (unsigned)

#### Logical
- [`ir_builder_emit_and()`](src/ir_builder.h:333) - و
- [`ir_builder_emit_or()`](src/ir_builder.h:338) - أو
- [`ir_builder_emit_xor()`](src/ir_builder.h:343) - أو_حصري
- [`ir_builder_emit_not()`](src/ir_builder.h:348) - نفي
- [`ir_builder_emit_shl()`](src/ir_builder.h:353) - ازاحة_يسار
- [`ir_builder_emit_shr()`](src/ir_builder.h:358) - ازاحة_يمين

#### Control Flow
- [`ir_builder_emit_br()`](src/ir_builder.h:369) - قفز
- [`ir_builder_emit_br_cond()`](src/ir_builder.h:378) - قفز_شرط
- [`ir_builder_emit_ret()`](src/ir_builder.h:386) - رجوع
- [`ir_builder_emit_call()`](src/ir_builder.h:414) - نداء
- [`ir_builder_emit_call_indirect()`](src/ir_builder.h:426) - نداء غير مباشر

#### SSA
- [`ir_builder_emit_phi()`](src/ir_builder.h:450) - فاي
- [`ir_builder_phi_add_incoming()`](src/ir_builder.h:459) - Add PHI incoming
- [`ir_builder_emit_copy()`](src/ir_builder.h:469) - نسخ

#### Type Conversion
- [`ir_builder_emit_cast()`](src/ir_builder.h:482) - تحويل

---

## Adding / Modifying an Optimization Pass

- Prefer correctness and verifier clarity over aggressive transforms.
- Run pass under `--verify-gate` during development.

Checklist:

- Pass preserves IR well-formedness.
- Pass preserves SSA invariants (if it runs before Out-of-SSA).
- Pass updates or rebuilds CFG metadata if it changes edges.

To add a new pass:

1. Create `src/ir_<passname>.c` and `src/ir_<passname>.h`
2. Define the pass function: `bool ir_<passname>_run(IRModule* module)`
3. Declare [`IRPass`](src/ir_pass.h) descriptor: `extern IRPass IR_PASS_<PASSNAME>;`
4. Register in [`src/ir_optimizer.c`](src/ir_optimizer.c) pipeline
5. Add tests under `tests/integration/ir/`

---

## Debugging Workflow

- Use `--dump-ir` / `--dump-ir-opt` to see IR before/after optimization.
- Use `--verify` to enable all verification checks.
- Use `--verify-gate` to catch pass bugs early.
- Use `--time-phases` when diagnosing compile-time regressions.
- Use `--trace-lexer` or `--trace-parser` for frontend issues.

### Common Debugging Commands

```bash
# View IR after lowering
./baa -O0 --dump-ir test.baa -o /dev/null

# View optimized IR
./baa -O2 --dump-ir-opt test.baa -o /dev/null

# Verify after each pass
./baa -O2 --verify-gate test.baa -o test.exe

# Just verify IR/SSA without compiling
./baa --verify-ir --verify-ssa test.baa -o /dev/null
```

---

## Memory Management

IR uses arena allocation for efficiency:

- All IR objects (types, values, instructions, blocks, functions) are allocated from [`IRModule.arena`](src/ir.h:424).
- Use [`ir_arena_alloc()`](src/ir_arena.h:39) / [`ir_arena_calloc()`](src/ir_arena.h:40) for allocation.
- Use [`ir_arena_strdup()`](src/ir_arena.h:41) for strings.
- Free the entire module with [`ir_module_free()`](src/ir.c) - never free individual objects.

The Def-Use analysis cache ([`IRFunc.def_use`](src/ir.h:357)) is heap-allocated and must be rebuilt when IR changes ([`ir_epoch`](src/ir.h:354) tracks modifications).
