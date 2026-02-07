# Baa Internal API Reference

> **Version:** 0.3.2.2 | [← User Guide](USER_GUIDE.md) | [Internals →](INTERNALS.md)

This document details the C functions, enumerations, and structures defined in `src/baa.h`, `src/ir.h`, `src/ir_builder.h`, `src/ir_lower.h`, `src/ir_analysis.h`, `src/ir_pass.h`, `src/ir_dce.h`, `src/ir_copyprop.h`, `src/ir_cse.h`, `src/ir_optimizer.h`, `src/isel.h`, and `src/regalloc.h`.

---

## Table of Contents

- [Lexer Module](#1-lexer-module)
- [Parser Module](#2-parser-module)
- [Semantic Analysis](#3-semantic-analysis)
- [IR Module](#4-ir-module)
- [IR Builder Module](#5-ir-builder-module)
- [IR Lowering Module](#6-ir-lowering-module)
- [IR Optimization Passes](#7-ir-optimization-passes)
- [Instruction Selection Module](#8-instruction-selection-module-v0321)
- [Register Allocation Module](#9-register-allocation-module-v0322)
- [Codegen Module](#10-codegen-module)
- [Diagnostic System](#11-diagnostic-system)
- [Symbol Table](#12-symbol-table)
- [Updater](#13-updater)
- [Data Structures](#14-data-structures)

---

## 1. Lexer Module

Handles UTF-8 string processing, tokenization, and **preprocessing**.

### `lexer_init`

```c
void lexer_init(Lexer* l, char* src, const char* filename)
```

Initializes a new Lexer instance.

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Pointer to Lexer struct to initialize |
| `src` | `char*` | Source code buffer (mutable for internal pointer usage) |
| `filename` | `const char*` | Name of the file (for error reporting) |

**Behavior:**

- Initializes the state stack depth to 0.
- Sets up position pointers (`cur_char`) to start of source.
- Detects and skips UTF-8 BOM (Byte Order Mark: `0xEF 0xBB 0xBF`) if present.
- Initializes line counter to 1.
- Resets preprocessor state (`macro_count = 0`, `skipping = false`).

---

### `lexer_next_token`

```c
Token lexer_next_token(Lexer* l)
```

Consumes input and returns the next valid token. **Handles preprocessor directives internally.**

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Initialized Lexer |

**Returns:** `Token` struct. Returns `TOKEN_EOF` at end of file.

**Behavior:**

- Skips whitespace and comments (`//`).
- **Preprocessor:**
  - Handles `#تعريف <name> <value>` (define) to register macros.
  - Handles `#إذا_عرف <name>` (ifdef) to conditionally compile code.
  - Handles `#وإلا` (else) and `#نهاية` (endif) for conditional blocks.
  - Handles `#تضمين "file"` (include) to push new files onto the stack.
  - Handles `#الغاء_تعريف <name>` (undefine) to remove macro definitions.
  - Replaces identifiers with macro values if defined.
- Identifies keywords, literals, and operators.
- Handles Arabic semicolon `؛`.
- Normalizes Arabic-Indic digits (`٠`-`٩`).

---

## 2. Parser Module

Handles syntactic analysis and AST construction.

### `parse`

```c
Node* parse(Lexer* l)
```

Entry point for the parsing phase.

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Initialized Lexer |

**Returns:** Pointer to root `Node` (type `NODE_PROGRAM`).

**Behavior:**

- Initializes internal `Parser` state with 2-token lookahead (current + next)
- Parses list of declarations (global variables or functions)
- Implements operator precedence climbing for expressions
- Builds linked-list AST structure

**Example:**

```c
Lexer lexer;
lexer_init(&lexer, source_code, "filename.baa");
Node* ast = parse(&lexer);
// Use ast...
```

---

## 3. Semantic Analysis

Handles type checking, symbol resolution, and **constant validation**.

### `analyze`

```c
bool analyze(Node* program)
```

Runs the semantic pass on the AST.

| Parameter | Type | Description |
|-----------|------|-------------|
| `program` | `Node*` | The root AST node |

**Returns:** `true` if valid, `false` if errors were found.

**Behavior:**

- **Type Checking**: Ensures type compatibility in assignments and operations.
- **Symbol Resolution**: Verifies variables are declared before use.
- **Scope Validation**: Tracks global vs local scope and prevents redefinitions.
- **Constant Checking** (v0.2.7+): Prevents reassignment of `ثابت` variables.
- **Control Flow Validation**: Ensures `break`/`continue` are only used within loops/switches.
- Traverses the entire tree.
- Reports errors using `error_report`.
- Does not modify the AST, only validates it.

**Constant Validation Rules:**

- Constants must be initialized at declaration time.
- Reassigning a constant produces: `Cannot reassign constant '<name>'`.
- Modifying constant array elements produces: `Cannot modify constant array '<name>'`.

---

## 4. IR Module (v0.3.0+)

The IR Module (`src/ir.h`, `src/ir.c`) provides Baa's Arabic-first Intermediate Representation.

### 4.1. Type Construction

#### `ir_type_ptr`

```c
IRType* ir_type_ptr(IRType* pointee)
```

Creates a pointer type.

| Parameter | Type | Description |
|-----------|------|-------------|
| `pointee` | `IRType*` | The type being pointed to |

**Returns:** New `IRType*` with `kind = IR_TYPE_PTR`.

---

#### `ir_type_array`

```c
IRType* ir_type_array(IRType* element, int count)
```

Creates an array type.

| Parameter | Type | Description |
|-----------|------|-------------|
| `element` | `IRType*` | Element type |
| `count` | `int` | Number of elements |

**Returns:** New `IRType*` with `kind = IR_TYPE_ARRAY`.

---

### 4.2. Value Construction

#### `ir_value_reg`

```c
IRValue* ir_value_reg(int reg_num, IRType* type)
```

Creates a virtual register reference.

| Parameter | Type | Description |
|-----------|------|-------------|
| `reg_num` | `int` | Register number (e.g., 0 for `%م٠`) |
| `type` | `IRType*` | Type of the value in the register |

---

#### `ir_value_const_int`

```c
IRValue* ir_value_const_int(int64_t value, IRType* type)
```

Creates an integer constant.

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `int64_t` | The constant value |
| `type` | `IRType*` | Type (defaults to `IR_TYPE_I64_T` if NULL) |

---

#### `ir_value_const_str`

```c
IRValue* ir_value_const_str(const char* str, int id)
```

Creates a string constant reference.

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `const char*` | String content |
| `id` | `int` | String table ID |

---

### 4.3. Instruction Construction

#### `ir_inst_binary`

```c
IRInst* ir_inst_binary(IROp op, IRType* type, int dest, IRValue* lhs, IRValue* rhs)
```

Creates a binary operation (add, sub, mul, div, etc.).

| Parameter | Type | Description |
|-----------|------|-------------|
| `op` | `IROp` | Operation (e.g., `IR_OP_ADD`) |
| `type` | `IRType*` | Result type |
| `dest` | `int` | Destination register number |
| `lhs` | `IRValue*` | Left operand |
| `rhs` | `IRValue*` | Right operand |

---

#### `ir_inst_cmp`

```c
IRInst* ir_inst_cmp(IRCmpPred pred, int dest, IRValue* lhs, IRValue* rhs)
```

Creates a comparison instruction.

| Parameter | Type | Description |
|-----------|------|-------------|
| `pred` | `IRCmpPred` | Comparison predicate (e.g., `IR_CMP_EQ`) |
| `dest` | `int` | Destination register (result is `i1`) |
| `lhs` | `IRValue*` | Left operand |
| `rhs` | `IRValue*` | Right operand |

---

#### `ir_inst_load`

```c
IRInst* ir_inst_load(IRType* type, int dest, IRValue* ptr)
```

Creates a memory load instruction.

| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | `IRType*` | Type of value to load |
| `dest` | `int` | Destination register |
| `ptr` | `IRValue*` | Pointer to load from |

---

#### `ir_inst_store`

```c
IRInst* ir_inst_store(IRValue* value, IRValue* ptr)
```

Creates a memory store instruction. No destination register.

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `IRValue*` | Value to store |
| `ptr` | `IRValue*` | Pointer to store to |

---

#### `ir_inst_br`

```c
IRInst* ir_inst_br(IRBlock* target)
```

Creates an unconditional branch.

| Parameter | Type | Description |
|-----------|------|-------------|
| `target` | `IRBlock*` | Target basic block |

---

#### `ir_inst_br_cond`

```c
IRInst* ir_inst_br_cond(IRValue* cond, IRBlock* if_true, IRBlock* if_false)
```

Creates a conditional branch.

| Parameter | Type | Description |
|-----------|------|-------------|
| `cond` | `IRValue*` | Condition (must be `i1`) |
| `if_true` | `IRBlock*` | Target if condition is true |
| `if_false` | `IRBlock*` | Target if condition is false |

---

#### `ir_inst_ret`

```c
IRInst* ir_inst_ret(IRValue* value)
```

Creates a return instruction.

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `IRValue*` | Return value (NULL for void) |

---

#### `ir_inst_call`

```c
IRInst* ir_inst_call(const char* target, IRType* ret_type, int dest, IRValue** args, int arg_count)
```

Creates a function call.

| Parameter | Type | Description |
|-----------|------|-------------|
| `target` | `const char*` | Function name |
| `ret_type` | `IRType*` | Return type |
| `dest` | `int` | Destination register (-1 for void) |
| `args` | `IRValue**` | Array of argument values |
| `arg_count` | `int` | Number of arguments |

---

#### `ir_inst_phi`

```c
IRInst* ir_inst_phi(IRType* type, int dest)
```

Creates a phi node for SSA.

| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | `IRType*` | Type of the phi result |
| `dest` | `int` | Destination register |

Use `ir_inst_phi_add()` to add incoming values.

---

### 4.4. Block & Function Construction

#### `ir_block_new`

```c
IRBlock* ir_block_new(const char* label, int id)
```

Creates a new basic block.

| Parameter | Type | Description |
|-----------|------|-------------|
| `label` | `const char*` | Block label (Arabic, e.g., "بداية") |
| `id` | `int` | Numeric ID for internal use |

---

#### `ir_block_append`

```c
void ir_block_append(IRBlock* block, IRInst* inst)
```

Appends an instruction to a block.

---

#### `ir_func_new`

```c
IRFunc* ir_func_new(const char* name, IRType* ret_type)
```

Creates a new function.

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const char*` | Function name |
| `ret_type` | `IRType*` | Return type |

---

#### `ir_func_alloc_reg`

```c
int ir_func_alloc_reg(IRFunc* func)
```

Allocates a new virtual register number.

**Returns:** The next available register number.

---

#### `ir_func_new_block`

```c
IRBlock* ir_func_new_block(IRFunc* func, const char* label)
```

Creates and adds a new block to a function.

---

### 4.5. Module Construction

#### `ir_module_new`

```c
IRModule* ir_module_new(const char* name)
```

Creates a new IR module.

---

#### `ir_module_add_func`

```c
void ir_module_add_func(IRModule* module, IRFunc* func)
```

Adds a function to the module.

---

#### `ir_module_add_string`

```c
int ir_module_add_string(IRModule* module, const char* str)
```

Adds a string to the string table.

**Returns:** String ID for referencing in IR.

---

### 4.6. Printing & Debugging

#### `ir_module_print`

```c
void ir_module_print(IRModule* module, FILE* out, int use_arabic)
```

Prints the IR module to a file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module` | `IRModule*` | The module to print |
| `out` | `FILE*` | Output file handle |
| `use_arabic` | `int` | 1 for Arabic names, 0 for English |

---

#### `ir_module_dump`

```c
void ir_module_dump(IRModule* module, const char* filename, int use_arabic)
```

Convenience wrapper that opens a file and prints the module.

---

### Compatibility Printer Wrappers (v0.3.0.6)

These wrappers exist to match the roadmap task naming (`ir_print_*`) and print to `stdout` using Arabic output:

#### `ir_print_func`

```c
void ir_print_func(IRFunc* func)
```

Prints a single function to stdout (Arabic mode). Implemented in [`ir_print_func()`](src/ir.c:1715).

---

#### `ir_print_block`

```c
void ir_print_block(IRBlock* block)
```

Prints a single basic block to stdout (Arabic mode). Implemented in [`ir_print_block()`](src/ir.c:1711).

---

#### `ir_print_inst`

```c
void ir_print_inst(IRInst* inst)
```

Prints a single instruction to stdout (Arabic mode). Implemented in [`ir_print_inst()`](src/ir.c:1707).

---

### 4.7. Arabic Name Conversion

#### `ir_op_to_arabic`

```c
const char* ir_op_to_arabic(IROp op)
```

Converts an opcode to its Arabic name.

**Example:** `ir_op_to_arabic(IR_OP_ADD)` returns `"جمع"`.

---

#### `ir_cmp_pred_to_arabic`

```c
const char* ir_cmp_pred_to_arabic(IRCmpPred pred)
```

Converts a comparison predicate to its Arabic name.

**Example:** `ir_cmp_pred_to_arabic(IR_CMP_EQ)` returns `"يساوي"`.

---

#### `ir_type_to_arabic`

```c
const char* ir_type_to_arabic(IRType* type)
```

Converts a type to its Arabic representation.

**Example:** `ir_type_to_arabic(IR_TYPE_I64_T)` returns `"ص٦٤"`.

---

#### `int_to_arabic_numerals`

```c
char* int_to_arabic_numerals(int n, char* buf)
```

Converts an integer to Arabic-Indic numerals.

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `int` | Number to convert |
| `buf` | `char*` | Output buffer (must be at least 32 bytes) |

**Example:** `int_to_arabic_numerals(42, buf)` returns `"٤٢"`.

---

### 4.8. Predefined Types

Global type singletons for convenience:

```c
extern IRType* IR_TYPE_VOID_T;   // فراغ
extern IRType* IR_TYPE_I1_T;     // ص١
extern IRType* IR_TYPE_I8_T;     // ص٨
extern IRType* IR_TYPE_I16_T;    // ص١٦
extern IRType* IR_TYPE_I32_T;    // ص٣٢
extern IRType* IR_TYPE_I64_T;    // ص٦٤
```

---

### 4.9. IR Analysis Module (v0.3.1.1)

Provides analysis utilities over IR functions/blocks:

- CFG validation
- Predecessor rebuilding
- Dominator tree + dominance frontier

#### `ir_func_validate_cfg`

```c
bool ir_func_validate_cfg(IRFunc* func);
```

Validates basic CFG well-formedness for a function:

- all blocks have terminators
- terminators have well-formed operands (branch targets are blocks, etc.)

---

#### `ir_module_validate_cfg`

```c
bool ir_module_validate_cfg(IRModule* module);
```

Validates CFG for all functions in a module.

---

#### `ir_func_rebuild_preds`

```c
void ir_func_rebuild_preds(IRFunc* func);
```

Clears and rebuilds `succs[]` and `preds[]` for all blocks based on terminator instructions.

---

#### `ir_module_rebuild_preds`

```c
void ir_module_rebuild_preds(IRModule* module);
```

Rebuilds CFG edges for all functions in a module.

---

#### `ir_func_compute_dominators`

```c
void ir_func_compute_dominators(IRFunc* func);
```

Computes immediate dominators (`idom`) and dominance frontier sets (`dom_frontier`) for a function.

---

#### `ir_module_compute_dominators`

```c
void ir_module_compute_dominators(IRModule* module);
```

Computes dominators for all functions in a module.

---

### 4.10. IR Pass Module (v0.3.1.1)

Defines a minimal pass interface for the optimizer pipeline.

#### `IRPass`

```c
typedef bool (*IRPassRunFn)(IRModule* module);

typedef struct IRPass {
    const char* name;
    IRPassRunFn run;
} IRPass;
```

---

#### `ir_pass_run`

```c
bool ir_pass_run(IRPass* pass, IRModule* module);
```

Runs a pass (NULL-safe). Returns `true` if the pass changed the module.

---

## 5. IR Builder Module (v0.3.0.2+)

The IR Builder Module (`src/ir_builder.h`, `src/ir_builder.c`) provides a convenient builder pattern API for constructing IR.

### 5.1. Builder Context

#### `IRBuilder` struct

```c
typedef struct IRBuilder {
    IRModule* module;           // Target module
    IRFunc* current_func;       // Current function being built
    IRBlock* insert_block;      // Current insertion block
    
    // Source location tracking
    const char* src_file;
    int src_line;
    int src_col;
    
    // Statistics
    int insts_emitted;
    int blocks_created;
} IRBuilder;
```

### 5.2. Lifecycle Functions

#### `ir_builder_new`

```c
IRBuilder* ir_builder_new(IRModule* module)
```

Creates a new IR builder.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module` | `IRModule*` | Target module (can be NULL) |

**Returns:** New builder instance.

---

#### `ir_builder_free`

```c
void ir_builder_free(IRBuilder* builder)
```

Frees an IR builder. Does NOT free the module or any IR created.

---

### 5.3. Function & Block Creation

#### `ir_builder_create_func`

```c
IRFunc* ir_builder_create_func(IRBuilder* builder, const char* name, IRType* ret_type)
```

Creates a new function and sets it as current.

| Parameter | Type | Description |
|-----------|------|-------------|
| `builder` | `IRBuilder*` | The builder |
| `name` | `const char*` | Function name |
| `ret_type` | `IRType*` | Return type |

**Returns:** The new function.

---

#### `ir_builder_create_block`

```c
IRBlock* ir_builder_create_block(IRBuilder* builder, const char* label)
```

Creates a new block in the current function.

| Parameter | Type | Description |
|-----------|------|-------------|
| `builder` | `IRBuilder*` | The builder |
| `label` | `const char*` | Block label (Arabic, e.g., "بداية") |

---

#### `ir_builder_set_insert_point`

```c
void ir_builder_set_insert_point(IRBuilder* builder, IRBlock* block)
```

Sets the insertion point. All subsequent emit functions will append to this block.

---

### 5.4. Register Allocation

#### `ir_builder_alloc_reg`

```c
int ir_builder_alloc_reg(IRBuilder* builder)
```

Allocates a new virtual register.

**Returns:** New register number (`%م<n>`).

---

### 5.5. Emit Functions - Arithmetic

#### `ir_builder_emit_add`

```c
int ir_builder_emit_add(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emits: `%dest = جمع type lhs, rhs`

**Returns:** Destination register number.

---

#### `ir_builder_emit_sub`

```c
int ir_builder_emit_sub(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emits: `%dest = طرح type lhs, rhs`

---

#### `ir_builder_emit_mul`

```c
int ir_builder_emit_mul(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emits: `%dest = ضرب type lhs, rhs`

---

#### `ir_builder_emit_div`

```c
int ir_builder_emit_div(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emits: `%dest = قسم type lhs, rhs`

---

### 5.6. Emit Functions - Memory

#### `ir_builder_emit_alloca`

```c
int ir_builder_emit_alloca(IRBuilder* builder, IRType* type)
```

Emits: `%dest = حجز type`

**Returns:** Destination register (pointer to allocated space).

---

#### `ir_builder_emit_load`

```c
int ir_builder_emit_load(IRBuilder* builder, IRType* type, IRValue* ptr)
```

Emits: `%dest = حمل type, ptr`

---

#### `ir_builder_emit_store`

```c
void ir_builder_emit_store(IRBuilder* builder, IRValue* value, IRValue* ptr)
```

Emits: `خزن value, ptr`

**Note:** Store has no result register.

---

### 5.7. Emit Functions - Comparison

#### `ir_builder_emit_cmp`

```c
int ir_builder_emit_cmp(IRBuilder* builder, IRCmpPred pred, IRValue* lhs, IRValue* rhs)
```

Emits: `%dest = قارن pred lhs, rhs`

**Returns:** Destination register (i1/boolean result).

---

#### Convenience Comparison Functions

```c
int ir_builder_emit_cmp_eq(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // يساوي
int ir_builder_emit_cmp_ne(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // لا_يساوي
int ir_builder_emit_cmp_gt(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أكبر
int ir_builder_emit_cmp_lt(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أصغر
int ir_builder_emit_cmp_ge(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أكبر_أو_يساوي
int ir_builder_emit_cmp_le(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أصغر_أو_يساوي
```

---

### 5.8. Emit Functions - Control Flow

#### `ir_builder_emit_br`

```c
void ir_builder_emit_br(IRBuilder* builder, IRBlock* target)
```

Emits: `قفز target`

Automatically updates CFG successor edges.

---

#### `ir_builder_emit_br_cond`

```c
void ir_builder_emit_br_cond(IRBuilder* builder, IRValue* cond, IRBlock* if_true, IRBlock* if_false)
```

Emits: `قفز_شرط cond, if_true, if_false`

Automatically updates CFG successor edges.

---

#### `ir_builder_emit_ret`

```c
void ir_builder_emit_ret(IRBuilder* builder, IRValue* value)
```

Emits: `رجوع value`

---

#### `ir_builder_emit_ret_void`

```c
void ir_builder_emit_ret_void(IRBuilder* builder)
```

Emits: `رجوع` (void return)

---

### 5.9. Emit Functions - Calls

#### `ir_builder_emit_call`

```c
int ir_builder_emit_call(IRBuilder* builder, const char* target, IRType* ret_type, IRValue** args, int arg_count)
```

Emits: `%dest = نداء ret_type @target(args...)`

**Returns:** Destination register (-1 for void calls).

---

#### `ir_builder_emit_call_void`

```c
void ir_builder_emit_call_void(IRBuilder* builder, const char* target, IRValue** args, int arg_count)
```

Emits: `نداء فراغ @target(args...)`

---

### 5.10. Control Flow Helpers

#### `ir_builder_create_if_then`

```c
void ir_builder_create_if_then(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* merge_label,
                                IRBlock** then_block, IRBlock** merge_block)
```

Creates an if-then structure with blocks and emits conditional branch.

---

#### `ir_builder_create_if_else`

```c
void ir_builder_create_if_else(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* else_label,
                                const char* merge_label,
                                IRBlock** then_block, IRBlock** else_block,
                                IRBlock** merge_block)
```

Creates an if-then-else structure with blocks and emits conditional branch.

---

#### `ir_builder_create_while`

```c
void ir_builder_create_while(IRBuilder* builder,
                              const char* header_label, const char* body_label,
                              const char* exit_label,
                              IRBlock** header_block, IRBlock** body_block,
                              IRBlock** exit_block)
```

Creates a while loop structure with header, body, and exit blocks.

---

### 5.11. Constant Helpers

```c
IRValue* ir_builder_const_int(int64_t value);      // i64 constant
IRValue* ir_builder_const_i64(int64_t value);      // i64 constant
IRValue* ir_builder_const_i32(int32_t value);      // i32 constant
IRValue* ir_builder_const_bool(int value);         // i1 constant
IRValue* ir_builder_const_string(IRBuilder* builder, const char* str);  // String constant
```

---

### 5.12. Example Usage

```c
// Create module and builder
IRModule* module = ir_module_new("test");
IRBuilder* builder = ir_builder_new(module);

// Create function: صحيح الرئيسية()
IRFunc* func = ir_builder_create_func(builder, "الرئيسية", IR_TYPE_I64_T);

// Create entry block
IRBlock* entry = ir_builder_create_block(builder, "بداية");
ir_builder_set_insert_point(builder, entry);

// Emit: %م٠ = حجز ص٦٤
int ptr = ir_builder_emit_alloca(builder, IR_TYPE_I64_T);

// Emit: خزن ص٦٤ ١٠, %م٠
ir_builder_emit_store(builder, ir_builder_const_i64(10), ir_value_reg(ptr, ir_type_ptr(IR_TYPE_I64_T)));

// Emit: %م١ = حمل ص٦٤ %م٠
int val = ir_builder_emit_load(builder, IR_TYPE_I64_T, ir_value_reg(ptr, ir_type_ptr(IR_TYPE_I64_T)));

// Emit: رجوع ص٦٤ %م١
ir_builder_emit_ret(builder, ir_value_reg(val, IR_TYPE_I64_T));

// Print IR
ir_module_print(module, stdout, 1);

// Cleanup
ir_builder_free(builder);
ir_module_free(module);
```

---

## 6. IR Lowering Module (v0.3.0.3+)

The IR Lowering module lowers validated AST nodes into Baa IR using the IR Builder.

Files:

- [`src/ir_lower.h`](src/ir_lower.h)
- [`src/ir_lower.c`](src/ir_lower.c)

### 6.1. `IRLowerCtx`

```c
typedef struct IRLowerCtx {
    IRBuilder* builder;

    IRLowerBinding locals[256];
    int local_count;

    int label_counter;

    struct IRBlock* break_targets[64];
    struct IRBlock* continue_targets[64];
    int cf_depth;
} IRLowerCtx;
```

A small context object used during lowering:

- Holds the active `IRBuilder` insertion point
- Tracks local variable bindings (name → pointer register)

---

### 6.2. `ir_lower_ctx_init`

```c
void ir_lower_ctx_init(IRLowerCtx* ctx, IRBuilder* builder);
```

Initializes the lowering context.

---

### 6.3. `ir_lower_bind_local`

```c
void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type);
```

Binds a local variable name to its `حجز` pointer register. This enables `NODE_VAR_REF` lowering to emit `حمل` from the pointer.

---

### 6.4. `lower_expr`

```c
IRValue* lower_expr(IRLowerCtx* ctx, Node* expr);
```

Main expression lowering dispatcher.

Currently supports:

- `NODE_INT` → immediate constant
- `NODE_VAR_REF` → `حمل` (load)
- `NODE_BIN_OP` → arithmetic (`جمع`/`طرح`/`ضرب`/`قسم`/`باقي`), comparisons (`قارن`), and boolean ops (`و`/`أو`)
- `NODE_UNARY_OP` → `سالب` and boolean `نفي`
- `NODE_CALL_EXPR` → `نداء`

---

### 6.5. `lower_stmt`

```c
void lower_stmt(IRLowerCtx* ctx, Node* stmt);
```

Lowers a single AST statement into IR using the active builder insertion point.

Supported statements (v0.3.0.5):

- `NODE_VAR_DECL`: `حجز` + `خزن` and bind local
- `NODE_ASSIGN`: `خزن` into existing local
- `NODE_RETURN`: `رجوع`
- `NODE_PRINT`: `نداء @اطبع(...)`
- `NODE_READ`: `نداء @اقرأ(%ptr)`
- `NODE_CALL_STMT`: lowered through call expression path
- `NODE_IF`, `NODE_WHILE`, `NODE_FOR`, `NODE_SWITCH`
- `NODE_BREAK`, `NODE_CONTINUE`

---

### 6.7. `ir_lower_program` (v0.3.0.7)

```c
IRModule* ir_lower_program(Node* program, const char* module_name);
```

Top-level entry point for the driver: converts a validated `NODE_PROGRAM` AST into a fully-populated `IRModule`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `program` | `Node*` | Root AST node (must be `NODE_PROGRAM`) |
| `module_name` | `const char*` | Optional module name (usually filename) |

**Returns:** Newly allocated `IRModule` (caller owns; free with `ir_module_free()`).

**Behavior:**

1. Creates a new `IRModule` and `IRBuilder`
2. Walks top-level declarations:
   - Global variables (`NODE_VAR_DECL` with `is_global`) → `ir_builder_create_global_init()`
   - Functions (`NODE_FUNC_DEF`) → `ir_builder_create_func()` + parameter spilling + `lower_stmt()`
3. Returns the fully-lowered module

---

### 6.6. `lower_stmt_list`

```c
void lower_stmt_list(IRLowerCtx* ctx, Node* first_stmt);
```

Lowers a linked list of statements (e.g., the statements list inside `NODE_BLOCK`).

---

## 7. IR Optimization Passes

### 7.1. Constant Folding (طي_الثوابت)

#### `ir_constfold_run`

```c
bool ir_constfold_run(IRModule* module)
```

Runs the constant folding pass on the given IR module. Folds arithmetic and comparison instructions with constant operands, replaces register uses, and removes folded instructions.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_CONSTFOLD`

```c
extern IRPass IR_PASS_CONSTFOLD;
```

Descriptor for the constant folding pass, usable with the IR optimizer pipeline.

---

### 7.2. Dead Code Elimination (حذف_الميت)

#### `ir_dce_run`

```c
bool ir_dce_run(IRModule* module)
```

Runs dead code elimination on the given IR module. Removes:

- Dead SSA instructions whose destination register is never used (only for instructions with no side effects)
- Unreachable basic blocks (not reachable from the function entry)

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_DCE`

```c
extern IRPass IR_PASS_DCE;
```

Descriptor for the dead code elimination pass, usable with the IR optimizer pipeline.

---

### 7.3. Copy Propagation (نشر_النسخ)

#### `ir_copyprop_run`

```c
bool ir_copyprop_run(IRModule* module)
```

Runs copy propagation on the given IR module. Replaces uses of registers defined by copy instructions (`نسخ`) with their original source values and removes redundant copy instructions.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_COPYPROP`

```c
extern IRPass IR_PASS_COPYPROP;
```

Descriptor for the copy propagation pass, usable with the IR optimizer pipeline.

---

### 7.4. Common Subexpression Elimination (حذف_المكرر)

#### `ir_cse_run`

```c
bool ir_cse_run(IRModule* module)
```

Runs common subexpression elimination on the given IR module. Detects duplicate expressions (same opcode + same operands), replaces uses of duplicates with the original result, and removes redundant instructions.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

**Eligible Operations:**

- Arithmetic: جمع، طرح، ضرب، قسم، باقي
- Comparisons: قارن
- Logical: و، أو، نفي

**NOT Eligible (side effects):**

- Memory: حجز، حمل، خزن
- Control: نداء، فاي، terminators

#### `IR_PASS_CSE`

```c
extern IRPass IR_PASS_CSE;
```

Descriptor for the common subexpression elimination pass, usable with the IR optimizer pipeline.

---

### 7.5. Optimization Pipeline (v0.3.1.6)

#### `OptLevel`

```c
typedef enum {
    OPT_LEVEL_0 = 0,  // No optimization (debug mode)
    OPT_LEVEL_1 = 1,  // Basic optimizations (default)
    OPT_LEVEL_2 = 2   // Full optimizations
} OptLevel;
```

Optimization level enum controlling which passes are run:
- **O0:** No optimization (for debugging).
- **O1:** Basic optimizations (constfold, copyprop, dce).
- **O2:** Full optimizations (+ CSE, fixpoint iteration).

#### `ir_optimizer_run`

```c
bool ir_optimizer_run(IRModule* module, OptLevel level)
```

Runs the optimization pipeline on the given IR module.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |
| `level`   | `OptLevel` | Optimization level (O0, O1, O2) |

**Returns:** `true` if any optimization was performed, `false` otherwise.

**Pass ordering:**
1. Constant Folding (طي_الثوابت)
2. Copy Propagation (نشر_النسخ)
3. CSE (حذف_المكرر) — O2 only
4. Dead Code Elimination (حذف_الميت)

**Fixpoint iteration:** Passes repeat until no changes (max 10 iterations).

#### `ir_optimizer_level_name`

```c
const char* ir_optimizer_level_name(OptLevel level)
```

Returns the string representation of an optimization level ("O0", "O1", "O2").

---

## 8. Instruction Selection Module (v0.3.2.1)

The Instruction Selection module (`src/isel.h`, `src/isel.c`) converts IR to abstract machine instructions for x86-64.

### 8.1. Operand Constructors

#### `mach_op_vreg`

```c
MachineOperand mach_op_vreg(int vreg, int bits)
```

Creates a virtual register operand.

| Parameter | Type | Description |
|-----------|------|-------------|
| `vreg` | `int` | Virtual register number |
| `bits` | `int` | Operand size in bits (8, 16, 32, 64) |

---

#### `mach_op_imm`

```c
MachineOperand mach_op_imm(int64_t imm, int bits)
```

Creates an immediate value operand.

| Parameter | Type | Description |
|-----------|------|-------------|
| `imm` | `int64_t` | Immediate value |
| `bits` | `int` | Operand size in bits |

---

#### `mach_op_mem`

```c
MachineOperand mach_op_mem(int base_vreg, int32_t offset, int bits)
```

Creates a memory operand `[base + offset]`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `base_vreg` | `int` | Base register number |
| `offset` | `int32_t` | Byte offset from base |
| `bits` | `int` | Access size in bits |

---

#### `mach_op_label`

```c
MachineOperand mach_op_label(int label_id)
```

Creates a block label operand (for jumps).

---

#### `mach_op_global`

```c
MachineOperand mach_op_global(const char* name)
```

Creates a global variable reference operand.

---

#### `mach_op_func`

```c
MachineOperand mach_op_func(const char* name)
```

Creates a function reference operand (for calls).

---

#### `mach_op_none`

```c
MachineOperand mach_op_none(void)
```

Creates an empty (no-operand) operand.

---

### 8.2. Instruction Construction

#### `mach_inst_new`

```c
MachineInst* mach_inst_new(MachineOp op, MachineOperand dst,
                           MachineOperand src1, MachineOperand src2)
```

Creates a new machine instruction.

| Parameter | Type | Description |
|-----------|------|-------------|
| `op` | `MachineOp` | x86-64 opcode (e.g., `MACH_ADD`) |
| `dst` | `MachineOperand` | Destination operand |
| `src1` | `MachineOperand` | First source operand |
| `src2` | `MachineOperand` | Second source operand (use `mach_op_none()` if unused) |

**Returns:** New `MachineInst*` (caller owns).

---

#### `mach_inst_free`

```c
void mach_inst_free(MachineInst* inst)
```

Frees a machine instruction and its comment string.

---

### 8.3. Block Construction

#### `mach_block_new`

```c
MachineBlock* mach_block_new(const char* label, int id)
```

Creates a new machine block.

---

#### `mach_block_append`

```c
void mach_block_append(MachineBlock* block, MachineInst* inst)
```

Appends an instruction to the end of a block's doubly-linked list.

---

#### `mach_block_free`

```c
void mach_block_free(MachineBlock* block)
```

Frees a block and all its instructions.

---

### 8.4. Function Construction

#### `mach_func_new`

```c
MachineFunc* mach_func_new(const char* name)
```

Creates a new machine function.

---

#### `mach_func_alloc_vreg`

```c
int mach_func_alloc_vreg(MachineFunc* func)
```

Allocates the next available virtual register number.

**Returns:** New virtual register number.

---

#### `mach_func_add_block`

```c
void mach_func_add_block(MachineFunc* func, MachineBlock* block)
```

Adds a block to a function (sets as entry if first block).

---

#### `mach_func_free`

```c
void mach_func_free(MachineFunc* func)
```

Frees a function and all its blocks.

---

### 8.5. Module Construction

#### `mach_module_new`

```c
MachineModule* mach_module_new(const char* name)
```

Creates a new machine module.

---

#### `mach_module_add_func`

```c
void mach_module_add_func(MachineModule* module, MachineFunc* func)
```

Adds a function to a module.

---

#### `mach_module_free`

```c
void mach_module_free(MachineModule* module)
```

Frees a module and all its functions. Does NOT free referenced IR globals/strings.

---

### 8.6. Instruction Selection Entry Point

#### `isel_run`

```c
MachineModule* isel_run(IRModule* ir_module)
```

Converts an entire IR module to machine representation.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ir_module` | `IRModule*` | Source IR module (after optimization) |

**Returns:** New `MachineModule*` (caller owns; free with `mach_module_free()`), or `NULL` on failure.

**Behavior:**

1. Creates a `MachineModule` and walks all IR functions
2. For each IR function, creates a `MachineFunc` with matching blocks
3. For each IR instruction, emits one or more `MachineInst` nodes using pattern matching
4. Inlines immediate values where x86-64 permits
5. Uses Windows x64 ABI conventions for calls/returns

---

### 8.7. Print Functions

#### `mach_op_to_string`

```c
const char* mach_op_to_string(MachineOp op)
```

Returns the string name of a machine opcode (e.g., `"ADD"`, `"MOV"`).

---

#### `mach_operand_print`

```c
void mach_operand_print(MachineOperand* op, FILE* out)
```

Prints a single operand (e.g., `%v5`, `$42`, `[%v-1 - 8]`, `@main`).

---

#### `mach_inst_print` / `mach_block_print` / `mach_func_print` / `mach_module_print`

```c
void mach_inst_print(MachineInst* inst, FILE* out);
void mach_block_print(MachineBlock* block, FILE* out);
void mach_func_print(MachineFunc* func, FILE* out);
void mach_module_print(MachineModule* module, FILE* out);
```

Hierarchical print functions for debugging machine IR output.

---

## 9. Register Allocation Module (v0.3.2.2)

Maps virtual registers from instruction selection to physical x86-64 registers using linear scan allocation.

**Header:** [`src/regalloc.h`](../src/regalloc.h) | **Implementation:** [`src/regalloc.c`](../src/regalloc.c)

### 9.1. Enumerations

#### `PhysReg`

```c
typedef enum {
    PHYS_RAX = 0,  PHYS_RCX = 1,  PHYS_RDX = 2,  PHYS_RBX = 3,
    PHYS_RSP = 4,  PHYS_RBP = 5,  PHYS_RSI = 6,  PHYS_RDI = 7,
    PHYS_R8  = 8,  PHYS_R9  = 9,  PHYS_R10 = 10, PHYS_R11 = 11,
    PHYS_R12 = 12, PHYS_R13 = 13, PHYS_R14 = 14, PHYS_R15 = 15,
    PHYS_REG_COUNT = 16,
    PHYS_NONE = -1
} PhysReg;
```

x86-64 physical register numbering. RSP and RBP are always reserved and never allocated.

### 9.2. Data Structures

#### `LiveInterval`

```c
typedef struct LiveInterval {
    int vreg;           // Virtual register number
    int start;          // Start position (first def)
    int end;            // End position (last use)
    PhysReg phys_reg;   // Assigned physical register (PHYS_NONE if spilled)
    bool spilled;       // Whether spilled to stack
    int spill_offset;   // Stack offset relative to RBP
} LiveInterval;
```

Represents the live range of a virtual register from its definition to its last use.

#### `BlockLiveness`

```c
typedef struct BlockLiveness {
    int block_id;
    uint64_t* def;      // Registers defined in this block
    uint64_t* use;      // Registers used before definition in this block
    uint64_t* live_in;  // Registers live at block entry
    uint64_t* live_out; // Registers live at block exit
} BlockLiveness;
```

Per-block liveness bitsets used in iterative dataflow analysis.

#### `RegAllocCtx`

```c
typedef struct RegAllocCtx {
    MachineFunc* func;
    int total_insts;
    MachineInst** inst_map;
    BlockLiveness* block_live;
    int block_count;
    LiveInterval* intervals;
    int interval_count;
    int interval_capacity;
    int bitset_words;
    int max_vreg;
    PhysReg* vreg_to_phys;
    bool* vreg_spilled;
    int* vreg_spill_offset;
    int next_spill_offset;
    int spill_count;
    bool callee_saved_used[PHYS_REG_COUNT];
} RegAllocCtx;
```

Main context for register allocation of a single function. Holds all intermediate and final allocation state.

### 9.3. Entry Points

#### `regalloc_run`

```c
bool regalloc_run(MachineModule* module);
```

Runs register allocation on all functions in a machine module. Returns `true` on success.

#### `regalloc_func`

```c
bool regalloc_func(MachineFunc* func);
```

Runs the full register allocation pipeline on a single function:
1. Number instructions
2. Compute def/use
3. Compute liveness (iterative dataflow)
4. Build live intervals
5. Linear scan allocation
6. Insert spill code
7. Rewrite operands

### 9.4. Liveness Analysis API

| Function | Signature | Description |
|----------|-----------|-------------|
| `regalloc_ctx_new` | `RegAllocCtx* regalloc_ctx_new(MachineFunc*)` | Create allocation context for a function |
| `regalloc_ctx_free` | `void regalloc_ctx_free(RegAllocCtx*)` | Free allocation context and all bitsets |
| `regalloc_number_insts` | `void regalloc_number_insts(RegAllocCtx*)` | Sequential instruction numbering, builds inst_map |
| `regalloc_compute_def_use` | `void regalloc_compute_def_use(RegAllocCtx*)` | Compute per-block def/use bitsets |
| `regalloc_compute_liveness` | `void regalloc_compute_liveness(RegAllocCtx*)` | Iterative dataflow to fixpoint (max 100 iterations) |
| `regalloc_build_intervals` | `void regalloc_build_intervals(RegAllocCtx*)` | Build live intervals from liveness sets |

### 9.5. Allocation API

| Function | Signature | Description |
|----------|-----------|-------------|
| `regalloc_linear_scan` | `void regalloc_linear_scan(RegAllocCtx*)` | Linear scan allocation with spill on pressure |
| `regalloc_insert_spill_code` | `void regalloc_insert_spill_code(RegAllocCtx*)` | Handle spilled vregs (implicit via rewrite) |
| `regalloc_rewrite` | `void regalloc_rewrite(RegAllocCtx*)` | Replace all VREG operands with physical regs |

### 9.6. Utility Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `phys_reg_name` | `const char* phys_reg_name(PhysReg)` | Get register name string ("rax", "rcx", ...) |
| `phys_reg_is_callee_saved` | `bool phys_reg_is_callee_saved(PhysReg)` | Check if register is callee-saved |
| `regalloc_print_intervals` | `void regalloc_print_intervals(RegAllocCtx*, FILE*)` | Print live intervals for debugging |
| `regalloc_print_allocation` | `void regalloc_print_allocation(RegAllocCtx*, FILE*)` | Print vreg-to-physical-reg mapping |

---

## 10. Codegen Module

Handles x86-64 assembly generation.

### `codegen`

```c
void codegen(Node* node, FILE* file)
```

Recursively generates assembly code from AST.

| Parameter | Type | Description |
|-----------|------|-------------|
| `node` | `Node*` | AST node to process (start with root) |
| `file` | `FILE*` | Open file handle for output (`out.s`) |

**Generated Sections:**

- `.data` — Global variables
- `.rdata` — String literals
- `.text` — Function bodies

**Assembly Features:**

- Windows x64 ABI compliant stack frames
- Short-circuit evaluation for `&&` and `||`
- Indexed addressing for array access
- Label-based control flow for loops

---

## 11. Diagnostic System

Centralized diagnostic system for compiler errors and warnings (v0.2.8+).

### 11.1. Error Reporting

#### `error_init`

```c
void error_init(const char* source)
```

Initializes the error reporting system with the source code for context display.

#### `error_report`

```c
void error_report(Token token, const char* message, ...)
```

Reports an error with source location, line context, and a pointer to the error position.

**Features:**

- Displays filename, line, and column
- Shows the actual source line with a `^` pointer
- Supports printf-style formatting
- **Colored output** (red) when terminal supports ANSI codes (v0.2.8+)

### 8.2. Warning System (v0.2.8+)

#### `warning_init`

```c
void warning_init(void)
```

Initializes the warning configuration with default settings. Called automatically at startup.

#### `warning_report`

```c
void warning_report(WarningType type, const char* filename, int line, int col, const char* message, ...)
```

Reports a warning with source location and warning type.

| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | `WarningType` | Category of warning (e.g., `WARN_UNUSED_VARIABLE`) |
| `filename` | `const char*` | Source filename |
| `line` | `int` | Line number |
| `col` | `int` | Column number |
| `message` | `const char*` | Printf-style format string |

**Features:**

- Only emitted if warning type is enabled (via `-Wall` or specific `-W<type>`)
- **Colored output** (yellow) when terminal supports ANSI codes
- Shows warning name in brackets: `[-Wunused-variable]`
- With `-Werror`, warnings are displayed as errors (red)

### 8.3. Warning Types

```c
typedef enum {
    WARN_UNUSED_VARIABLE,    // Variable declared but never used
    WARN_DEAD_CODE,          // Unreachable code after return/break
    WARN_IMPLICIT_RETURN,    // Function without explicit return (future)
    WARN_SHADOW_VARIABLE,    // Local variable shadows global
    WARN_COUNT               // Total count (for iteration)
} WarningType;
```

### 8.4. Warning Configuration

```c
typedef struct {
    bool enabled[WARN_COUNT];   // Enable/disable each warning type
    bool warnings_as_errors;    // -Werror: treat warnings as errors
    bool all_warnings;          // -Wall: enable all warnings
    bool colored_output;        // ANSI colors in output
} WarningConfig;

extern WarningConfig g_warning_config;
```

### 8.5. State Query Functions

| Function | Return | Description |
|----------|--------|-------------|
| `error_has_occurred()` | `bool` | Returns `true` if any error was reported |
| `warning_has_occurred()` | `bool` | Returns `true` if any warning was reported |
| `warning_get_count()` | `int` | Returns total number of warnings |
| `error_reset()` | `void` | Resets error state |
| `warning_reset()` | `void` | Resets warning state and counter |

---

## 10. Symbol Table

Manages variable scope and resolution.

### Symbol Structure

Moved to `baa.h` for shared use between Analysis and Codegen.

```c
typedef struct {
    char name[32];         // Variable name
    ScopeType scope;       // SCOPE_GLOBAL or SCOPE_LOCAL
    DataType type;         // TYPE_INT or TYPE_STRING
    int offset;            // Stack offset or memory address
    bool is_const;         // Immutability flag (v0.2.7+)
    bool is_used;          // Usage tracking for warnings (v0.2.8+)
    int decl_line;         // Declaration line (v0.2.8+)
    int decl_col;          // Declaration column (v0.2.8+)
    const char* decl_file; // Declaration file (v0.2.8+)
} Symbol;
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | `char[32]` | Variable identifier (Arabic UTF-8) |
| `scope` | `ScopeType` | `SCOPE_GLOBAL` or `SCOPE_LOCAL` |
| `type` | `DataType` | `TYPE_INT` or `TYPE_STRING` |
| `offset` | `int` | Stack offset (local) or memory address (global) |
| `is_const` | `bool` | `true` if declared with `ثابت` (v0.2.7+) |
| `is_used` | `bool` | `true` if variable is referenced (v0.2.8+) |
| `decl_line` | `int` | Line number where symbol was declared (v0.2.8+) |
| `decl_col` | `int` | Column number where symbol was declared (v0.2.8+) |
| `decl_file` | `const char*` | Filename where symbol was declared (v0.2.8+) |

> **Note**: Symbol table management functions (`add_local`, `lookup`, etc.) are implemented as static helper functions within `analysis.c` and `codegen.c` independently. In v0.2.4+, the `Symbol` struct definition is shared via `baa.h` to enable semantic analysis before code generation.

---

## 11. Updater

### `run_updater`

```c
void run_updater(void);
```

Runs the built-in updater.

**Notes (v0.2.9):**

- Implemented in [updater.c](file:///D:/My%20Dev%20Life/Software%20Dev/Baa/src/updater.c).
- Windows-only implementation (links against `urlmon` and `wininet` on Windows builds).
- Invoked from the CLI as `baa update` (must be the only argument).

---

## 12. Data Structures

### 11.1. Preprocessor Structures

### Lexer & Preprocessor Structures

```c
// Macro Definition
typedef struct {
    char* name;
    char* value;
} Macro;

// State for a single file
typedef struct {
    char* source;
    char* cur_char;
    const char* filename;
    int line;
    int col;
} LexerState;

// Lexer State
typedef struct {
    LexerState state;       // Current file state
    LexerState stack[10];   // Include stack (max depth: 10)
    int stack_depth;
    
    // Preprocessor
    Macro macros[100];      // Defined macros
    int macro_count;
    bool skipping;          // True if inside disabled #if block
} Lexer;
```

### 11.2. Token

Represents a single atomic unit of source code.

```c
typedef struct {
    BaaTokenType type;  // Discriminator (TOKEN_INT, TOKEN_IF, etc.)
    char* value;        // Payload (name for ID, digits for INT)
    int line;           // Source line number (for errors)
    int col;            // Source column
    const char* filename;
} Token;
```

### 11.3. Token Types

### BaaTokenType Enum

```c
typedef enum {
    TOKEN_EOF,
    TOKEN_INT,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_IDENTIFIER,

    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_STRING,
    TOKEN_KEYWORD_BOOL,
    TOKEN_CONST,
    TOKEN_RETURN,
    TOKEN_PRINT,
    TOKEN_READ,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_TRUE,
    TOKEN_FALSE,

    TOKEN_ASSIGN,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_SEMICOLON,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_INC,
    TOKEN_DEC,

    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_LTE,
    TOKEN_GTE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    TOKEN_INVALID
} BaaTokenType;
```

---

### 11.4. AST Node Structure

### Node (AST)

The AST is a tagged union representing grammatical structure.

```c
typedef struct Node {
    NodeType type;
    struct Node* next;

    union {
        struct { struct Node* declarations; } program;
        struct { struct Node* statements; } block;

        struct {
            char* name;
            DataType return_type;
            struct Node* params;
            struct Node* body;   // NULL if prototype
            bool is_prototype;
        } func_def;

        struct {
            char* name;
            DataType type;
            struct Node* expression; // Required for locals, optional for globals
            bool is_global;
            bool is_const;
        } var_decl;

        struct {
            char* name;
            int size;
            bool is_global;
            bool is_const;
        } array_decl;

        struct {
            char* name;
            struct Node* index;
            struct Node* value; // NULL for read
        } array_op;

        struct {
            char* name;
            struct Node* args;
        } call;

        struct {
            struct Node* condition;
            struct Node* then_branch;
            struct Node* else_branch;
        } if_stmt;

        struct { struct Node* condition; struct Node* body; } while_stmt;
        struct { struct Node* expression; } return_stmt;
        struct { struct Node* expression; } print_stmt;
        struct { char* var_name; } read_stmt;
        struct { char* name; struct Node* expression; } assign_stmt;

        struct {
            struct Node* init;
            struct Node* condition;
            struct Node* increment;
            struct Node* body;
        } for_stmt;

        struct { struct Node* expression; struct Node* cases; } switch_stmt;

        struct {
            struct Node* value;
            struct Node* body;
            bool is_default;
        } case_stmt;

        struct { int value; } integer;
        struct { char* value; int id; } string_lit;
        struct { int value; } char_lit;
        struct { bool value; } bool_lit;

        struct { char* name; } var_ref;
        struct { struct Node* left; struct Node* right; OpType op; } bin_op;

        struct { struct Node* operand; UnaryOpType op; } unary_op;
    } data;
} Node;
```

---

### 11.5. Node Types

### NodeType Enum

```c
typedef enum {
    // Top-level
    NODE_PROGRAM,
    NODE_FUNC_DEF,
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_SWITCH,
    NODE_CASE,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_ASSIGN,
    NODE_CALL_STMT,
    NODE_READ,
    NODE_ARRAY_DECL,
    NODE_ARRAY_ASSIGN,
    NODE_ARRAY_ACCESS,
    NODE_PRINT,
    NODE_BIN_OP,
    NODE_UNARY_OP,
    NODE_POSTFIX_OP,
    NODE_INT,
    NODE_STRING,
    NODE_CHAR,
    NODE_BOOL,
    NODE_VAR_REF,
    NODE_CALL_EXPR
} NodeType;
```

---

*[← User Guide](USER_GUIDE.md) | [Internals →](INTERNALS.md)*
