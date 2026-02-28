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
    IRArena arena;                 // Arena allocator for all IR objects (ساحة ذاكرة IR)
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
    IRBlock* entry;             // Entry block (first block)
    IRBlock* blocks;            // All blocks (linked list)
    int block_count;
    int next_reg;               // Next virtual register number (%م<n>)
    int next_inst_id;           // Instruction ID counter
    uint32_t ir_epoch;          // Change counter for cache invalidation
    IRDefUse* def_use;          // Def-use analysis cache
    int next_block_id;          // Block ID counter
    bool is_prototype;          // Declaration without body
    struct IRFunc* next;        // Linked list of functions in module
} IRFunc;
```

### Block Structure

The [`IRBlock`](src/ir.h:285) represents a basic block in the CFG:

```c
typedef struct IRBlock {
    char* label;                // Block label (Arabic name like "بداية")
    int id;                     // Numeric ID for internal use
    struct IRFunc* parent;      // Containing function (الأب)
    IRInst* first;              // First instruction
    IRInst* last;               // Last instruction
    int inst_count;
    struct IRBlock* succs[2];   // Successors (0-2 for br/br_cond)
    int succ_count;
    struct IRBlock** preds;     // Predecessors (dynamic array)
    int pred_count;
    int pred_capacity;
    struct IRBlock* idom;       // Immediate dominator
    struct IRBlock** dom_frontier;
    int dom_frontier_count;
    struct IRBlock* next;       // Linked list of blocks in function
} IRBlock;
```

### Instruction Structure

The [`IRInst`](src/ir.h:229) represents a single IR instruction:

```c
typedef struct IRInst {
    IROp op;                    // Opcode (e.g., IR_OP_ADD)
    IRType* type;               // Result type
    int id;                     // Unique instruction ID (for diagnostics/tests)
    int dest;                   // Destination register (-1 if none)
    IRValue* operands[4];       // Up to 4 operands
    int operand_count;
    IRCmpPred cmp_pred;         // For comparison instructions
    IRPhiEntry* phi_entries;    // Linked list for PHI nodes
    char* call_target;          // Function name for calls
    IRValue* call_callee;       // Indirect call value (NULL for direct calls)
    IRValue** call_args;        // Call arguments
    int call_arg_count;
    const char* src_file;       // Debug info: source file
    int src_line;               // Debug info: line number
    int src_col;                // Debug info: column number
    const char* dbg_name;       // Optional symbol name (for debugging)
    struct IRBlock* parent;     // Containing block
    struct IRInst* prev;        // Linked list within block
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

Value kinds ([`IRValueKind`](src/ir.h:177)):
- `IR_VAL_NONE` - No value (void)
- `IR_VAL_CONST_INT` - Constant integer
- `IR_VAL_CONST_STR` - Constant string (pointer to .rdata)
- `IR_VAL_BAA_STR` - Baa string (pointer to .rodata UTF-8 chars array)
- `IR_VAL_REG` - Virtual register (%م<n>)
- `IR_VAL_GLOBAL` - Global variable (@name)
- `IR_VAL_FUNC` - Function reference (@name)
- `IR_VAL_BLOCK` - Basic block reference

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
Types:         فراغ، ص١، ص٨، ص١٦، ص٣٢، ص٦٤، ط٨، ط١٦، ط٣٢، ط٦٤، حرف، ع٦٤، مؤشر، مصفوفة، دالة
```

### IR Type System ([`IRTypeKind`](src/ir.h:126))

| Kind | Arabic | Description |
|------|--------|-------------|
| `IR_TYPE_VOID` | فراغ | Void (no value) |
| `IR_TYPE_I1` | ص١ | 1-bit integer (boolean) |
| `IR_TYPE_I8` | ص٨ | 8-bit integer (byte/char) |
| `IR_TYPE_I16` | ص١٦ | 16-bit integer |
| `IR_TYPE_I32` | ص٣٢ | 32-bit integer |
| `IR_TYPE_I64` | ص٦٤ | 64-bit integer (primary) |
| `IR_TYPE_U8` | ط٨ | 8-bit unsigned integer |
| `IR_TYPE_U16` | ط١٦ | 16-bit unsigned integer |
| `IR_TYPE_U32` | ط٣٢ | 32-bit unsigned integer |
| `IR_TYPE_U64` | ط٦٤ | 64-bit unsigned integer |
| `IR_TYPE_CHAR` | حرف | UTF-8 character (packed) |
| `IR_TYPE_F64` | ع٦٤ | 64-bit floating point |
| `IR_TYPE_PTR` | مؤشر | Pointer type |
| `IR_TYPE_ARRAY` | مصفوفة | Array type |
| `IR_TYPE_FUNC` | دالة | Function type |

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

### IRBuilder Structure

The [`IRBuilder`](src/ir_builder.h:43) struct maintains context for IR construction:

```c
typedef struct IRBuilder {
    IRModule* module;           // Target module
    IRFunc* current_func;       // Current function being built
    IRBlock* insert_block;      // Current insertion block
    
    // Source location tracking (for debugging)
    const char* src_file;
    int src_line;
    int src_col;
    
    // Statistics
    int insts_emitted;
    int blocks_created;
} IRBuilder;
```

### Example Usage

```c
// Create a builder and module
IRBuilder* builder = ir_builder_new(module);

// Create a function and entry block
IRFunc* func = ir_builder_create_func(builder, "مضاعفة", IR_TYPE_I64_T);
ir_builder_add_param(builder, "x", IR_TYPE_I64_T);

IRBlock* entry = ir_builder_create_block(builder, "بداية");
ir_builder_set_insert_point(builder, entry);

// Emit: %م٠ = ضرب ص٦٤ %معامل٠، ٢
IRValue* param0 = ir_builder_reg_value(builder, 0, IR_TYPE_I64_T);
int res = ir_builder_emit_mul(builder, IR_TYPE_I64_T, param0, ir_builder_const_i64(2));

// Emit: رجوع ص٦٤ %م٠
ir_builder_emit_ret(builder, ir_builder_reg_value(builder, res, IR_TYPE_I64_T));

ir_builder_free(builder);
```

### Builder Lifecycle

- [`ir_builder_new()`](src/ir_builder.h:67) - Create a new IR builder
- [`ir_builder_free()`](src/ir_builder.h:74) - Free an IR builder (does NOT free the module)
- [`ir_builder_set_module()`](src/ir_builder.h:81) - Set the target module

### Function Creation

- [`ir_builder_create_func()`](src/ir_builder.h:94) - Create a new function and set it as current
- [`ir_builder_add_param()`](src/ir_builder.h:103) - Add a parameter to the current function
- [`ir_builder_set_func()`](src/ir_builder.h:110) - Set the current function
- [`ir_builder_get_func()`](src/ir_builder.h:117) - Get the current function

### Block Creation & Navigation

- [`ir_builder_create_block()`](src/ir_builder.h:129) - Create a new block in the current function
- [`ir_builder_create_block_and_set()`](src/ir_builder.h:137) - Create a block and set it as insertion point
- [`ir_builder_set_insert_point()`](src/ir_builder.h:144) - Set the insertion point
- [`ir_builder_get_insert_block()`](src/ir_builder.h:151) - Get the current insertion block
- [`ir_builder_is_block_terminated()`](src/ir_builder.h:158) - Check if the current block is terminated

### Register Allocation

- [`ir_builder_alloc_reg()`](src/ir_builder.h:169) - Allocate a new virtual register (%م<n>)
- [`ir_builder_reg_value()`](src/ir_builder.h:178) - Create a register value for a given register number

### Source Location Tracking

- [`ir_builder_set_loc()`](src/ir_builder.h:191) - Set source location for subsequent instructions
- [`ir_builder_clear_loc()`](src/ir_builder.h:197) - Clear source location (for generated code)

### Arithmetic Instructions (العمليات الحسابية)

- [`ir_builder_emit_add()`](src/ir_builder.h:207) - جمع (Addition)
- [`ir_builder_emit_sub()`](src/ir_builder.h:213) - طرح (Subtraction)
- [`ir_builder_emit_mul()`](src/ir_builder.h:219) - ضرب (Multiplication)
- [`ir_builder_emit_div()`](src/ir_builder.h:225) - قسم (Division)
- [`ir_builder_emit_mod()`](src/ir_builder.h:231) - باقي (Modulo)
- [`ir_builder_emit_neg()`](src/ir_builder.h:237) - سالب (Negation)

### Memory Instructions (عمليات الذاكرة)

- [`ir_builder_emit_alloca()`](src/ir_builder.h:249) - حجز (Stack allocation)
- [`ir_builder_emit_load()`](src/ir_builder.h:258) - حمل (Load from memory)
- [`ir_builder_emit_store()`](src/ir_builder.h:267) - خزن (Store to memory)
- [`ir_builder_emit_ptr_offset()`](src/ir_builder.h:274) - إزاحة_مؤشر (Pointer offset: base + index * sizeof(pointee))

### Comparison Instructions (عمليات المقارنة)

- [`ir_builder_emit_cmp()`](src/ir_builder.h:288) - قارن (Compare with predicate)
- [`ir_builder_emit_cmp_eq()`](src/ir_builder.h:293) - يساوي (Equal)
- [`ir_builder_emit_cmp_ne()`](src/ir_builder.h:298) - لا_يساوي (Not equal)
- [`ir_builder_emit_cmp_gt()`](src/ir_builder.h:303) - أكبر (Greater than, signed)
- [`ir_builder_emit_cmp_lt()`](src/ir_builder.h:308) - أصغر (Less than, signed)
- [`ir_builder_emit_cmp_ge()`](src/ir_builder.h:313) - أكبر_أو_يساوي (Greater or equal, signed)
- [`ir_builder_emit_cmp_le()`](src/ir_builder.h:318) - أصغر_أو_يساوي (Less or equal, signed)
- [`ir_builder_emit_cmp_ugt()`](src/ir_builder.h:321) - أكبر (Greater than, unsigned)
- [`ir_builder_emit_cmp_ult()`](src/ir_builder.h:322) - أصغر (Less than, unsigned)
- [`ir_builder_emit_cmp_uge()`](src/ir_builder.h:323) - أكبر_أو_يساوي (Greater or equal, unsigned)
- [`ir_builder_emit_cmp_ule()`](src/ir_builder.h:324) - أصغر_أو_يساوي (Less or equal, unsigned)

### Logical Instructions (العمليات المنطقية)

- [`ir_builder_emit_and()`](src/ir_builder.h:333) - و (Bitwise AND)
- [`ir_builder_emit_or()`](src/ir_builder.h:338) - أو (Bitwise OR)
- [`ir_builder_emit_xor()`](src/ir_builder.h:343) - أو_حصري (Bitwise XOR)
- [`ir_builder_emit_not()`](src/ir_builder.h:348) - نفي (Bitwise NOT)
- [`ir_builder_emit_shl()`](src/ir_builder.h:353) - ازاحة_يسار (Shift left)
- [`ir_builder_emit_shr()`](src/ir_builder.h:358) - ازاحة_يمين (Shift right)

### Control Flow Instructions (عمليات التحكم)

- [`ir_builder_emit_br()`](src/ir_builder.h:369) - قفز (Unconditional branch)
- [`ir_builder_emit_br_cond()`](src/ir_builder.h:378) - قفز_شرط (Conditional branch)
- [`ir_builder_emit_ret()`](src/ir_builder.h:386) - رجوع (Return with value)
- [`ir_builder_emit_ret_void()`](src/ir_builder.h:392) - رجوع (Void return)
- [`ir_builder_emit_ret_int()`](src/ir_builder.h:399) - رجوع with integer constant

### Function Call Instructions (استدعاء الدوال)

- [`ir_builder_emit_call()`](src/ir_builder.h:414) - نداء (Direct function call)
- [`ir_builder_emit_call_indirect()`](src/ir_builder.h:426) - نداء غير مباشر (Indirect function call)
- [`ir_builder_emit_call_void()`](src/ir_builder.h:436) - نداء فراغ (Void function call)

### SSA Instructions (عمليات SSA)

- [`ir_builder_emit_phi()`](src/ir_builder.h:450) - فاي (Phi node)
- [`ir_builder_phi_add_incoming()`](src/ir_builder.h:459) - Add PHI incoming value
- [`ir_builder_emit_copy()`](src/ir_builder.h:469) - نسخ (Copy instruction)

### Type Conversion (تحويل الأنواع)

- [`ir_builder_emit_cast()`](src/ir_builder.h:482) - تحويل (Type cast/conversion)

### Constant Value Helpers

- [`ir_builder_const_int()`](src/ir_builder.h:493) - Create an integer constant value
- [`ir_builder_const_i64()`](src/ir_builder.h:500) - Create an i64 constant value
- [`ir_builder_const_i32()`](src/ir_builder.h:507) - Create an i32 constant value
- [`ir_builder_const_bool()`](src/ir_builder.h:514) - Create an i1 (boolean) constant value
- [`ir_builder_const_string()`](src/ir_builder.h:522) - Create a string constant (adds to module string table)
- [`ir_builder_const_baa_string()`](src/ir_builder.h:523) - Create a Baa string constant

### Global Variables

- [`ir_builder_create_global()`](src/ir_builder.h:537) - Create a global variable
- [`ir_builder_create_global_init()`](src/ir_builder.h:549) - Create a global variable with an initializer
- [`ir_builder_get_global()`](src/ir_builder.h:558) - Get a reference to a global variable

### Control Flow Graph Helpers

These high-level helpers create structured control flow:

- [`ir_builder_create_if_then()`](src/ir_builder.h:573) - Create an if-then structure
- [`ir_builder_create_if_else()`](src/ir_builder.h:588) - Create an if-then-else structure
- [`ir_builder_create_while()`](src/ir_builder.h:604) - Create a while loop structure

Example of structured control flow:

```c
// Create if-then-else structure
IRBlock *then_block, *else_block, *merge_block;
ir_builder_create_if_else(builder, condition, "then", "else", "merge",
                          &then_block, &else_block, &merge_block);

// Emit code in then block
ir_builder_set_insert_point(builder, then_block);
// ... then block instructions ...
ir_builder_emit_br(builder, merge_block);

// Emit code in else block
ir_builder_set_insert_point(builder, else_block);
// ... else block instructions ...
ir_builder_emit_br(builder, merge_block);

// Continue at merge block
ir_builder_set_insert_point(builder, merge_block);
```

### Debugging & Statistics

- [`ir_builder_get_inst_count()`](src/ir_builder.h:619) - Get number of instructions emitted
- [`ir_builder_get_block_count()`](src/ir_builder.h:626) - Get number of blocks created
- [`ir_builder_print_stats()`](src/ir_builder.h:632) - Print builder statistics to stderr

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

IR uses arena allocation for efficiency (ساحة ذاكرة IR):

- All IR objects (types, values, instructions, blocks, functions) are allocated from [`IRModule.arena`](src/ir.h:424).
- Use [`ir_arena_alloc()`](src/ir_arena.h:39) / [`ir_arena_calloc()`](src/ir_arena.h:40) for allocation.
- Use [`ir_arena_strdup()`](src/ir_arena.h:41) for strings.
- Free the entire module with [`ir_module_free()`](src/ir.c) - never free individual objects.

The Def-Use analysis cache ([`IRFunc.def_use`](src/ir.h:357)) is heap-allocated (كاش Def-Use - يُخصّص على heap ويُعاد بناؤه عند التغييرات) and must be rebuilt when IR changes ([`ir_epoch`](src/ir.h:354) tracks modifications).
