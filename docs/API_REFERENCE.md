# Baa Internal API Reference

> **Version:** 0.3.0 | [← User Guide](USER_GUIDE.md) | [Internals →](INTERNALS.md)

This document details the C functions, enumerations, and structures defined in `src/baa.h` and `src/ir.h`.

---

## Table of Contents

- [Lexer Module](#1-lexer-module)
- [Parser Module](#2-parser-module)
- [Semantic Analysis](#3-semantic-analysis)
- [IR Module](#4-ir-module)
- [Codegen Module](#5-codegen-module)
- [Diagnostic System](#6-diagnostic-system)
- [Symbol Table](#7-symbol-table)
- [Updater](#8-updater)
- [Data Structures](#9-data-structures)

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

## 5. Codegen Module

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

## 6. Diagnostic System

Centralized diagnostic system for compiler errors and warnings (v0.2.8+).

### 5.1. Error Reporting

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

### 5.2. Warning System (v0.2.8+)

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

### 5.3. Warning Types

```c
typedef enum {
    WARN_UNUSED_VARIABLE,    // Variable declared but never used
    WARN_DEAD_CODE,          // Unreachable code after return/break
    WARN_IMPLICIT_RETURN,    // Function without explicit return (future)
    WARN_SHADOW_VARIABLE,    // Local variable shadows global
    WARN_COUNT               // Total count (for iteration)
} WarningType;
```

### 5.4. Warning Configuration

```c
typedef struct {
    bool enabled[WARN_COUNT];   // Enable/disable each warning type
    bool warnings_as_errors;    // -Werror: treat warnings as errors
    bool all_warnings;          // -Wall: enable all warnings
    bool colored_output;        // ANSI colors in output
} WarningConfig;

extern WarningConfig g_warning_config;
```

### 5.5. State Query Functions

| Function | Return | Description |
|----------|--------|-------------|
| `error_has_occurred()` | `bool` | Returns `true` if any error was reported |
| `warning_has_occurred()` | `bool` | Returns `true` if any warning was reported |
| `warning_get_count()` | `int` | Returns total number of warnings |
| `error_reset()` | `void` | Resets error state |
| `warning_reset()` | `void` | Resets warning state and counter |

---

## 7. Symbol Table

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

## 8. Updater

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

## 9. Data Structures

### 8.1. Preprocessor Structures

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

### 8.2. Token

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
### 8.3. Token Types

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
### 8.4. AST Node Structure

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
### 8.5. Node Types

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
