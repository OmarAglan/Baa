# Baa Internal API Reference

> **Version:** 0.1.1 | [← Internals](INTERNALS.md)

This document details the C functions, enumerations, and structures defined in `src/baa.h`.

---

## Table of Contents

- [Lexer Module](#1-lexer-module)
- [Parser Module](#2-parser-module)
- [Semantic Analysis](#3-semantic-analysis)
- [Codegen Module](#4-codegen-module)
- [Symbol Table](#5-symbol-table)
- [Data Structures](#6-data-structures)

---

## 1. Lexer Module

Handles UTF-8 string processing and tokenization.

### `lexer_init`

```c
void lexer_init(Lexer* l, const char* src)
```

Initializes a new Lexer instance.

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Pointer to Lexer struct to initialize |
| `src` | `const char*` | UTF-8 source code string |

**Behavior:**
- Sets up position pointers to start of source
- Detects and skips UTF-8 BOM (`0xEF 0xBB 0xBF`) if present
- Initializes line counter to 1

---

### `lexer_next_token`

```c
Token lexer_next_token(Lexer* l)
```

Consumes input and returns the next valid token.

| Parameter | Type | Description |
|-----------|------|-------------|
| `l` | `Lexer*` | Initialized Lexer |

**Returns:** `Token` struct. Returns `TOKEN_EOF` at end of file.

**Behavior:**
- Skips whitespace and comments (`//`)
- Identifies keywords: `صحيح`, `إذا`, `طالما`, `لكل`, `اطبع`, `إرجع`
- Handles Arabic semicolon `؛` (UTF-8 `0xD8 0x9B`)
- Handles multi-char operators: `++`, `--`, `&&`, `||`, `<=`, `>=`, `==`, `!=`
- Normalizes Arabic-Indic digits (`٠`-`٩`) to ASCII (`0`-`9`)
- Allocates memory for `token.value` (caller must free)

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
- Initializes internal `Parser` state with 1-token lookahead
- Parses list of declarations (global variables or functions)
- Implements operator precedence climbing for expressions
- Builds linked-list AST structure

**Example:**
```c
Lexer lexer;
lexer_init(&lexer, source_code);
Node* ast = parse(&lexer);
// Use ast...
```

---

## 3. Semantic Analysis

Handles type checking and logic validation.

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
- Traverses the entire tree.
- Reports errors using `error_report`.
- Does not modify the AST, only validates it.

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

## 5. Symbol Table

Manages variable scope and resolution.

### Symbol Structure

Moved to `baa.h` for shared use between Analysis and Codegen.

```c
typedef struct { 
    char name[32];     // Variable name
    ScopeType scope;   // SCOPE_GLOBAL or SCOPE_LOCAL
    DataType type;     // TYPE_INT or TYPE_STRING
    int offset;        // Stack offset or memory address
} Symbol;
```

### `add_local`

```c
void add_local(const char* name, int size)
```

Registers a local variable, parameter, or array.

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const char*` | Variable identifier |
| `size` | `int` | Slots to reserve (1 for scalar, N for array[N]) |

**Behavior:**
- Decrements `current_stack_offset` by `size × 8`
- Records base address in symbol table

---

### `lookup_symbol`

```c
Symbol* lookup_symbol(const char* name)
```

Searches for a symbol by name.

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const char*` | Symbol name to find |

**Returns:** Pointer to `Symbol` if found, `NULL` otherwise.

**Search Order:**
1. Local scope
2. Global scope

---

## 6. Data Structures

### Token

Represents a single atomic unit of source code.

```c
typedef struct {
    TokenType type;  // Discriminator (TOKEN_FOR, TOKEN_INT, etc.)
    char* value;     // Payload (name for ID, digits for INT)
    int line;        // Source line number (for errors)
} Token;
```

### TokenType Enum

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
    TOKEN_IF,           // إذا
    TOKEN_WHILE,        // طالما
    TOKEN_FOR,          // لكل
    TOKEN_PRINT,        // اطبع
    TOKEN_RETURN,       // إرجع
    
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
    TOKEN_DOT, TOKEN_COMMA,
    TOKEN_SEMICOLON,                // ؛
    
    TOKEN_INVALID
} TokenType;
```

---

### Node (AST)

The AST is a tagged union representing grammatical structure.

```c
typedef struct Node {
    NodeType type;      // Discriminator
    struct Node* next;  // Linked list pointer
    
    union {
        // Program
        struct { struct Node* declarations; } program;
        
        // Function
        struct {
            char* name;
            struct Node* params;
            struct Node* body;
        } func;
        
        // Variable declaration
        struct {
            char* name;
            struct Node* expression;
            bool is_global;
        } var_decl;
        
        // Array declaration
        struct {
            char* name;
            int size;
            bool is_global;
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

*[← Internals](INTERNALS.md)*