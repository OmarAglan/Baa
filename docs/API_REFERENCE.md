# Baa Internal API Reference

> **Version:** 0.2.7 | [← User Guide](USER_GUIDE.md) | [Internals →](INTERNALS.md)

This document details the C functions, enumerations, and structures defined in `src/baa.h`.

---

## Table of Contents

- [Lexer Module](#1-lexer-module)
- [Parser Module](#2-parser-module)
- [Semantic Analysis](#3-semantic-analysis)
- [Codegen Module](#4-codegen-module)
- [Error Reporting](#5-error-reporting)
- [Symbol Table](#6-symbol-table)
- [Data Structures](#7-data-structures)

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
+- Detects and skips UTF-8 BOM (Byte Order Mark: `0xEF 0xBB 0xBF`) if present.
- Initializes line counter to 1.
- **Initializes the macro table** for the preprocessor.

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

## 4. Codegen Module

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

## 5. Error Reporting

Centralized diagnostic system for compiler errors.

### `error_init`

```c
void error_init(const char* source)
```

Initializes the error reporting system with the source code for context display.

### `error_report`

```c
void error_report(Token token, const char* message, ...)
```

Reports an error with source location, line context, and a pointer to the error position.

**Features:**
- Displays filename, line, and column
- Shows the actual source line with a `^` pointer
- Supports printf-style formatting

---

## 6. Symbol Table

Manages variable scope and resolution.

### Symbol Structure

Moved to `baa.h` for shared use between Analysis and Codegen.

```c
typedef struct {
    char name[32];     // Variable name
    ScopeType scope;   // SCOPE_GLOBAL or SCOPE_LOCAL
    DataType type;     // TYPE_INT or TYPE_STRING
    int offset;        // Stack offset or memory address
    bool is_const;     // NEW in v0.2.7: Immutability flag
} Symbol;
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | `char[32]` | Variable identifier (Arabic UTF-8) |
| `scope` | `ScopeType` | `SCOPE_GLOBAL` or `SCOPE_LOCAL` |
| `type` | `DataType` | `TYPE_INT` or `TYPE_STRING` |
| `offset` | `int` | Stack offset (local) or memory address (global) |
| `is_const` | `bool` | `true` if declared with `ثابت` (v0.2.7+) |

> **Note**: Symbol table management functions (`add_local`, `lookup`, etc.) are implemented as static helper functions within `analysis.c` and `codegen.c` independently. In v0.2.4+, the `Symbol` struct definition is shared via `baa.h` to enable semantic analysis before code generation.

---

## 7. Data Structures

### 7.1. Preprocessor Structures

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

### 7.2. Token

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
### 7.3. Token Types

### BaaTokenType Enum

```c
typedef enum {
    // End of file
    TOKEN_EOF,
    
    // Literals
    TOKEN_INT,          // 123, ١٢٣
    TOKEN_STRING,       // "مرحباً"
    TOKEN_CHAR,         // 'أ'
    TOKEN_ID,           // متغير
    
    // Keywords
    TOKEN_TYPE_INT,     // صحيح
    TOKEN_TYPE_STRING,  // نص
    TOKEN_CONST,        // ثابت (NEW in v0.2.7)
    TOKEN_IF,           // إذا
    TOKEN_ELSE,         // وإلا
    TOKEN_WHILE,        // طالما
    TOKEN_FOR,          // لكل
    TOKEN_SWITCH,       // اختر
    TOKEN_CASE,         // حالة
    TOKEN_DEFAULT,      // افتراضي
    TOKEN_PRINT,        // اطبع
    TOKEN_RETURN,       // إرجع
    TOKEN_BREAK,        // توقف
    TOKEN_CONTINUE,     // استمر
    
    // Operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_MOD,
    TOKEN_INC, TOKEN_DEC,           // ++, --
    TOKEN_EQ, TOKEN_NE,             // ==, !=
    TOKEN_LT, TOKEN_GT, TOKEN_LTE, TOKEN_GTE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, // &&, ||, !
    
    // Delimiters
    TOKEN_LPAREN, TOKEN_RPAREN,     // ( )
    TOKEN_LBRACE, TOKEN_RBRACE,     // { }
    TOKEN_LBRACKET, TOKEN_RBRACKET, // [ ]
    TOKEN_DOT, TOKEN_COMMA, TOKEN_COLON,
    TOKEN_SEMICOLON,                // ؛
    TOKEN_ASSIGN,                   // =
    
    TOKEN_INVALID
} TokenType;
```

---
### 7.4. AST Node Structure

### Node (AST)

The AST is a tagged union representing grammatical structure.

```c
typedef struct Node {
    NodeType type;      // Discriminator
    struct Node* next;  // Linked list pointer
    
    union {
        // Program (root node)
        struct { struct Node* declarations; } program;
        
        // Function definition
        struct {
            char* name;
            DataType return_type;
            struct Node* params;
            struct Node* body;
            bool is_prototype;  // NEW in v0.2.5
        } func;
        
        // Variable declaration
        struct {
            char* name;
            DataType type;
            struct Node* expression;
            bool is_global;
            bool is_const;      // NEW in v0.2.7
        } var_decl;
        
        // Array declaration
        struct {
            char* name;
            int size;
            bool is_global;
            bool is_const;      // NEW in v0.2.7
        } array_decl;
        
        // Array access/assign
        struct {
            char* name;
            struct Node* index;
            struct Node* value;  // NULL for read
        } array_op;
        
        // For loop
        struct {
            struct Node* init;
            struct Node* condition;
            struct Node* increment;
            struct Node* body;
        } for_stmt;
        
        // Switch statement (NEW in v0.1.3)
        struct {
            struct Node* expression;
            struct Node* cases;
        } switch_stmt;
        
        // Case statement
        struct {
            struct Node* value;
            struct Node* body;
            bool is_default;
        } case_stmt;
        
        // Binary operation
        struct {
            struct Node* left;
            struct Node* right;
            OpType op;
        } bin_op;
        
        // Unary/Postfix operation
        struct {
            struct Node* operand;
            UnaryOpType op;
        } unary_op;
        
        // Literals
        struct { int value; } int_lit;
        struct { char* value; } string_lit;
        struct { int value; } char_lit;
        
        // Variable reference
        struct { char* name; } var_ref;
        
        // ... additional fields
    } data;
} Node;
```

---
### 7.5. Node Types

### NodeType Enum

```c
typedef enum {
    // Top-level
    NODE_PROGRAM,
    NODE_FUNC_DEF,
    NODE_PARAM,
    
    // Blocks
    NODE_BLOCK,
    
    // Variables
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_VAR_REF,
    
    // Arrays
    NODE_ARRAY_DECL,
    NODE_ARRAY_ACCESS,
    NODE_ARRAY_ASSIGN,
    
    // Control flow
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_SWITCH,      // NEW in v0.1.3
    NODE_CASE,        // NEW in v0.1.3
    NODE_BREAK,       // NEW in v0.1.2
    NODE_CONTINUE,    // NEW in v0.1.2
    NODE_PRINT,
    
    // Expressions
    NODE_BIN_OP,
    NODE_UNARY_OP,
    NODE_POSTFIX_OP,
    NODE_CALL_EXPR,
    NODE_CALL_STMT,
    
    // Literals
    NODE_INT,
    NODE_STRING,
    NODE_CHAR
} NodeType;
```

---

*[← User Guide](USER_GUIDE.md) | [Internals →](INTERNALS.md)*