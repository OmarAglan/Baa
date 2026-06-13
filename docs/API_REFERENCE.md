# Baa Internal API Reference

> **Version:** 0.9.1.5 | [← Compiler Internals](INTERNALS.md) | [IR Specification →](BAA_IR_SPECIFICATION.md)

This document details the C functions, enumerations, and structures defined in the current component-owned public headers under `src/frontend/`, `src/support/`, `src/middleend/`, and `src/backend/`. `src/baa.h` remains as a compatibility umbrella over the frontend/support public surface.

---

## Table of Contents

- [QA Tooling](#0-qa-tooling-v038)
- [Lexer Module](#1-lexer-module)
- [Parser Module](#2-parser-module)
- [Semantic Analysis](#3-semantic-analysis)
- [IR Module](#4-ir-module)
  - [IR Data Layout](#411-ir-data-layout-module-v03266)
  - [Target Abstraction](#412-target-abstraction-v03281)
  - [Code Model](#413-code-model-v03283)
- [IR Builder Module](#5-ir-builder-module)
- [IR Lowering Module](#6-ir-lowering-module)
- [IR Optimization Passes](#7-ir-optimization-passes)
- [Instruction Selection Module](#8-instruction-selection-module-v0321)
- [Register Allocation Module](#9-register-allocation-module-v0322)
- [Code Emission Module](#10-code-emission-module-v0323)
- [Diagnostic System](#11-diagnostic-system)
- [Symbol Table](#12-symbol-table)
- [Updater](#13-updater)
- [Data Structures](#14-data-structures)

---

## 0. QA Tooling (v0.3.8)

### `scripts/qa_run.py`

Unified QA entrypoint used by CI and local validation.

```bash
python scripts/qa_run.py --mode quick
python scripts/qa_run.py --mode full
python scripts/qa_run.py --mode stress
python scripts/qa_run.py --mode release
```

| Flag | Description |
|------|-------------|
| `--mode quick` | Integration smoke (`tests/integration/**/*.baa` via `tests/test.py`) |
| `--mode full` | Quick + regression + verify smoke + multi-file smoke + build maturity checks |
| `--mode stress` | Full + stress suite (`tests/stress/*.baa`) + seeded fuzz-lite |
| `--mode release` | Stress + deterministic IR/assembly/manifest checks + bootstrap subset gate |
| `--summary-json <path>` | Writes machine-readable QA summary JSON |
| `--fuzz-cases <N>` | Number of fuzz-lite cases in stress/release mode |
| `--seed <N>` | RNG seed for deterministic fuzz-lite generation |

Additional build-maturity guards:

- `scripts/check_build_profiles.py` validates required CMake configure/build presets.
- `scripts/test_build_maturity.py` validates manifest stability, incremental cache hits, and header invalidation.
- `scripts/test_determinism.py` validates stable `--dump-ir`, `--dump-ir-opt`, assembly, manifest, `--verify-gate`, cross-target assembly generation, and committed IR snapshot hashes under `tests/snapshots/`.
- `scripts/qa_bootstrap_gate.py` validates Baa0 positive sources, banned-feature fixtures, runtime-marked bootstrap cases, and cross-target `-S` assembly generation for the bootstrap corpus.
- `scripts/qa_parser_harness.py` builds the opt-in C/Baa parser compiler and runs the curated mixed parser corpus.
- `scripts/qa_parser_parity.py` compares parser AST dumps and parser diagnostic output between the production Baa parser wrapper and an explicit C parser baseline.
- `scripts/qa_phase45_handoff.py` creates the Phase 4.5 handoff evidence bundle and records local platform signoff status.
- `scripts/qa_stage0_manifest.py` validates the v0.9.0.1 Stage-0 snapshot tag and frozen artifact Git object IDs.

### Test Metadata Contract

Supported in test files:

- `// RUN:` execution behavior
  - `expect-pass`
  - `expect-fail`
  - `runtime`
  - `compile-only`
  - `skip`
- `// EXPECT:` negative diagnostic anchor(s)
- `// EXPECT-NOT:` forbidden negative diagnostic anchor(s)
- `// EXPECT-DIAG-COUNT:` exact negative diagnostic count
- `// FLAGS:` per-test compiler flags
- `// ARGS:` runtime execution arguments (one line; passed to the produced executable)
- `// STDIN:` stdin lines for runtime tests (may appear multiple times; joined with `\n` and a trailing newline is appended)
- `// EXPECT-ASM:` assembly substring expectations (used with `-S` compile-only tests)
- `// EXPECT-NOT-ASM:` forbidden assembly substring expectations (used with `-S` compile-only tests)
- `// EXPECT-EXIT:` expected runtime exit status
- `// EXPECT-OUT:` runtime stdout substring expectations
- `// EXPECT-ERR:` runtime stderr substring expectations

`tests/test.py` and `tests/regress.py` both recognize these markers.

---

## 1. Lexer Module

Handles UTF-8 string processing, tokenization, and **preprocessing**.

### `lexer_init`

```c
void lexer_init(Lexer* l,
                char* src,
                const char* filename,
                const char* const* include_dirs,
                size_t include_dir_count)
```

Initializes a new Lexer instance.

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Pointer to Lexer struct to initialize |
| `src` | `char*` | Source code buffer (mutable for internal pointer usage) |
| `filename` | `const char*` | Name of the file (for error reporting) |
| `include_dirs` | `const char* const*` | Optional `-I` include search paths |
| `include_dir_count` | `size_t` | Number of include search paths |

**Behavior:**

- Initializes the state stack depth to 0.
- Sets up position pointers (`cur_char`) to start of source.
- Detects and skips UTF-8 BOM (Byte Order Mark: `0xEF 0xBB 0xBF`) if present.
- Initializes line counter to 1.
- Initializes dependency tracking and the Baa-backed scanner bridge.

---

### `lexer_next_token`

```c
Token lexer_next_token(Lexer* l)
```

Consumes input and returns the next valid token. **Delegates tokenization and preprocessing to the Baa scanner.**

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Initialized Lexer |

**Returns:** `Token` struct. Returns `TOKEN_EOF` at end of file.

**Behavior:**

- Skips whitespace and comments (`//`).
- **Preprocessor:**
  - Handles `#تعريف <name> <value>` (define) in Baa-owned scanner state.
  - Handles `#إذا_عرف <name>` (ifdef) to conditionally compile code.
  - Handles `#إذا_لم_يعرف <name>` (ifndef) to conditionally compile code when a macro is absent.
  - Handles `#وإلا` (else) and `#نهاية` (endif) for conditional blocks.
  - Handles `#تضمين "file"` (include) through the C include service.
  - Handles `#الغاء_تعريف <name>` (undefine) in Baa-owned scanner state.
  - Replaces identifiers with macro values if defined.
- Identifies keywords, literals, and operators.
- Handles Arabic semicolon `؛`.
- Normalizes Arabic-Indic digits (`٠`-`٩`).
- Validates UTF-8 sequences in identifiers and literals (v0.3.7) and reports lexical errors through centralized diagnostics.

---

## 2. Parser Module

Handles syntactic analysis and AST construction.

### Baa parser contract (v0.9.2)

`src/frontend/parser.baahd` defines the first Baa-side parser entry contract for the parser rewrite:

```baa
#تضمين "ast.baahd"

عقدة_باء محلل_قواعد_باء_حلل(ط٨* محلل_لفظي).
```

The Baa contract deliberately uses raw handles for the lexer and AST root while the production C API remains `parse(Lexer*) -> Node*`. The default build routes that C-facing API through the Baa parser wrapper. In the current migration slice, Baa owns the top-level loop, declaration dispatch, and the `نوع <alias> = <type>.` declaration shell, then delegates reusable type-spec parsing and non-alias leaf declaration grammar to C helpers; `BAA_USE_BAA_PARSER_TOPLEVEL=OFF` remains available temporarily as the C parser baseline during migration.

### Baa AST contract (v0.9.2)

`src/frontend/ast.baahd` defines the Baa-side AST migration contract:

```baa
نوع عقدة_باء = ط٨*.
نوع توقيع_دالة_باء = ط٨*.

تعداد نوع_عقدة_باء { ... }
تعداد نوع_بيانات_باء { ... }
تعداد عملية_ثنائية_باء { ... }
تعداد عملية_أحادية_باء { ... }

عقدة_باء شجرة_باء_عقدة_أنشئ(صحيح وسم_نوع، ط٨* ملف، صحيح سطر، صحيح عمود، صحيح طول).
عدم شجرة_باء_عقدة_التالي_ضع(عقدة_باء عقدة، عقدة_باء التالي).
عقدة_باء شجرة_باء_عقدة_التالي(عقدة_باء عقدة).
عدم شجرة_باء_برنامج_تصريحات_ضع(عقدة_باء برنامج، عقدة_باء تصريحات).
عدم شجرة_باء_حرر(عقدة_باء جذر).
```

The header defines Baa enums aligned with `NodeType`, `DataType`, `OpType`, and `UnaryOpType`. These enums are a migration contract and must stay numerically aligned with `src/frontend/ast.h` until the full frontend representation is Baa-owned.

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
- Parses the top-level declaration list through the Baa wrapper and dispatches each declaration to the appropriate parser leaf
- Parses the global type-alias declaration shell in Baa: `نوع <name> = <type>.` while C still parses the `<type>` spec (v0.3.6.5)
- Parses declaration qualifiers `ثابت` / `ساكن` in any order for variable/array declarations (v0.3.7.5)
- Parses low-level expressions/operators: `&`, `|`, `^`, `~`, `<<`, `>>`, and `حجم(type|expr)` (v0.3.6)
- Implements operator precedence climbing for expressions
- Uses context-aware panic recovery modes (statement/declaration/switch) to continue after syntax errors (v0.3.7)
- Builds linked-list AST structure

**Example:**

```c
Lexer lexer;
lexer_init(&lexer, source_code, "filename.baa", NULL, 0);
Node* ast = parse(&lexer);
// Use ast...
```

### `ast_dump_print`

```c
void ast_dump_print(const Node* root, FILE* out)
```

Prints a deterministic parser-owned AST shape for diagnostics and parser migration parity tests. The dump is intentionally structural: it excludes addresses and semantic-analysis-only inferred fields.

| Parameter | Type | Description |
|-----------|------|-------------|
| `root` | `const Node*` | AST root, usually `NODE_PROGRAM` |
| `out` | `FILE*` | Output file handle; `NULL` defaults to `stdout` |

**Driver flag:** `--dump-ast` parses each input, prints this AST shape to stdout, and exits before semantic analysis, IR lowering, assembling, or linking.

---

## 3. Semantic Analysis

Handles type checking, symbol resolution, **constant validation**, and **static-storage validation**.

### Baa semantic-analysis contract (v0.9.3)

`src/frontend/analysis.baahd` defines the first Baa-side semantic entry contract:

```baa
#تضمين "ast.baahd"

صحيح تحليل_دلالي_باء_حلل(عقدة_باء برنامج).
```

The Baa entry returns `1` for a valid AST and `0` for semantic errors. The production C API remains `analyze(Node*) -> bool` until the semantic-analysis port is wired through a compatibility wrapper.

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
- **Static Storage Rules** (v0.3.7.5): Enforces compile-time initializer constraints for static-storage declarations.
- **Control Flow Validation**: Ensures `break`/`continue` are only used within loops/switches.
- **Type Alias Validation** (v0.3.6.5): Registers/validates `نوع` aliases and enforces strict collision checks with symbols/functions.
- **Low-Level Validation** (v0.3.6): integer-only bitwise checks, `حجم(...)` compile-time sizing checks, and `عدم` return/declaration constraints.
- **Lookup Acceleration** (v0.3.7): Uses hash-indexed symbol lookup paths for local/global symbol resolution.
- Traverses the entire tree.
- Reports errors using `error_report`.
- Does not modify the AST, only validates it.

**Constant Validation Rules:**

- Automatic-storage constants must be initialized at declaration time.
- Reassigning a constant produces an Arabic semantic diagnostic for reassigning `ثابت`.
- Modifying constant array elements produces an Arabic semantic diagnostic for modifying a constant array.
- Static-storage declarations (global or `ساكن`) require compile-time-constant initializers when provided.

---

## 4. IR Module (v0.3.0+)

The IR Module (`src/ir.h`, `src/ir.c`) provides Baa's Arabic-first Intermediate Representation.

### 4.0. IR Data Structures and Enums (v0.3.0)

#### `IROp` Enum
All IR instruction opcodes.

```c
typedef enum {
    // --------------------------------------------------------------------
    // Arithmetic Operations (العمليات الحسابية)
    // --------------------------------------------------------------------
    IR_OP_ADD,      // جمع - Addition
    IR_OP_SUB,      // طرح - Subtraction
    IR_OP_MUL,      // ضرب - Multiplication
    IR_OP_DIV,      // قسم - Signed division
    IR_OP_MOD,      // باقي - Modulo (remainder)
    IR_OP_NEG,      // سالب - Negation (unary minus)
    
    // --------------------------------------------------------------------
    // Memory Operations (عمليات الذاكرة)
    // --------------------------------------------------------------------
    IR_OP_ALLOCA,   // حجز - Stack allocation
    IR_OP_LOAD,     // حمل - Load from memory
    IR_OP_STORE,    // خزن - Store to memory
    IR_OP_PTR_OFFSET, // إزاحة_مؤشر - Pointer offset: base + index * sizeof(pointee)
    
    // --------------------------------------------------------------------
    // Comparison Operations (عمليات المقارنة)
    // --------------------------------------------------------------------
    IR_OP_CMP,      // قارن - Compare (with predicate)
    
    // --------------------------------------------------------------------
    // Logical Operations (العمليات المنطقية)
    // --------------------------------------------------------------------
    IR_OP_AND,      // و - Bitwise AND
    IR_OP_OR,       // أو - Bitwise OR
    IR_OP_XOR,      // أو_حصري - Bitwise XOR
    IR_OP_NOT,      // نفي - Bitwise NOT
    IR_OP_SHL,      // ازاحة_يسار - Shift left
    IR_OP_SHR,      // ازاحة_يمين - Shift right (arith/logical حسب النوع)
    
    // --------------------------------------------------------------------
    // Control Flow Operations (عمليات التحكم)
    // --------------------------------------------------------------------
    IR_OP_BR,       // قفز - Unconditional branch
    IR_OP_BR_COND,  // قفز_شرط - Conditional branch
    IR_OP_RET,      // رجوع - Return from function
    IR_OP_CALL,     // نداء - Function call
    
    // --------------------------------------------------------------------
    // SSA Operations (عمليات SSA)
    // --------------------------------------------------------------------
    IR_OP_PHI,      // فاي - Phi node for SSA
    IR_OP_COPY,     // نسخ - Copy (for SSA construction)
    
    // --------------------------------------------------------------------
    // Type Conversion (تحويل الأنواع)
    // --------------------------------------------------------------------
    IR_OP_CAST,     // تحويل - Type cast/conversion
    
    // --------------------------------------------------------------------
    // Special Operations
    // --------------------------------------------------------------------
    IR_OP_NOP,      // No operation (placeholder)
    
    IR_OP_COUNT     // Total number of opcodes
} IROp;
```

#### `IRTypeKind` Enum
Kinds of IR types.

```c
typedef enum {
    IR_TYPE_VOID,   // فراغ - Void (no value)
    IR_TYPE_I1,     // ص١ - 1-bit integer (boolean)
    IR_TYPE_I8,     // ص٨ - 8-bit integer (byte/char)
    IR_TYPE_I16,    // ص١٦ - 16-bit integer
    IR_TYPE_I32,    // ص٣٢ - 32-bit integer
    IR_TYPE_I64,    // ص٦٤ - 64-bit integer (primary)
    IR_TYPE_U8,     // ط٨ - 8-bit unsigned integer
    IR_TYPE_U16,    // ط١٦ - 16-bit unsigned integer
    IR_TYPE_U32,    // ط٣٢ - 32-bit unsigned integer
    IR_TYPE_U64,    // ط٦٤ - 64-bit unsigned integer
    IR_TYPE_CHAR,   // حرف - UTF-8 character (packed)
    IR_TYPE_F64,    // ع٦٤ - 64-bit floating point (storage only for now)
    IR_TYPE_PTR,    // مؤشر - Pointer type
    IR_TYPE_ARRAY,  // مصفوفة - Array type
    IR_TYPE_FUNC,   // دالة - Function type
} IRTypeKind;
```

#### `IRValueKind` Enum
Discriminator for `IRValue`.

```c
typedef enum {
    IR_VAL_NONE,        // No value (void)
    IR_VAL_CONST_INT,   // Constant integer
    IR_VAL_CONST_STR,   // Constant string (pointer to .rdata)
    IR_VAL_BAA_STR,     // Baa string (pointer to .rodata UTF-8 chars array)
    IR_VAL_REG,         // Virtual register (%م<n>)
    IR_VAL_GLOBAL,      // Global variable (@name)
    IR_VAL_FUNC,        // Function reference (@name)
    IR_VAL_BLOCK,       // Basic block reference
} IRValueKind;
```

#### `IRInst` Struct
Represents a single IR instruction.

```c
typedef struct IRInst {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IROp op;                    // Opcode
    IRType* type;               // Result type (or void for stores/branches)

    // معرّف ثابت للتعليمة داخل الدالة (للتشخيص/الاختبارات)
    int id;

    // Destination register (for instructions that produce a value)
    int dest;                   // -1 if no destination

    // Operands (meaning depends on opcode)
    IRValue* operands[4];       // Up to 4 operands
    int operand_count;

    // For comparison instructions
    IRCmpPred cmp_pred;

    // For Phi nodes
    IRPhiEntry* phi_entries;    // Linked list of [value, block] pairs

    // For calls
    char* call_target;          // Function name
    IRValue* call_callee;       // قيمة الهدف عند النداء غير المباشر (NULL في النداء المباشر)
    IRValue** call_args;        // Argument list
    int call_arg_count;

    // Source location (for debugging)
    const char* src_file;
    int src_line;
    int src_col;

    // اسم رمز/متغير اختياري للتتبع (للديبغ)
    const char* dbg_name;

    // الأب (الكتلة التي تحتوي التعليمة)
    struct IRBlock* parent;

    // Linked list within block
    struct IRInst* prev;
    struct IRInst* next;
} IRInst;
```

**Note:** Includes Arabic comments from source for field documentation. Fields include `src_file`, `src_line`, `src_col`, and `dbg_name` for enhanced debugging support.

**Memory management (v0.3.2.6.1):** IR objects are allocated from a module-owned arena (`src/ir_arena.c`). Treat the IR as **module-owned** and release everything with `ir_module_free()`. The legacy `*_free` functions remain for compatibility but do not perform per-object frees under the arena model.

**Arena stats (v0.3.2.9.2):** you can query arena usage for profiling:

```c
typedef struct IRArenaStats {
    size_t chunks;
    size_t used_bytes;
    size_t cap_bytes;
} IRArenaStats;

void ir_arena_get_stats(const IRArena* arena, IRArenaStats* out_stats);
```

#### `IRParam` Struct
Represents a function parameter.

```c
typedef struct IRParam {
    char* name;                 // Parameter name
    IRType* type;               // Parameter type
    int reg;                    // Virtual register assigned (%معامل<n>)
} IRParam;
```

#### `IRStringEntry` Struct
Represents a string literal in the string table.

```c
typedef struct IRStringEntry {
    char* content;              // String content (UTF-8)
    int id;                     // Unique ID (.Lstr_<id>)
    struct IRStringEntry* next;
} IRStringEntry;
```

#### `IRBaaStringEntry` Struct
Baa string constant represented as a `حرف[]` array.

```c
typedef struct IRBaaStringEntry {
    char* content;              // String content (UTF-8)
    int id;                     // Unique ID (.Lbs_<id>)
    struct IRBaaStringEntry* next;
} IRBaaStringEntry;
```

**Note:** These structures are allocated within the IR arena and are freed in bulk when the module is destroyed.

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

#### `ir_type_func`

```c
IRType* ir_type_func(IRType* ret, IRType** params, int param_count)
```

Creates a function type.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ret` | `IRType*` | Return type |
| `params` | `IRType**` | Array of parameter types |
| `param_count` | `int` | Number of parameters |

**Returns:** New `IRType*` with `kind = IR_TYPE_FUNC`.

---

#### `ir_types_equal`

```c
int ir_types_equal(IRType* a, IRType* b)
```

Returns 1 if two types are equal, 0 otherwise.

---

#### `ir_type_bits`

```c
int ir_type_bits(IRType* type)
```

Returns the size of a type in bits.

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

#### `ir_value_baa_str`

```c
IRValue* ir_value_baa_str(const char* str, int id)
```

Creates a Baa string constant (`حرف[]`).

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `const char*` | String content |
| `id` | `int` | Baa string table ID |

---

#### `ir_value_block`

```c
IRValue* ir_value_block(IRBlock* block)
```

Creates a basic block reference value.

---

#### `ir_value_global`

```c
IRValue* ir_value_global(const char* name, IRType* type)
```

Creates a global variable reference value.

---

#### `ir_value_func_ref`

```c
IRValue* ir_value_func_ref(const char* name, IRType* type)
```

Creates a function reference value.

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

#### `ir_inst_unary`

```c
IRInst* ir_inst_unary(IROp op, IRType* type, int dest, IRValue* operand)
```

Creates a unary operation (neg, not, etc.).

| Parameter | Type | Description |
|-----------|------|-------------|
| `op` | `IROp` | Operation (e.g., `IR_OP_NEG`) |
| `type` | `IRType*` | Result type |
| `dest` | `int` | Destination register number |
| `operand` | `IRValue*` | Operand |

---

#### `ir_inst_new`

```c
IRInst* ir_inst_new(IROp op, IRType* type, int dest)
```

Creates a new instruction with the given opcode, type, and destination.

---

#### `ir_inst_add_operand`

```c
void ir_inst_add_operand(IRInst* inst, IRValue* operand)
```

Adds an operand to an instruction.

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

#### `ir_inst_alloca`

```c
IRInst* ir_inst_alloca(IRType* type, int dest)
```

Creates a stack allocation instruction.

Emits: `%dest = حجز type`

| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | `IRType*` | Type to allocate (result is a pointer to this type) |
| `dest` | `int` | Destination register (pointer) |

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

#### `ir_inst_cast`

```c
IRInst* ir_inst_cast(IRValue* value, IRType* to_type, int dest)
```

Creates a type cast instruction.

Emits: `%dest = تحويل from_type to to_type value`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `IRValue*` | Value to cast |
| `to_type` | `IRType*` | Target type |
| `dest` | `int` | Destination register |

---

#### `ir_inst_ptr_offset`

```c
IRInst* ir_inst_ptr_offset(IRType* ptr_type, int dest, IRValue* base, IRValue* index)
```

Creates a pointer offset instruction (used for array indexing).

Emits: `%dest = إزاحة_مؤشر ptr_type base, index`

**Semantics:** `index` is an element index scaled by `sizeof(pointee)` (target data layout).

| Parameter | Type | Description |
|-----------|------|-------------|
| `ptr_type` | `IRType*` | Result pointer type (must match `base` type) |
| `dest` | `int` | Destination register |
| `base` | `IRValue*` | Base pointer |
| `index` | `IRValue*` | Element index (integer) |

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

#### `ir_inst_call_indirect`

```c
IRInst* ir_inst_call_indirect(IRValue* callee, IRType* ret_type, int dest, IRValue** args, int arg_count)
```

Creates an **indirect** function call through a callee value of type `IR_TYPE_FUNC`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `callee` | `IRValue*` | Callee value (must be `IR_TYPE_FUNC`) |
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

#### `ir_inst_phi_add`

```c
void ir_inst_phi_add(IRInst* phi, IRValue* value, IRBlock* block)
```

Adds an incoming value to a phi node.

---

#### `ir_inst_set_loc`

```c
void ir_inst_set_loc(IRInst* inst, const char* file, int line, int col)
```

Sets source location for debugging.

---

#### `ir_inst_set_dbg_name`

```c
void ir_inst_set_dbg_name(IRInst* inst, const char* name)
```

Sets an optional symbol name for debugging.

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

#### `ir_block_add_pred`

```c
void ir_block_add_pred(IRBlock* block, IRBlock* pred)
```

Adds a predecessor block to the block's predecessor list.

---

#### `ir_block_add_succ`

```c
void ir_block_add_succ(IRBlock* block, IRBlock* succ)
```

Adds a successor block to the block's successor list.

---

#### `ir_block_is_terminated`

```c
int ir_block_is_terminated(IRBlock* block)
```

Returns 1 if the block has a terminator instruction, 0 otherwise.

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

#### `ir_func_alloc_block_id`

```c
int ir_func_alloc_block_id(IRFunc* func)
```

Allocates a new block ID for the function.

---

#### `ir_func_add_block`

```c
void ir_func_add_block(IRFunc* func, IRBlock* block)
```

Adds an existing block to the function.

---

#### `ir_func_add_param`

```c
void ir_func_add_param(IRFunc* func, const char* name, IRType* type)
```

Adds a parameter to the function.

---

### 4.4.1. Global Variable Construction

#### `ir_global_new`

```c
IRGlobal* ir_global_new(const char* name, IRType* type, int is_const)
```

Creates a new global variable.

---

#### `ir_global_set_init`

```c
void ir_global_set_init(IRGlobal* global, IRValue* init)
```

Sets the initializer for a global variable.

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

#### `ir_module_add_global`

```c
void ir_module_add_global(IRModule* module, IRGlobal* global)
```

Adds a global variable to the module.

---

#### `ir_module_add_string`

```c
int ir_module_add_string(IRModule* module, const char* str)
```

Adds a string to the string table.

**Returns:** String ID for referencing in IR.

---

#### `ir_module_add_baa_string`

```c
int ir_module_add_baa_string(IRModule* module, const char* str)
```

Adds a Baa string (`حرف[]`) to the Baa string table.

**Returns:** String ID for referencing in IR.

---

#### `ir_module_find_func`

```c
IRFunc* ir_module_find_func(IRModule* module, const char* name)
```

Finds a function by name in the module.

---

#### `ir_module_find_global`

```c
IRGlobal* ir_module_find_global(IRModule* module, const char* name)
```

Finds a global variable by name in the module.

---

#### `ir_module_get_string`

```c
const char* ir_module_get_string(IRModule* module, int id)
```

Retrieves a string from the string table by ID.

---

#### `ir_module_get_baa_string`

```c
const char* ir_module_get_baa_string(IRModule* module, int id)
```

Retrieves a Baa string from the Baa string table by ID.

---

#### `ir_module_free`

```c
void ir_module_free(IRModule* module)
```

Frees an IR module and all IR objects owned by it.

**Note (v0.3.2.6.1):** This is a bulk free backed by the IR arena.

---

#### `ir_module_set_current` / `ir_module_get_current`

```c
void ir_module_set_current(IRModule* module);
IRModule* ir_module_get_current(void);
```

Sets/gets the active IR module context used by IR allocation helpers. IR passes call this to ensure newly created IR nodes are allocated into the correct module arena.

---

### 4.5.1. IR Mutation Helpers (v0.3.2.6.1)

These helpers provide consistent instruction list manipulation for IR passes.

#### `ir_block_insert_before`

```c
void ir_block_insert_before(IRBlock* block, IRInst* before, IRInst* inst);
```

Inserts `inst` before `before` in `block` (or appends if `before == NULL`).

---

#### `ir_block_remove_inst`

```c
void ir_block_remove_inst(IRBlock* block, IRInst* inst);
```

Unlinks `inst` from `block` and updates block bookkeeping.

---

#### `ir_block_insert_phi`

```c
void ir_block_insert_phi(IRBlock* block, IRInst* phi);
```

Inserts a `فاي` (phi) instruction at the start of the block after any existing phi nodes.

---

#### `ir_block_free_analysis_caches`

```c
void ir_block_free_analysis_caches(IRBlock* block);
```

Frees heap-allocated analysis caches stored in an `IRBlock` (preds/dom_frontier) without freeing the IR block itself.

---

### 4.5.2. Def-Use Chains (v0.3.2.6.1)

Def-Use is an analysis cache that maps SSA registers to their definitions and use sites.

#### `ir_defuse_build`

```c
IRDefUse* ir_defuse_build(IRFunc* func);
```

Builds Def-Use chains for a function.

---

#### `ir_defuse_free`

```c
void ir_defuse_free(IRDefUse* du);
```

Frees a Def-Use cache.

---

#### `ir_func_get_defuse`

```c
IRDefUse* ir_func_get_defuse(IRFunc* func, bool rebuild);
```

Returns a cached Def-Use for `func` (rebuilds if requested or stale).

---

#### `ir_func_invalidate_defuse`

```c
void ir_func_invalidate_defuse(IRFunc* func);
```

Invalidates Def-Use by bumping the function epoch; callers rebuild via `ir_func_get_defuse()`.

---

### 4.5.3. IR Cloning (v0.3.2.6.1)

#### `ir_func_clone`

```c
IRFunc* ir_func_clone(IRModule* module, IRFunc* src, const char* new_name);
```

Deep-clones an IR function (blocks, instructions, phi entries, call args) into `module`.

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

### 4.7. IR Text Serialization (v0.3.2.6.3)

The IR text serialization module (`src/ir_text.h`, `src/ir_text.c`) provides a **machine-readable** IR format designed for round-trip testing:

- Write IR module to a stable text form.
- Read the text form back into an `IRModule`.

#### `ir_text_write_module`

```c
int ir_text_write_module(IRModule* module, FILE* out)
```

Writes a canonical IR text form to `out`.

---

#### `ir_text_dump_module`

```c
int ir_text_dump_module(IRModule* module, const char* filename)
```

Convenience wrapper that opens `filename` and writes the canonical IR text.

---

#### `ir_text_read_module_file`

```c
IRModule* ir_text_read_module_file(const char* filename)
```

Reads an IR module from the canonical IR text format.

---

### 4.8. IR Loop Detection (v0.3.2.7.1)

The loop analysis module (`src/ir_loop.h`, `src/ir_loop.c`) discovers **natural loops** in the IR control-flow graph using **back edges** and dominance.

#### `ir_loop_analyze_func`

```c
IRLoopInfo* ir_loop_analyze_func(IRFunc* func)
```

Returns an `IRLoopInfo*` for `func` (caller frees via `ir_loop_info_free`).

---

#### `ir_loop_info_free`

```c
void ir_loop_info_free(IRLoopInfo* info)
```

Frees loop analysis results.

---

#### `ir_loop_info_count`

```c
int ir_loop_info_count(IRLoopInfo* info)
```

Returns number of detected loops.

---

#### `ir_loop_info_get`

```c
IRLoop* ir_loop_info_get(IRLoopInfo* info, int index)
```

Returns loop at `index`.

---

#### `ir_loop_contains`

```c
bool ir_loop_contains(IRLoop* loop, IRBlock* block)
```

Checks whether `block` is in `loop`.

---

#### `ir_loop_header`

```c
IRBlock* ir_loop_header(IRLoop* loop)
```

Returns the loop header block.

---

#### `ir_loop_preheader`

```c
IRBlock* ir_loop_preheader(IRLoop* loop)
```

Returns the preheader block if one exists (may be NULL).

---

#### `ir_loop_block_count`

```c
int ir_loop_block_count(IRLoop* loop)
```

Returns the number of blocks in the loop.

---

#### `ir_loop_block_at`

```c
IRBlock* ir_loop_block_at(IRLoop* loop, int index)
```

Returns a block from within the loop at the given index.

---

### 4.9. IR Loop Unrolling (v0.3.2.7.1)

The loop unrolling utility (`src/ir_unroll.h`, `src/ir_unroll.c`) provides a **conservative** full-unroll for small constant trip-count loops. It is currently used by the driver via `-funroll-loops` **after** Out-of-SSA.

#### `ir_unroll_run`

```c
bool ir_unroll_run(IRModule* module, int max_trip)
```

Attempts to fully unroll eligible loops up to `max_trip` iterations.

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

### 4.7.1. English Name Conversion (for debugging)

#### `ir_op_to_english`

```c
const char* ir_op_to_english(IROp op)
```

Converts an opcode to its English name (for debugging).

**Example:** `ir_op_to_english(IR_OP_ADD)` returns `"ADD"`.

---

#### `ir_cmp_pred_to_english`

```c
const char* ir_cmp_pred_to_english(IRCmpPred pred)
```

Converts a comparison predicate to its English name.

**Example:** `ir_cmp_pred_to_english(IR_CMP_EQ)` returns `"EQ"`.

---

#### `ir_type_to_english`

```c
const char* ir_type_to_english(IRType* type)
```

Converts a type to its English representation.

**Example:** `ir_type_to_english(IR_TYPE_I64_T)` returns `"i64"`.

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
extern IRType* IR_TYPE_U8_T;     // ط٨
extern IRType* IR_TYPE_U16_T;    // ط١٦
extern IRType* IR_TYPE_U32_T;    // ط٣٢
extern IRType* IR_TYPE_U64_T;    // ط٦٤
extern IRType* IR_TYPE_CHAR_T;   // حرف
extern IRType* IR_TYPE_F64_T;    // ع٦٤
```

**Note:** Added unsigned type constants (`U8_T` through `U64_T`) for complete type coverage.

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

### 4.11. IR Data Layout Module (v0.3.2.6.6)

The Data Layout module (`src/ir_data_layout.h`, `src/ir_data_layout.c`) provides target-specific type information.

#### `IRDataLayout` struct

Data layout descriptor for target-specific type sizes and alignments.

```c
typedef struct IRDataLayout {
    int pointer_size_bytes;     // حجم المؤشر (e.g., 8 on x86-64)
    int pointer_align_bytes;    // محاذاة المؤشر
    int i1_store_size_bytes;    // حجم تخزين ص١ (always 1)
    int i8_align_bytes;         // محاذاة ص٨
    int i16_align_bytes;        // محاذاة ص١٦
    int i32_align_bytes;        // محاذاة ص٣٢
    int i64_align_bytes;        // محاذاة ص٦٤
} IRDataLayout;

extern const IRDataLayout IR_DATA_LAYOUT_WIN_X64;
```

**Note:** The data layout uses `int` types and focuses on pointer properties and alignment values needed for code generation.

---

#### `ir_type_size_bytes`

```c
int ir_type_size_bytes(const IRDataLayout* dl, const IRType* type);
```

Returns the size of a type in bytes (including padding for aggregates).
- `dl`: Layout descriptor (pass `NULL` for default Win-x64).
- `type`: The type to query.

---

#### `ir_type_alignment`

```c
int ir_type_alignment(const IRDataLayout* dl, const IRType* type);
```

Returns the required alignment for a type in bytes.

---

#### `ir_type_store_size`

```c
int ir_type_store_size(const IRDataLayout* dl, const IRType* type);
```

Returns the number of bytes written to memory (same as size for now).

---

#### `ir_type_is_integer`

```c
int ir_type_is_integer(const IRType* type);
```

Returns 1 if the type is `i1`, `i8`, `i16`, `i32`, or `i64`. Returns 0 for pointers/void/arrays.

---

#### `ir_type_is_pointer`

```c
int ir_type_is_pointer(const IRType* type);
```

Returns 1 if the type is `ptr`.

---

### 4.12. Target Abstraction (v0.3.2.8.1)

The Target module (`src/target.h`, `src/target.c`) provides target-specific abstractions for OS, object format, and calling convention.

#### Target Kinds and Object Formats

```c
typedef enum {
    BAA_TARGET_X86_64_WINDOWS = 0,
    BAA_TARGET_X86_64_LINUX   = 1,
} BaaTargetKind;

typedef enum {
    BAA_OBJFORMAT_COFF = 0,
    BAA_OBJFORMAT_ELF  = 1,
} BaaObjectFormat;
```

#### `BaaCallingConv` Struct

Describes the calling convention for a target.

```c
typedef struct BaaCallingConv {
    int int_arg_reg_count;            // عدد سجلات معاملات الأعداد الصحيحة
    int int_arg_phys_regs[8];         // PhysReg values (from regalloc.h)

    int ret_phys_reg;                 // PhysReg (عادةً RAX)

    unsigned int callee_saved_mask;   // bitmask over PhysReg
    unsigned int caller_saved_mask;   // bitmask over PhysReg

    int stack_align_bytes;            // محاذاة المكدس عند نقاط الاستدعاء (عادة 16)

    // تمثيل سجلات معاملات ABI داخل Machine IR كسجلات افتراضية سالبة
    // arg i -> (abi_arg_vreg0 - i)
    int abi_arg_vreg0;                // افتراضي: -10
    int abi_ret_vreg;                 // افتراضي: -2 (RAX)

    int shadow_space_bytes;           // Windows: 32, SysV: 0
    bool home_reg_args_on_call;       // Windows varargs: true, SysV: false
    bool sysv_set_al_zero_on_call;    // SysV varargs rule: true
} BaaCallingConv;
```

#### `BaaTarget` Struct

Complete target descriptor.

```c
typedef struct BaaTarget {
    BaaTargetKind kind;
    const char* name;                 // short name: x86_64-windows, x86_64-linux
    const char* triple;               // future: full triple

    BaaObjectFormat obj_format;
    const IRDataLayout* data_layout;
    const BaaCallingConv* cc;

    const char* default_exe_ext;      // ".exe" on Windows, "" on Linux
} BaaTarget;
```

#### Target Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `baa_target_builtin_windows_x86_64` | `const BaaTarget* baa_target_builtin_windows_x86_64(void)` | Returns the built-in Windows x86-64 target |
| `baa_target_builtin_linux_x86_64` | `const BaaTarget* baa_target_builtin_linux_x86_64(void)` | Returns the built-in Linux x86-64 target |
| `baa_target_host_default` | `const BaaTarget* baa_target_host_default(void)` | Returns the default target for the host system |
| `baa_target_parse` | `const BaaTarget* baa_target_parse(const char* s)` | Parses a target string ("x86_64-windows", "x86_64-linux") |

---

### 4.13. Code Model (v0.3.2.8.3)

The Code Model module (`src/code_model.h`) defines code generation options.

#### `BaaCodeModel` Enum

```c
typedef enum {
    BAA_CODEMODEL_SMALL = 0,
} BaaCodeModel;
```

#### `BaaStackProtectorMode` Enum

```c
typedef enum {
    BAA_STACKPROT_OFF = 0,
    BAA_STACKPROT_ON  = 1,
    BAA_STACKPROT_ALL = 2,
} BaaStackProtectorMode;
```

#### `BaaCodegenOptions` Struct

```c
typedef struct {
    BaaCodeModel code_model;
    bool pic;                 // -fPIC
    bool pie;                 // -fPIE / link -pie
    BaaStackProtectorMode stack_protector;
    bool asm_comments;        // --asm-comments
} BaaCodegenOptions;
```

#### `baa_codegen_options_default`

```c
static inline BaaCodegenOptions baa_codegen_options_default(void)
```

Returns default codegen options (small code model, no PIC/PIE, no stack protector).

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

#### `ir_builder_add_param`

```c
int ir_builder_add_param(IRBuilder* builder, const char* name, IRType* type)
```

Add a parameter to the current function.

| Parameter | Type | Description |
|-----------|------|-------------|
| `builder` | `IRBuilder*` | The builder |
| `name` | `const char*` | Parameter name |
| `type` | `IRType*` | Parameter type |

**Returns:** Register number assigned to the parameter.

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

#### `ir_builder_create_block_and_set`

```c
IRBlock* ir_builder_create_block_and_set(IRBuilder* builder, const char* label)
```

Create a new block and set it as insertion point.

| Parameter | Type | Description |
|-----------|------|-------------|
| `builder` | `IRBuilder*` | The builder |
| `label` | `const char*` | Block label |

---

#### `ir_builder_set_insert_point`

```c
void ir_builder_set_insert_point(IRBuilder* builder, IRBlock* block)
```

Sets the insertion point. All subsequent emit functions will append to this block.

---

#### `ir_builder_set_module`

```c
void ir_builder_set_module(IRBuilder* builder, IRModule* module)
```

Sets the target module for the builder.

---

#### `ir_builder_set_func`

```c
void ir_builder_set_func(IRBuilder* builder, IRFunc* func)
```

Sets the current function being built.

---

#### `ir_builder_get_func`

```c
IRFunc* ir_builder_get_func(IRBuilder* builder)
```

Gets the current function (or NULL).

---

#### `ir_builder_get_insert_block`

```c
IRBlock* ir_builder_get_insert_block(IRBuilder* builder)
```

Gets the current insertion block (or NULL).

---

#### `ir_builder_is_block_terminated`

```c
int ir_builder_is_block_terminated(IRBuilder* builder)
```

Returns 1 if the current block is terminated, 0 otherwise.

---

### 5.4. Register Allocation

#### `ir_builder_alloc_reg`

```c
int ir_builder_alloc_reg(IRBuilder* builder)
```

Allocates a new virtual register.

**Returns:** New register number (`%م<n>`).

---

#### `ir_builder_reg_value`

```c
IRValue* ir_builder_reg_value(IRBuilder* builder, int reg, IRType* type)
```

Creates a register value for a given register number.

---

#### `ir_builder_set_loc`

```c
void ir_builder_set_loc(IRBuilder* builder, const char* file, int line, int col)
```

Sets source location for subsequent instructions.

| Parameter | Type | Description |
|-----------|------|-------------|
| `builder` | `IRBuilder*` | The builder |
| `file` | `const char*` | Source filename |
| `line` | `int` | Line number |
| `col` | `int` | Column number |

---

#### `ir_builder_clear_loc`

```c
void ir_builder_clear_loc(IRBuilder* builder)
```

Clears source location (for generated code).

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

#### `ir_builder_emit_mod`

```c
int ir_builder_emit_mod(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emits: `%dest = باقي type lhs, rhs`

---

#### `ir_builder_emit_neg`

```c
int ir_builder_emit_neg(IRBuilder* builder, IRType* type, IRValue* operand)
```

Emits: `%dest = سالب type operand`

---

#### `ir_builder_emit_and` / `ir_builder_emit_or` / `ir_builder_emit_xor`

```c
int ir_builder_emit_and(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
int ir_builder_emit_or(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
int ir_builder_emit_xor(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emit bitwise binary operations:
- `%dest = و type lhs, rhs`
- `%dest = أو type lhs, rhs`
- `%dest = أو_حصري type lhs, rhs`

---

#### `ir_builder_emit_not` / `ir_builder_emit_shl` / `ir_builder_emit_shr`

```c
int ir_builder_emit_not(IRBuilder* builder, IRType* type, IRValue* operand)
int ir_builder_emit_shl(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
int ir_builder_emit_shr(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs)
```

Emit unary/shift low-level operations:
- `%dest = نفي type operand`
- `%dest = ازاحة_يسار type lhs, rhs`
- `%dest = ازاحة_يمين type lhs, rhs`

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

#### `ir_builder_emit_ptr_offset`

```c
int ir_builder_emit_ptr_offset(IRBuilder* builder, IRType* ptr_type, IRValue* base, IRValue* index)
```

Emits: `%dest = إزاحة_مؤشر ptr_type base, index`

**Note:** `index` is an element index scaled by `sizeof(pointee)` (target data layout).

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

#### Unsigned Comparison Functions

```c
int ir_builder_emit_cmp_ugt(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أكبر (بدون إشارة)
int ir_builder_emit_cmp_ult(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أصغر (بدون إشارة)
int ir_builder_emit_cmp_uge(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أكبر_أو_يساوي (بدون إشارة)
int ir_builder_emit_cmp_ule(IRBuilder* builder, IRValue* lhs, IRValue* rhs);  // أصغر_أو_يساوي (بدون إشارة)
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

#### `ir_builder_emit_ret_int`

```c
void ir_builder_emit_ret_int(IRBuilder* builder, int64_t value)
```

Emits: `رجوع value` with an integer constant.

---

### 5.9. Emit Functions - Calls

#### `ir_builder_emit_call`

```c
int ir_builder_emit_call(IRBuilder* builder, const char* target, IRType* ret_type, IRValue** args, int arg_count)
```

Emits: `%dest = نداء ret_type @target(args...)`

**Returns:** Destination register (-1 for void calls).

---

#### `ir_builder_emit_call_indirect`

```c
int ir_builder_emit_call_indirect(IRBuilder* builder, IRValue* callee, IRType* ret_type, IRValue** args, int arg_count)
```

Emits: `%dest = نداء ret_type <callee>(args...)` (indirect call through a value of type `IR_TYPE_FUNC`)

**Returns:** Destination register (-1 for void calls).

---

#### `ir_builder_emit_call_void`

```c
void ir_builder_emit_call_void(IRBuilder* builder, const char* target, IRValue** args, int arg_count)
```

Emits: `نداء فراغ @target(args...)`

---

### 5.10. Emit Functions - Type Conversion

#### `ir_builder_emit_cast`

```c
int ir_builder_emit_cast(IRBuilder* builder, IRValue* value, IRType* to_type)
```

Emits: `%dest = تحويل from_type to to_type value`

**Returns:** Destination register number.

---

### 5.11. Control Flow Helpers

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

### 5.12. Constant Helpers

```c
IRValue* ir_builder_const_int(int64_t value);      // i64 constant
IRValue* ir_builder_const_i64(int64_t value);      // i64 constant
IRValue* ir_builder_const_i32(int32_t value);      // i32 constant
IRValue* ir_builder_const_bool(int value);         // i1 constant
IRValue* ir_builder_const_string(IRBuilder* builder, const char* str);  // String constant
IRValue* ir_builder_const_baa_string(IRBuilder* builder, const char* str);  // Baa string (حرف[])
```

---

### 5.13. Global Variables

#### `ir_builder_create_global`

```c
IRGlobal* ir_builder_create_global(IRBuilder* builder, const char* name,
                                    IRType* type, int is_const);
```

Creates a global variable.

---

#### `ir_builder_create_global_init`

```c
IRGlobal* ir_builder_create_global_init(IRBuilder* builder, const char* name,
                                         IRType* type, IRValue* init, int is_const);
```

Creates a global variable with an initializer.

---

#### `ir_builder_get_global`

```c
IRValue* ir_builder_get_global(IRBuilder* builder, const char* name);
```

Gets a reference to a global variable.

---

### 5.14. Statistics and Debugging

#### `ir_builder_get_inst_count`

```c
int ir_builder_get_inst_count(IRBuilder* builder);
```

Returns the number of instructions emitted.

---

#### `ir_builder_get_block_count`

```c
int ir_builder_get_block_count(IRBuilder* builder);
```

Returns the number of blocks created.

---

#### `ir_builder_print_stats`

```c
void ir_builder_print_stats(IRBuilder* builder);
```

Prints builder statistics to stderr.

---

### 5.15. Example Usage

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

- [`src/ir_lower.h`](../src/ir_lower.h)
- [`src/ir_lower.c`](../src/ir_lower.c)

### 6.1. `IRLowerCtx`

```c
typedef struct IRLowerBinding {
    const char* name;
    int ptr_reg;
    IRType* value_type;
    const char* global_name;
    bool is_static_storage;
    bool is_array;
    int array_rank;
    const int* array_dims;
    DataType array_elem_type;
    const char* array_elem_type_name;
} IRLowerBinding;

typedef struct IRLowerCtx {
    IRBuilder* builder;

    IRLowerBinding locals[256];
    int local_count;

    int scope_stack[64];
    int scope_depth;

    int label_counter;
    const char* current_func_name;
    int static_local_counter;

    struct IRBlock* break_targets[64];
    struct IRBlock* continue_targets[64];
    int cf_depth;

    int had_error;
    bool enable_bounds_checks;
    Node* program_root;
} IRLowerCtx;
```

A small context object used during lowering:

- Holds the active `IRBuilder` insertion point
- Tracks local/static variable bindings (including array rank/dim metadata)
- Tracks scope/control-flow stacks during lowering
- Carries explicit optional runtime bounds-check mode + root AST metadata lookup context

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

### 6.4. `ir_lower_bind_local_static`

```c
void ir_lower_bind_local_static(IRLowerCtx* ctx, const char* name, const char* global_name, IRType* value_type);
```

Binds a local name to static storage represented by an internal global symbol.

---

### 6.5. `lower_expr`

```c
IRValue* lower_expr(IRLowerCtx* ctx, Node* expr);
```

Main expression lowering dispatcher.

Currently supports:

- `NODE_INT` → immediate constant
- `NODE_VAR_REF` → `حمل` (load)
- `NODE_BIN_OP` → arithmetic (`جمع`/`طرح`/`ضرب`/`قسم`/`باقي`), comparisons (`قارن`), and bitwise/shift ops (`و`/`أو`/`أو_حصري`/`ازاحة_يسار`/`ازاحة_يمين`)
- `NODE_UNARY_OP` → `سالب`, bitwise `نفي` (`~`), and logical-not lowering via compare-to-zero
- `NODE_SIZEOF` → compile-time constant size (`ص٦٤`)
- `NODE_CALL_EXPR` → `نداء`
- `NODE_RAW_STRING` → static NUL-terminated UTF-8 byte storage (`ط٨*`)
- Builtin string calls (`v0.3.9`) in `NODE_CALL_EXPR`:
  - `طول_نص`, `قارن_نص`
  - `نسخ_نص`, `دمج_نص` (heap-backed via `malloc`)
  - `حرر_نص` (via `free`)
- Builtin raw-byte calls (`v0.9.1.4.5`) in `NODE_CALL_EXPR`:
  - `طول_خام`
  - `قارن_خام`
  - `قارن_خام_بطول`
- Builtin memory/file calls (`v0.3.11`/`v0.3.12`) in `NODE_CALL_EXPR`:
  - `حجز_ذاكرة/تحرير_ذاكرة/إعادة_حجز/نسخ_ذاكرة/تعيين_ذاكرة`
  - `فتح_ملف/اغلق_ملف/اقرأ_ملف/اكتب_ملف/اقرأ_حرف/اكتب_حرف/نهاية_ملف/موقع_ملف/اذهب_لموقع/اقرأ_سطر(ملف)/اكتب_سطر`
- Builtin module calls (`v0.4.2`) in `NODE_CALL_EXPR`:
  - Math: `جذر_تربيعي`, `أس`, `جيب`, `جيب_تمام`, `ظل`, `مطلق`, `عشوائي`
  - System: `متغير_بيئة`, `نفذ_أمر`
  - Time: `وقت_حالي`, `وقت_كنص`
- Builtin error-handling calls (`v0.4.3`) in `NODE_CALL_EXPR`:
  - `تأكد`, `توقف_فوري`
  - `كود_خطأ_النظام`, `ضبط_كود_خطأ_النظام`, `نص_كود_خطأ`

---

### 6.6. `lower_stmt`

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
- `NODE_INLINE_ASM`: lowered to metadata pseudo-call consumed in ISel and emitted as raw assembly lines
- `NODE_IF`, `NODE_WHILE`, `NODE_FOR`, `NODE_SWITCH`
- `NODE_BREAK`, `NODE_CONTINUE`

---

### 6.7. `ir_lower_program` (v0.3.0.7)

```c
IRModule* ir_lower_program(Node* program, const char* module_name,
                           bool enable_bounds_checks, const BaaTarget* target);
```

Top-level entry point for the driver: converts a validated `NODE_PROGRAM` AST into a fully-populated `IRModule`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `program` | `Node*` | Root AST node (must be `NODE_PROGRAM`) |
| `module_name` | `const char*` | Optional module name (usually filename) |
| `enable_bounds_checks` | `bool` | Enables optional runtime bounds-check lowering paths for array accesses, driven by `-fruntime-checks` |
| `target` | `const BaaTarget*` | Active target descriptor (ABI + data layout). Required for target-specific lowering decisions |

**Returns:** Newly allocated `IRModule` (caller owns; free with `ir_module_free()`).

**Behavior:**

1. Creates a new `IRModule` and `IRBuilder`
2. Walks top-level declarations:
   - Global variables (`NODE_VAR_DECL` with `is_global`) → `ir_builder_create_global_init()`
   - Functions (`NODE_FUNC_DEF`) → `ir_builder_create_func()` + parameter spilling + `lower_stmt()`
3. Propagates lowering options/metadata through `IRLowerCtx` (including explicit optional array bounds checks)
4. Returns the fully-lowered module

---

### 6.6. `lower_stmt_list`

```c
void lower_stmt_list(IRLowerCtx* ctx, Node* first_stmt);
```

Lowers a linked list of statements (e.g., the statements list inside `NODE_BLOCK`).

---

## 7. IR Optimization Passes

### 7.1. Mem2Reg (ترقية الذاكرة إلى سجلات)

#### `ir_mem2reg_run`

```c
bool ir_mem2reg_run(IRModule* module)
```

Runs the canonical Mem2Reg pass on the given IR module. Inserts `فاي` (phi) nodes using dominance frontiers and performs SSA renaming to rewrite `حمل/خزن` into SSA values across basic blocks (with safety-first restrictions such as “no pointer escape” and “alloca block dominates all uses”).

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_MEM2REG`

```c
extern IRPass IR_PASS_MEM2REG;
```

Descriptor for the Mem2Reg pass, usable with the IR optimizer pipeline.

---

### 7.1.5. Out-of-SSA (الخروج من SSA)

#### `ir_outssa_run`

```c
bool ir_outssa_run(IRModule* module)
```

Eliminates `فاي` (IR_OP_PHI) before the backend by inserting edge copies and splitting critical edges when needed. This ensures no `phi` reaches ISel/RegAlloc/Emit.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to rewrite out of SSA |

**Returns:** `true` if the module was modified, `false` otherwise.

---

#### `ir_outssa_run_ex`

```c
bool ir_outssa_run_ex(IRModule* module, bool* out_changed)
```

Run Out-of-SSA with separate success/changed indicators.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module` | `IRModule*` | The IR module to rewrite out of SSA |
| `out_changed` | `bool*` | Optional pointer to receive change status (1 = changed, 0 = not changed) |

**Returns:** `true` on success, `false` on internal failure or invalid IR.

---

### 7.1.6. SSA Verification (التحقق من SSA)

#### `ir_func_verify_ssa`

```c
bool ir_func_verify_ssa(IRFunc* func, FILE* out)
```

Validates SSA invariants for a single IR function. Intended to run **after Mem2Reg** and **before** the Out-of-SSA rewrite.

| Parameter | Type | Description |
|-----------|------|-------------|
| `func`    | `IRFunc*` | Target function |
| `out`     | `FILE*` | Diagnostics output (typically `stderr`) |

**Returns:** `true` if SSA is valid; `false` otherwise.

#### `ir_module_verify_ssa`

```c
bool ir_module_verify_ssa(IRModule* module, FILE* out)
```

Validates SSA invariants for all (non-prototype) functions in a module.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | Target IR module |
| `out`     | `FILE*` | Diagnostics output (typically `stderr`) |

**Returns:** `true` if all functions are valid; `false` otherwise.

**Driver flag:** `--verify-ssa` runs this check after optimization and before Out-of-SSA, and requires `-O1`/`-O2`.

---

### 7.1.7. IR Well-Formedness Verification (التحقق من سلامة الـ IR) — v0.3.2.6.5

#### `ir_func_verify_ir`

```c
bool ir_func_verify_ir(IRFunc* func, FILE* out)
```

Validates general IR invariants for a single IR function (non SSA-specific): operand counts, type consistency, terminator rules, phi placement/incoming shape, and basic call structure.

| Parameter | Type | Description |
|-----------|------|-------------|
| `func`    | `IRFunc*` | Target function |
| `out`     | `FILE*` | Diagnostics output (typically `stderr`) |

**Returns:** `true` if IR is well-formed; `false` otherwise.

#### `ir_module_verify_ir`

```c
bool ir_module_verify_ir(IRModule* module, FILE* out)
```

Validates general IR invariants for all (non-prototype) functions in a module. When the callee exists inside the same module, the verifier also checks call arity/types against the callee signature.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | Target IR module |
| `out`     | `FILE*` | Diagnostics output (typically `stderr`) |

**Returns:** `true` if all functions are valid; `false` otherwise.

**Driver flags:**

- `--verify-ir` runs this check after optimization and before Out-of-SSA.
- `--verify-gate` runs IR/SSA verification after each optimizer iteration (**requires `-O1`/`-O2`**).

---

### 7.1.8. Canonicalization (توحيد_الـ IR) — v0.3.2.6.5

#### `ir_canon_run`

```c
bool ir_canon_run(IRModule* module)
```

Normalizes commutative operand ordering, constant placement, and comparison forms to improve optimization effectiveness.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | Target IR module |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_CANON`

```c
extern IRPass IR_PASS_CANON;
```

Descriptor for the canonicalization pass, usable with the IR optimizer pipeline.

---

### 7.1.9. CFG Simplification (تبسيط_CFG) — v0.3.2.6.5

#### `ir_cfg_simplify_run`

```c
bool ir_cfg_simplify_run(IRModule* module)
```

Simplifies the IR control-flow graph by removing redundant branches and merging trivial blocks conservatively.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | Target IR module |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `ir_cfg_split_critical_edge`

```c
IRBlock* ir_cfg_split_critical_edge(IRFunc* func, IRBlock* pred, IRBlock* succ)
```

Splits a critical edge `pred -> succ` by inserting a new block when needed (pred has multiple successors and succ has multiple predecessors). Returns the inserted block, or `succ` if the edge is not critical.

| Parameter | Type | Description |
|-----------|------|-------------|
| `func` | `IRFunc*` | Target function |
| `pred` | `IRBlock*` | Predecessor block |
| `succ` | `IRBlock*` | Successor block |

**Returns:** new split block, or `succ` if no split was required, or `NULL` on failure.

#### `IR_PASS_CFG_SIMPLIFY`

```c
extern IRPass IR_PASS_CFG_SIMPLIFY;
```

Descriptor for the CFG simplification pass, usable with the IR optimizer pipeline.

---

### 7.1.10. LICM (Loop Invariant Code Motion) — v0.3.2.7.1

#### `ir_licm_run`

```c
bool ir_licm_run(IRModule* module)
```

Conservatively hoists pure, non-trapping loop-invariant instructions to loop preheaders (requires a unique preheader).

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | Target IR module |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_LICM`

```c
extern IRPass IR_PASS_LICM;
```

Descriptor for the LICM pass.

---

### 7.1.11. Inlining (تضمين الدوال) — v0.3.2.7.2

#### `ir_inline_run`

```c
bool ir_inline_run(IRModule* module)
```

Conservatively inlines small internal functions with a single call site at `-O2`. The inliner runs before Mem2Reg; post-inline cleanup is handled by the existing optimizer pipeline.

---

### 7.1.12. InstCombine (دمج_التعليمات) — v0.3.2.8.6

#### `ir_instcombine_run`

```c
bool ir_instcombine_run(IRModule* module)
```

Runs local instruction combining/simplification on the given IR module. Rewrites eligible instructions into `IR_OP_COPY` and constants to improve later passes.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_INSTCOMBINE`

```c
extern IRPass IR_PASS_INSTCOMBINE;
```

Descriptor for the InstCombine pass.

---

### 7.1.13. SCCP (نشر_الثوابت_المتناثر) — v0.3.2.8.6

#### `ir_sccp_run`

```c
bool ir_sccp_run(IRModule* module)
```

Runs sparse conditional constant propagation (SCCP) on the given IR module. Propagates integer constants through SSA, tracks reachability, and can fold `IR_OP_BR_COND` into `IR_OP_BR`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_SCCP`

```c
extern IRPass IR_PASS_SCCP;
```

Descriptor for the SCCP pass.

---

### 7.1.14. GVN (ترقيم_القيم) — v0.3.2.8.6

#### `ir_gvn_run`

```c
bool ir_gvn_run(IRModule* module)
```

Runs global value numbering (GVN) on the given IR module. Removes redundant pure expressions across dominator scopes.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |

**Returns:** `true` if the module was modified, `false` otherwise.

#### `IR_PASS_GVN`

```c
extern IRPass IR_PASS_GVN;
```

Descriptor for the GVN pass.

---

### 7.2. Constant Folding (طي_الثوابت)

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

### 7.3. Dead Code Elimination (حذف_الميت)

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

### 7.4. Copy Propagation (نشر_النسخ)

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

### 7.5. Common Subexpression Elimination (حذف_المكرر)

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

### 7.6. Optimization Pipeline (v0.3.1.6, updated v0.3.2.8.6)

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
- **O1:** Basic optimizations (Mem2Reg, Canon, InstCombine, SCCP, constfold, copyprop, DCE, CFG simplify, LICM).
- **O2:** Full optimizations (+ inlining, GVN, CSE, fixpoint iteration).

#### `ir_optimizer_run`

```c
bool ir_optimizer_run(IRModule* module, OptLevel level)
```

Runs the optimization pipeline on the given IR module.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module`  | `IRModule*` | The IR module to optimize |
| `level`   | `OptLevel` | Optimization level (O0, O1, O2) |

**Returns:** `true` on success, `false` only on optimizer/verification failure.

**Pass ordering:**
0. (O2) Inlining (تضمين الدوال) — runs once before the fixpoint loop
1. Mem2Reg (ترقية الذاكرة إلى سجلات) — phi insertion + SSA renaming
2. Canonicalization (توحيد_الـIR)
3. InstCombine (دمج_التعليمات)
4. SCCP (نشر_الثوابت_المتناثر)
5. Constant Folding (طي_الثوابت)
6. Copy Propagation (نشر_النسخ)
7. (O2) GVN (ترقيم_القيم)
8. (O2) CSE (حذف_المكرر)
9. Dead Code Elimination (حذف_الميت)
10. CFG Simplification (تبسيط_CFG)
11. LICM (حركة التعليمات غير المتغيرة)

**Note:** Out-of-SSA (`ir_outssa_run()`) is executed by the driver before ISel, not as part of the optimizer fixpoint loop.

**Fixpoint iteration:** Passes repeat until no changes (max 10 iterations).

#### `ir_optimizer_set_verify_gate`

```c
void ir_optimizer_set_verify_gate(int enabled)
```

Enable/disable verification gate inside the optimizer. When enabled, runs `ir_module_verify_ir()` and `ir_module_verify_ssa()` after each optimizer iteration.

| Parameter | Type | Description |
|-----------|------|-------------|
| `enabled` | `int` | 1 to enable, 0 to disable |

---

#### `ir_optimizer_level_name`

```c
const char* ir_optimizer_level_name(OptLevel level)
```

Returns the string representation of an optimization level ("O0", "O1", "O2").

---

## 8. Instruction Selection Module (v0.3.2.1)

The Instruction Selection module (`src/isel.h`, `src/isel.c`) converts IR to abstract machine instructions for x86-64.

### 8.0. Machine IR Structures (v0.3.2.1)

#### `MachineOp` Enum
Supported x86-64 machine opcodes.

```c
typedef enum {
    // Arithmetic
    MACH_ADD, MACH_SUB, MACH_IMUL, MACH_SHL, MACH_SHR, MACH_SAR, MACH_IDIV, MACH_DIV, MACH_NEG, MACH_CQO,
    // Floating Point (SSE2)
    MACH_ADDSD, MACH_SUBSD, MACH_MULSD, MACH_DIVSD, MACH_UCOMISD, MACH_XORPD, MACH_CVTSI2SD, MACH_CVTTSD2SI,
    // Data Movement
    MACH_MOV, MACH_LEA, MACH_LOAD, MACH_STORE,
    // Comparison & Flags
    MACH_CMP, MACH_TEST, MACH_SETE, MACH_SETNE, MACH_SETG, MACH_SETL, MACH_SETGE, MACH_SETLE,
    MACH_SETA, MACH_SETB, MACH_SETAE, MACH_SETBE, MACH_SETP, MACH_SETNP, MACH_MOVZX, MACH_MOVSX,
    // Logical
    MACH_AND, MACH_OR, MACH_NOT, MACH_XOR,
    // Control Flow
    MACH_JMP, MACH_JE, MACH_JNE, MACH_CALL, MACH_TAILJMP, MACH_RET,
    // Stack
    MACH_PUSH, MACH_POP,
    // Special
    MACH_NOP, MACH_LABEL, MACH_COMMENT
} MachineOp;
```

#### `MachineOperand` Struct

```c
typedef struct MachineOperand {
    MachineOperandKind kind;
    int size_bits;              
    union {
        int vreg;               // MACH_OP_VREG
        int64_t imm;            // MACH_OP_IMM
        struct { int base_vreg; int32_t offset; } mem; // MACH_OP_MEM
        int label_id;           // MACH_OP_LABEL
        char* name;             // MACH_OP_GLOBAL, MACH_OP_FUNC
        int xmm;                // MACH_OP_XMM
    } data;
} MachineOperand;
```

#### `MachineInst` Struct

```c
typedef struct MachineInst {
    MachineOp op;
    MachineOperand dst;
    MachineOperand src1;
    MachineOperand src2;
    int ir_reg;
    const char* comment;
    const char* src_file;
    int src_line;
    int src_col;
    int ir_inst_id;
    const char* dbg_name;
    int sysv_al;
    struct MachineInst* prev;
    struct MachineInst* next;
} MachineInst;
```

### 8.1. Operand Constructors

#### `mach_op_vreg` / `mach_op_xmm`

```c
MachineOperand mach_op_vreg(int vreg, int bits);
MachineOperand mach_op_xmm(int xmm);
```

#### `mach_op_imm`

```c
MachineOperand mach_op_imm(int64_t imm, int bits)
```

Creates an immediate value operand.

---

#### `mach_op_mem`

```c
MachineOperand mach_op_mem(int base_vreg, int32_t offset, int bits)
```

Creates a memory operand `[base + offset]`.

---

#### `mach_op_label` / `mach_op_global` / `mach_op_func`

```c
MachineOperand mach_op_label(int label_id);
MachineOperand mach_op_global(const char* name);
MachineOperand mach_op_func(const char* name);
```

---

#### `mach_op_none`

```c
MachineOperand mach_op_none(void)
```

---

### 8.2. Instruction Construction

#### `mach_inst_new`

```c
MachineInst* mach_inst_new(MachineOp op, MachineOperand dst,
                           MachineOperand src1, MachineOperand src2)
```

---

### 8.4. Function & Module Construction

#### `mach_func_new` / `mach_module_new`

```c
MachineFunc* mach_func_new(const char* name);
MachineModule* mach_module_new(const char* name);
```

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
MachineModule* isel_run_ex(IRModule* ir_module, bool enable_tco, const BaaTarget* target)
```

Converts an entire IR module to machine representation.

| Parameter | Type | Description |
|-----------|------|-------------|
| `ir_module` | `IRModule*` | Source IR module (after optimization) |
| `enable_tco` | `bool` | Enable tail call optimization in ISel (used by `-O2`) |
| `target` | `BaaTarget*` | Target descriptor (ABI + object format); NULL defaults to Windows x64 |

**Returns:** New `MachineModule*` (caller owns; free with `mach_module_free()`), or `NULL` on failure.

**Behavior:**

1. Creates a `MachineModule` and walks all IR functions
2. For each IR function, creates a `MachineFunc` with matching blocks
3. For each IR instruction, emits one or more `MachineInst` nodes using pattern matching
4. Inlines immediate values where x86-64 permits
5. Uses the selected target calling convention for calls/returns (Windows x64 or SystemV AMD64)

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
    PHYS_RAX = 0,   // سجل المراكم / القيمة المرجعة
    PHYS_RCX = 1,   // سجل عام / ضمن سجلات معاملات ABI حسب الهدف
    PHYS_RDX = 2,   // سجل عام / باقي القسمة
    PHYS_RBX = 3,   // سجل محفوظ (callee-saved)
    PHYS_RSP = 4,   // مؤشر المكدس (محجوز)
    PHYS_RBP = 5,   // مؤشر الإطار (محجوز)
    PHYS_RSI = 6,   // سجل عام
    PHYS_RDI = 7,   // سجل عام
    PHYS_R8  = 8,   // سجل عام / ضمن سجلات معاملات ABI حسب الهدف
    PHYS_R9  = 9,   // سجل عام / ضمن سجلات معاملات ABI حسب الهدف
    PHYS_R10 = 10,  // سجل مؤقت (caller-saved)
    PHYS_R11 = 11,  // سجل مؤقت (caller-saved)
    PHYS_R12 = 12,  // سجل محفوظ (callee-saved)
    PHYS_R13 = 13,  // سجل محفوظ (callee-saved)
    PHYS_R14 = 14,  // سجل محفوظ (callee-saved)
    PHYS_R15 = 15,  // سجل محفوظ (callee-saved)

    PHYS_REG_COUNT = 16,  // عدد السجلات الفيزيائية
    PHYS_NONE = -1        // لا سجل مخصص
} PhysReg;
```

x86-64 physical register numbering. RSP and RBP are always reserved and never allocated.

### 9.2. Data Structures

#### `LiveInterval`

```c
typedef struct LiveInterval {
    int vreg;           // رقم السجل الافتراضي
    int start;          // موقع البداية (أول تعريف)
    int end;            // موقع النهاية (آخر استخدام)

    // نتيجة التخصيص
    PhysReg phys_reg;   // السجل الفيزيائي المخصص (PHYS_NONE إذا سُرّب)
    bool spilled;       // هل تم تسريبه إلى المكدس؟
    int spill_offset;   // إزاحة التسريب في المكدس (بالنسبة لـ RBP)
} LiveInterval;
```

Represents the live range of a virtual register from its definition to its last use.

#### `BlockLiveness`

```c
typedef struct BlockLiveness {
    int block_id;       // معرف الكتلة

    // مجموعات البتات (كل بت يمثل سجل افتراضي)
    uint64_t* def;      // السجلات المعرّفة في هذه الكتلة
    uint64_t* use;      // السجلات المستخدمة قبل تعريفها في هذه الكتلة
    uint64_t* live_in;  // السجلات الحية عند دخول الكتلة
    uint64_t* live_out; // السجلات الحية عند خروج الكتلة
} BlockLiveness;
```

Per-block liveness bitsets used in iterative dataflow analysis.

#### `RegAllocCtx`

```c
typedef struct RegAllocCtx {
    MachineFunc* func;          // الدالة الآلية المعالَجة

    // اتفاقية الاستدعاء الفعّالة (للتمييز بين Windows x64 و SysV)
    const BaaCallingConv* cc;

    // ترقيم التعليمات
    int total_insts;            // عدد التعليمات الإجمالي
    MachineInst** inst_map;     // خريطة رقم → تعليمة

    // حيوية الكتل
    BlockLiveness* block_live;  // مصفوفة حيوية لكل كتلة
    int block_count;            // عدد الكتل

    // فترات الحيوية
    LiveInterval* intervals;    // مصفوفة فترات الحيوية
    int interval_count;         // عدد الفترات
    int interval_capacity;      // سعة المصفوفة

    // حجم مصفوفات البتات (بعدد الكلمات uint64_t)
    int bitset_words;           // عدد كلمات uint64_t في كل مجموعة بتات
    int max_vreg;               // أعلى رقم سجل افتراضي + 1

    // نتائج التخصيص
    PhysReg* vreg_to_phys;      // خريطة vreg → سجل فيزيائي
    bool* vreg_spilled;         // هل تم تسريب كل vreg؟
    int* vreg_spill_offset;     // إزاحة التسريب لكل vreg

    // معلومات التسريب
    int next_spill_offset;      // الإزاحة التالية المتاحة للتسريب
    int spill_count;            // عدد السجلات المسرّبة

    // السجلات المحفوظة (callee-saved) المستخدمة
    bool callee_saved_used[PHYS_REG_COUNT];
} RegAllocCtx;
```

Main context for register allocation of a single function. Holds all intermediate and final allocation state.

### 9.3. Entry Points

#### `regalloc_run`

```c
bool regalloc_run(MachineModule* module);
bool regalloc_run_ex(MachineModule* module, const BaaTarget* target);
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

## 10. Code Emission Module (v0.3.2.3)

The Code Emission module (`src/emit.h`, `src/emit.c`) converts machine IR (after register allocation) to x86-64 AT&T assembly.

### 10.1. Entry Points

#### `emit_module`

```c
bool emit_module(MachineModule* module, FILE* out, bool debug_info)
bool emit_module_ex(MachineModule* module, FILE* out, bool debug_info, const BaaTarget* target)
bool emit_module_ex2(MachineModule* module, FILE* out, bool debug_info, const BaaTarget* target, BaaCodegenOptions opts)
```

Top-level entry point for emitting a complete assembly file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `module` | `MachineModule*` | Machine module with physical registers |
| `out` | `FILE*` | Output file handle for assembly |
| `debug_info` | `bool` | Emit `.file`/`.loc` directives and debug breadcrumbs |
| `target` | `BaaTarget*` | Target descriptor (COFF/ELF + ABI); NULL defaults to Windows x64 |
| `opts` | `BaaCodegenOptions` | Code model options (PIC/PIE/stack protector) + `asm_comments` for `--asm-comments` |

**Returns:** `true` on success, `false` on failure.

**Behavior:**

1. Emits read-only data section with format strings (COFF: `.rdata`, ELF: `.rodata`)
2. Emits `.data` section with global variables and initializers (including fixed-size typed global arrays with partial init + zero-fill, plus pointer-array string label initializers and compound storage for `هيكل`/`اتحاد`)
3. Emits `.text` section with all functions
4. Emits string table with `.Lstr_N` labels into read-only data section (COFF/ELF)

**Driver flags:** `--debug-info` enables debug directives, `--asm-comments` enables explanatory assembly comments, and `-fruntime-checks` controls optional runtime safety lowering before backend emission.

---

#### `emit_func`

```c
bool emit_func(MachineFunc* func, FILE* out)
```

Emits a single function with prologue and epilogue.

| Parameter | Type | Description |
|-----------|------|-------------|
| `func` | `MachineFunc*` | Function to emit |
| `out` | `FILE*` | Output file handle |

**Returns:** `true` on success, `false` on failure.

**Behavior:**

1. Emits function label (translates Arabic names: الرئيسية → main)
2. Emits prologue (stack setup, callee-saved preservation)
3. Emits all instructions in all blocks
4. Emits epilogue (callee-saved restoration, stack teardown)

---

#### `emit_inst`

```c
void emit_inst(MachineInst* inst, MachineFunc* func, FILE* out)
```

Translates a single machine instruction to AT&T assembly.

| Parameter | Type | Description |
|-----------|------|-------------|
| `inst` | `MachineInst*` | Instruction to emit |
| `func` | `MachineFunc*` | Owning function (context) |
| `out` | `FILE*` | Output file handle |

**Supported Operations:**

- Data movement: MOV, LEA, LOAD, STORE
- Arithmetic: ADD, SUB, IMUL, NEG, CQO, IDIV
- Comparison: CMP, TEST, SETcc (6 variants), MOVZX
- Logical: AND, OR, NOT, XOR
- Control flow: JMP, JE, JNE, CALL, RET
- Stack: PUSH, POP

---

### 10.2. Prologue/Epilogue Generation

#### `emit_prologue`

```c
static void emit_prologue(MachineFunc* func, FILE* out, bool* callee_saved_used)
```

Emits function prologue with stack setup and callee-saved register preservation.

**Generated Code:**

```asm
push %rbp
mov %rsp, %rbp
sub $N, %rsp           # N = stack_size + shadow_space + callee_saved (16-byte aligned)
mov %rbx, -8(%rbp)     # Save callee-saved registers (if used)
mov %r12, -16(%rbp)
...
```

---

#### `emit_epilogue`

```c
static void emit_epilogue(MachineFunc* func, FILE* out, bool* callee_saved_used)
```

Emits function epilogue with callee-saved register restoration and stack teardown.

**Generated Code:**

```asm
mov -16(%rbp), %r12    # Restore callee-saved registers (reverse order)
mov -8(%rbp), %rbx
leave                  # mov %rbp, %rsp; pop %rbp
ret
```

---

### 10.3. Utility Functions

#### `phys_reg_name_att`

```c
const char* phys_reg_name_att(PhysReg reg, int size_bits)
```

Returns AT&T register name with size suffix.

| Parameter | Type | Description |
|-----------|------|-------------|
| `reg` | `PhysReg` | Physical register enum |
| `size_bits` | `int` | Size in bits (8, 16, 32, 64) |

**Returns:** Register name string (e.g., `"%rax"`, `"%eax"`, `"%ax"`, `"%al"`).

---

#### `size_suffix`

```c
static char size_suffix(int bits)
```

Returns AT&T size suffix for instruction.

| Bits | Suffix | Example |
|------|--------|---------|
| 64 | `q` | `movq` |
| 32 | `l` | `movl` |
| 16 | `w` | `movw` |
| 8 | `b` | `movb` |

---

#### `translate_func_name`

```c
static const char* translate_func_name(const char* name)
```

Translates Arabic function names to C runtime equivalents.

| Baa Name | Translated Name |
|----------|-----------------|
| `الرئيسية` | `main` |
| `اطبع` | `printf` |
| `اقرأ` | `scanf` |

---

### 10.4. Design Notes

**AT&T Syntax:**
- Operand order: `source, destination` (opposite of Intel)
- Register prefix: `%` (e.g., `%rax`)
- Immediate prefix: `$` (e.g., `$10`)
- Size suffixes: `q` (64-bit), `l` (32-bit), `w` (16-bit), `b` (8-bit)

**ABI (Target-dependent):**

- **Windows x64:** shadow space (32 bytes) around calls; args in RCX/RDX/R8/R9; return in RAX.
- **SystemV AMD64 (Linux):** no shadow space; args in RDI/RSI/RDX/RCX/R8/R9; return in RAX; varargs require `AL=0` when no XMM args.

**Optimizations:**
- Redundant move elimination: skips `mov %reg, %reg`
- Callee-saved detection: only preserves registers actually used
- Call frame management: target-dependent (shadow space on Windows, SysV call sequence on ELF)

---

## 12. Diagnostic System

Centralized diagnostic system for compiler errors and warnings (v0.2.8+).

### 11.1. Error Reporting

#### `error_init`

```c
void error_init(const char* source)
```

Initializes the error reporting system with the source code for context display.

---

#### `error_register_source`

```c
void error_register_source(const char* filename, const char* source)
```

Registers a source file for use when printing error/warning context.

| Parameter | Type | Description |
|-----------|------|-------------|
| `filename` | `const char*` | The filename |
| `source` | `const char*` | The source code content |

---

#### `error_report`

```c
void error_report_loc(const char* filename, int line, int col, const char* message, ...)
void error_report_span(DiagnosticSpan span, const char* message, ...)
void error_report_span_hint(DiagnosticSpan span, const char* hint, const char* message, ...)
```

Reports an error with source location, line context, and a pointer to the error position.

The compatibility surface used throughout the compiler is still:

```c
#define error_report(token_like, ...) \
    error_report_loc((token_like).filename, (token_like).line, (token_like).col, __VA_ARGS__)
```

Span-aware callers can pass:

```c
typedef struct {
    const char* filename;
    int line;
    int col;
    int end_col;
} DiagnosticSpan;
```

**Features:**

- Displays filename, line, and column
- Shows the actual source line with a `^` pointer or span-width underline
- Supports Arabic `مساعدة:` hint lines for curated common errors
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
void warning_report_span(WarningType type, DiagnosticSpan span, const char* message, ...)
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
- Supports numeric diagnostics flags: `-Wimplicit-narrowing`, `-Wsigned-unsigned-compare`

### 8.3. Warning Types

```c
typedef enum {
    WARN_UNUSED_VARIABLE,    // Variable declared but never used
    WARN_DEAD_CODE,          // Unreachable code after return/break
    WARN_IMPLICIT_RETURN,    // Function without explicit return (future)
    WARN_SHADOW_VARIABLE,    // Local variable shadows global
    WARN_IMPLICIT_NARROWING, // Potentially lossy implicit numeric conversion
    WARN_SIGNED_UNSIGNED_COMPARE, // Mixed signed/unsigned comparison
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

Defined in `src/frontend/analysis.h` as part of the frontend-owned semantic-analysis surface.

```c
typedef struct {
    char name[32];         // اسم الرمز
    ScopeType scope;       // النطاق (عام أو محلي)
    DataType type;         // نوع البيانات (للمتغير: نوعه، للمصفوفة: نوع العنصر)
    char type_name[32];    // اسم النوع عند TYPE_ENUM/TYPE_STRUCT (فارغ لغير ذلك)
    DataType ptr_base_type;      // نوع أساس المؤشر عندما type == TYPE_POINTER
    char ptr_base_type_name[32]; // اسم النوع المركب لأساس المؤشر
    int ptr_depth;               // عمق المؤشر عندما type == TYPE_POINTER
    FuncPtrSig* func_sig;        // توقيع مؤشر الدالة عندما type == TYPE_FUNC_PTR
    bool is_array;         // هل الرمز مصفوفة؟
    int array_rank;        // عدد الأبعاد
    int64_t array_total_elems; // حاصل ضرب الأبعاد
    int* array_dims;       // أبعاد المصفوفة (مملوك لجدول الرموز)
    int offset;            // الإزاحة في المكدس أو العنوان
    bool is_const;         // هل هو ثابت (immutable)؟
    bool is_static;        // هل التخزين ساكن؟
    bool is_used;          // هل تم استخدام هذا المتغير؟ (للتحذيرات)
    int decl_line;         // سطر التعريف (للتحذيرات)
    int decl_col;          // عمود التعريف (للتحذيرات)
    const char* decl_file; // ملف التعريف (للتحذيرات)
} Symbol;
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | `char[32]` | Variable identifier (Arabic UTF-8) |
| `scope` | `ScopeType` | `SCOPE_GLOBAL` or `SCOPE_LOCAL` |
| `type` | `DataType` | Variable type (or array element type) |
| `type_name` | `char[32]` | Type name when `type` is `TYPE_ENUM`/`TYPE_STRUCT`/`TYPE_UNION` |
| `is_array` | `bool` | `true` if this symbol is an array |
| `array_rank` | `int` | Number of dimensions for array symbols |
| `array_total_elems` | `int64_t` | Product of all dimension sizes |
| `array_dims` | `int*` | Owned dimension array (`rank` entries) |
| `offset` | `int` | Stack offset (local) or memory address (global) |
| `is_const` | `bool` | `true` if declared with `ثابت` (v0.2.7+) |
| `is_static` | `bool` | `true` if declared with `ساكن` (v0.3.7.5) |
| `is_used` | `bool` | `true` if variable is referenced (v0.2.8+) |
| `decl_line` | `int` | Line number where symbol was declared (v0.2.8+) |
| `decl_col` | `int` | Column number where symbol was declared (v0.2.8+) |
| `decl_file` | `const char*` | Filename where symbol was declared (v0.2.8+) |
| `ptr_base_type` | `DataType` | The base type for pointers |
| `ptr_base_type_name` | `char[32]` | The name of the struct/union/enum base type |
| `ptr_depth` | `int` | The depth of the pointer (0 if not a pointer) |
| `func_sig` | `FuncPtrSig*` | The function pointer signature if type is TYPE_FUNC_PTR |

> **Note**: Symbol table management functions are implemented as static helper functions within `analysis.c`. The `Symbol` struct definition now lives in `src/frontend/analysis.h`.

### DataType Enum

Compound types are tracked by `DataType` + an optional `type_name`.

```c
typedef enum {
    TYPE_INT,           // صحيح / ص٦٤ (int64)

    // أحجام الأعداد الصحيحة (v0.3.5.5)
    TYPE_I8,            // ص٨
    TYPE_I16,           // ص١٦
    TYPE_I32,           // ص٣٢
    TYPE_U8,            // ط٨
    TYPE_U16,           // ط١٦
    TYPE_U32,           // ط٣٢
    TYPE_U64,           // ط٦٤

    TYPE_STRING,        // نص (حرف[])
    TYPE_POINTER,       // مؤشر عام
    TYPE_FUNC_PTR,      // مؤشر دالة: دالة(...) -> نوع
    TYPE_BOOL,          // منطقي (bool - stored as byte)
    TYPE_CHAR,          // حرف (UTF-8 sequence)
    TYPE_FLOAT,         // عشري (float64)
    TYPE_VOID,          // عدم (void)
    TYPE_ENUM,          // تعداد (يُخزن كـ int64)
    TYPE_STRUCT,        // هيكل (ليس قيمة من الدرجة الأولى)
    TYPE_UNION          // اتحاد (ليس قيمة من الدرجة الأولى)
} DataType;

typedef struct FuncPtrSig {
    DataType return_type;
    DataType return_ptr_base_type;
    char* return_ptr_base_type_name; // مملوك (قد يكون NULL)
    int return_ptr_depth;
    bool is_variadic;                // هل التوقيع متغير المعاملات ( ... )

    int param_count;
    DataType* param_types;              // مملوك (malloc)
    DataType* param_ptr_base_types;     // مملوك (malloc)
    char** param_ptr_base_type_names;   // مملوك (malloc) وعناصره مملوكة (strdup) وقد تكون NULL
    int* param_ptr_depths;              // مملوك (malloc)
} FuncPtrSig;
```

---

## 11. Updater

### `run_updater`

```c
void run_updater(void);
```

Runs the built-in updater.

**Notes:**

- Implemented in `src/updater.c` on Windows builds.
- Windows-only implementation (links against `urlmon`, `wininet`, `wintrust`, and `crypt32`).
- Verifies the downloaded installer using Authenticode signature validation before execution.
- Non-Windows builds provide a stub implementation in `src/updater_stub.c`.
- Invoked from the CLI as `baa update` (must be the only argument).

---

## 12. Data Structures

### 11.1. Lexer Structures

```c
typedef struct {
    char* source;
    char* cur_char;
    const char* filename;
    int line;
    int col;
} LexerState;

typedef struct {
    LexerState state;
    const char* const* include_dirs;
    size_t include_dir_count;
    LexerState stack[10];
    int stack_depth;
    char** dependency_paths;
    size_t dependency_count;
    size_t dependency_capacity;
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
    TOKEN_EOF,          // نهاية الملف
    TOKEN_INT,          // رقم صحيح: 123
    TOKEN_FLOAT,        // رقم عشري: 3.14
    TOKEN_STRING,       // نص: "مرحباً"
    TOKEN_CHAR,         // حرف: 'أ'
    TOKEN_IDENTIFIER,   // معرف: اسم متغير أو دالة (س، ص، الرئيسية)
    
    // Keywords (الكلمات المفتاحية)
    TOKEN_KEYWORD_INT,  // صحيح
    TOKEN_KEYWORD_I8,   // ص٨
    TOKEN_KEYWORD_I16,  // ص١٦
    TOKEN_KEYWORD_I32,  // ص٣٢
    TOKEN_KEYWORD_I64,  // ص٦٤
    TOKEN_KEYWORD_U8,   // ط٨
    TOKEN_KEYWORD_U16,  // ط١٦
    TOKEN_KEYWORD_U32,  // ط٣٢
    TOKEN_KEYWORD_U64,  // ط٦٤
    TOKEN_KEYWORD_STRING, // نص
    TOKEN_KEYWORD_BOOL, // منطقي
    TOKEN_KEYWORD_CHAR, // حرف
    TOKEN_KEYWORD_FLOAT, // عشري
    TOKEN_KEYWORD_FLOAT32, // عشري٣٢ (alias إلى TYPE_FLOAT في v0.4.2)
    TOKEN_KEYWORD_VOID, // عدم
    TOKEN_CAST,         // كـ
    TOKEN_SIZEOF,       // حجم
    TOKEN_TYPE_ALIAS,   // نوع
    TOKEN_CONST,        // ثابت
    TOKEN_STATIC,       // ساكن
    TOKEN_RETURN,       // إرجع
    TOKEN_PRINT,        // اطبع
    TOKEN_READ,         // اقرأ
    TOKEN_ASM,          // مجمع
    TOKEN_IF,           // إذا
    TOKEN_ELSE,         // وإلا
    TOKEN_WHILE,        // طالما
    TOKEN_FOR,          // لكل
    TOKEN_BREAK,        // توقف
    TOKEN_CONTINUE,     // استمر
    TOKEN_SWITCH,       // اختر
    TOKEN_CASE,         // حالة
    TOKEN_DEFAULT,      // افتراضي
    TOKEN_TRUE,         // صواب
    TOKEN_FALSE,        // خطأ

    // Compound Types (أنواع مركبة)
    TOKEN_ENUM,         // تعداد
    TOKEN_STRUCT,       // هيكل
    TOKEN_UNION,        // اتحاد
    
    // Symbols (الرموز)
    TOKEN_ASSIGN,       // =
    TOKEN_DOT,          // .
    TOKEN_ELLIPSIS,     // ...
    TOKEN_COMMA,        // ,
    TOKEN_COLON,        // :
    TOKEN_SEMICOLON,    // ؛
    
    // Math (العمليات الحسابية)
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_INC,          // ++
    TOKEN_DEC,          // --
    
    // Logic / Relational (العمليات المنطقية والعلائقية)
    TOKEN_EQ,           // ==
    TOKEN_NEQ,          // !=
    TOKEN_LT,           // <
    TOKEN_GT,           // >
    TOKEN_LTE,          // <=
    TOKEN_GTE,          // >=
    TOKEN_AND,          // &&
    TOKEN_OR,           // ||
    TOKEN_NOT,          // !
    TOKEN_AMP,          // &
    TOKEN_PIPE,         // |
    TOKEN_CARET,        // ^
    TOKEN_TILDE,        // ~
    TOKEN_SHL,          // <<
    TOKEN_SHR,          // >>
    
    // Grouping (التجميع)
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    
    TOKEN_INVALID       // وحدة غير صالحة
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

    // Source location info
    const char* filename;
    int line;
    int col;

    // Semantic analysis inferred types
    DataType inferred_type;
    DataType inferred_ptr_base_type;
    char* inferred_ptr_base_type_name;
    int inferred_ptr_depth;
    FuncPtrSig* inferred_func_sig;

    union {
        struct { struct Node* declarations; } program;
        struct { struct Node* statements; } block;

        struct {
            char* name;
            DataType return_type;
            DataType return_ptr_base_type;
            char* return_ptr_base_type_name;
            int return_ptr_depth;
            FuncPtrSig* return_func_sig;
            struct Node* params;
            struct Node* body;   // NULL if prototype
            bool is_variadic;
            bool is_prototype;
        } func_def;

        struct {
            char* name;
            DataType type;
            char* type_name;         // Used for TYPE_ENUM/TYPE_STRUCT/TYPE_UNION
            DataType ptr_base_type;
            char* ptr_base_type_name;
            int ptr_depth;
            FuncPtrSig* func_sig;
            int struct_size;         // For TYPE_STRUCT/TYPE_UNION storage (bytes)
            int struct_align;        // For TYPE_STRUCT/TYPE_UNION storage (bytes)
            struct Node* expression; // Scalars: required for automatic locals, optional for static/global
            bool is_global;
            bool is_const;
            bool is_static;
        } var_decl;

        struct {
            char* name;
            DataType target_type;
            char* target_type_name;
            DataType target_ptr_base_type;
            char* target_ptr_base_type_name;
            int target_ptr_depth;
            FuncPtrSig* target_func_sig;
        } type_alias;

        struct {
            char* name;
            DataType element_type;
            char* element_type_name;
            DataType element_ptr_base_type;
            char* element_ptr_base_type_name;
            int element_ptr_depth;
            int element_struct_size;
            int element_struct_align;
            int* dims;
            int dim_count;
            int64_t total_elems;
            bool is_global;
            bool is_const;
            bool is_static;
            bool has_init;
            struct Node* init_values;
            int init_count;
        } array_decl;

        struct { char* name; struct Node* members; } enum_decl;
        struct { char* name; int64_t value; bool has_value; } enum_member;
        struct { char* name; struct Node* fields; } struct_decl;
        struct { char* name; struct Node* fields; } union_decl;

        struct {
            struct Node* base;
            char* member;
            // semantic-filled resolution fields exist in code; omitted here for brevity.
        } member_access;

        struct {
            struct Node* target; // NODE_MEMBER_ACCESS
            struct Node* value;
        } member_assign;

        struct {
            char* name;
            struct Node* indices;
            int index_count;
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
    // المستوى الأعلى
    NODE_PROGRAM,       // البرنامج: يحتوي على قائمة التصريحات
    NODE_FUNC_DEF,      // تعريف دالة
    NODE_VAR_DECL,      // تعريف متغير (عام أو محلي)
    NODE_TYPE_ALIAS,    // تعريف اسم نوع بديل: نوع معرف = ط٦٤.

    // تعريفات الأنواع المركبة (v0.3.4)
    NODE_ENUM_DECL,     // تعريف تعداد: تعداد لون { ... }
    NODE_STRUCT_DECL,   // تعريف هيكل: هيكل سيارة { ... }
    NODE_ENUM_MEMBER,   // عنصر تعداد داخل تعريفه
    NODE_UNION_DECL,    // تعريف اتحاد: اتحاد قيمة { ... }
    
    // الجمل البرمجية (Statements)
    NODE_BLOCK,         // نطاق { ... } وكتلة الدالة
    NODE_RETURN,        // جملة الإرجاع (إرجع)
    NODE_PRINT,         // جملة الطباعة (اطبع)
    NODE_IF,            // جملة الشرط (إذا)
    NODE_WHILE,         // جملة التكرار (طالما)
    NODE_FOR,           // جملة التكرار المحدد (لكل)
    NODE_SWITCH,        // جملة الاختيار (اختر)
    NODE_CASE,          // حالة الاختيار (حالة/افتراضي)
    NODE_BREAK,         // جملة التوقف (توقف)
    NODE_CONTINUE,      // جملة الاستمرار (استمر)
    NODE_ASSIGN,        // جملة التعيين (س = 5)
    NODE_CALL_STMT,     // استدعاء دالة كجملة
    NODE_READ,          // جملة الإدخال (اقرأ)
    NODE_INLINE_ASM,    // جملة تجميع مدمج: مجمع { ... }
    NODE_ASM_OPERAND,   // عنصر معامل تجميع مدمج (قيد + تعبير)
    
    // المصفوفات
    NODE_ARRAY_DECL,    // تعريف مصفوفة: صحيح س[5].
    NODE_ARRAY_ASSIGN,  // تعيين عنصر مصفوفة: س[0] = 1.
    NODE_ARRAY_ACCESS,  // قراءة عنصر مصفوفة: س[0]

    // الوصول للأعضاء/المؤهلات (v0.3.4)
    NODE_MEMBER_ACCESS, // <expr>:<member> (عضو هيكل أو قيمة تعداد)
    NODE_MEMBER_ASSIGN, // <member_access> = <expr>.
    NODE_DEREF_ASSIGN,  // *ptr = <expr>.
    
    // التعبيرات (Expressions)
    NODE_BIN_OP,        // العمليات الثنائية (+، -، *، /)
    NODE_UNARY_OP,      // العمليات الأحادية (!، -)
    NODE_POSTFIX_OP,    // العمليات اللاحقة (++، --)
    NODE_INT,           // قيمة عددية صحيحة
    NODE_FLOAT,         // قيمة عددية عشرية
    NODE_STRING,        // قيمة نصية
    NODE_CHAR,          // قيمة حرفية
    NODE_BOOL,          // قيمة منطقية (صواب/خطأ)
    NODE_NULL,          // مؤشر فارغ (عدم في سياق التعبير)
    NODE_CAST,          // تحويل صريح: كـ<نوع>(تعبير)
    NODE_SIZEOF,        // حجم(type) أو حجم(expr)
    NODE_VAR_REF,       // إشارة لمتغير
    NODE_CALL_EXPR      // تعبير استدعاء دالة
} NodeType;
```

---

*[← Compiler Internals](INTERNALS.md) | [IR Specification →](BAA_IR_SPECIFICATION.md)*
