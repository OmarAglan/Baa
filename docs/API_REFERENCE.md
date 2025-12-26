# Baa Internal API Reference (v0.0.9)

This document details the C functions, enumerations, and structures defined in `src/baa.h`. It reflects the architecture changes for supporting Functions, Global Scope, and the Windows x64 ABI.

---

## 1. Lexer Module
Handles string processing, UTF-8 decoding, and tokenization.

### `void lexer_init(Lexer* l, const char* src)`
Initializes a new Lexer instance.
*   **Parameters:**
    *   `l`: Pointer to a `Lexer` struct.
    *   `src`: Null-terminated UTF-8 string containing the source code.
*   **Behavior:** 
    *   Sets up position pointers.
    *   Detects and skips UTF-8 BOM (`0xEF 0xBB 0xBF`) if present.

### `Token lexer_next_token(Lexer* l)`
Consumes the input stream and returns the next valid token.
*   **Returns:** A `Token` struct. If end of file, returns `TOKEN_EOF`.
*   **Behavior:**
    *   Skips whitespace and comments (`//`).
    *   Identifies Keywords (`صحيح`, `إذا`, `طالما`, `اطبع`, `إرجع`).
    *   Normalizes Arabic-Indic digits (`٠`-`٩`) to ASCII (`0`-`9`) in the token value.
    *   Allocates memory for `token.value` (Identifiers/Numbers).

---

## 2. Parser Module
Handles syntactic analysis, lookahead, and AST construction.

### `Node* parse(Lexer* l)`
The entry point for the parsing phase.
*   **Parameters:** `l`: Pointer to an initialized Lexer.
*   **Returns:** Pointer to the root `Node` (Type `NODE_PROGRAM`).
*   **Behavior:**
    *   Initializes internal `Parser` state with 1-token Lookahead.
    *   Parses a list of **Declarations** (Global Variables or Functions).
    *   Returns the root node containing the linked list of declarations.
    *   Exits program on syntax error.

---

## 3. Codegen Module
Handles x86_64 Assembly generation and ABI compliance.

### `void codegen(Node* node, FILE* file)`
Recursively generates assembly code.
*   **Parameters:**
    *   `node`: The AST node to process (starting with Root).
    *   `file`: An open file handle (write mode) to `out.s`.
*   **Behavior:**
    *   **Pass 1 (Globals):** Generates `.data` section for `NODE_VAR_DECL` found at root level.
    *   **Pass 2 (Text):** Generates `.text` section.
    *   **Functions:** Generates Prologue (stack setup) and Epilogue (stack teardown) for `NODE_FUNC_DEF`.
    *   **Entry Point:** Detects `الرئيسية` and exports it as `main`.

---

## 4. Symbol Table Module (New)
Manages scope and variable resolution.

### `void symbol_table_enter_scope()`
Prepares the symbol table for a new function scope (resets local offsets).

### `void symbol_table_add(const char* name, ScopeType scope)`
Registers a variable or parameter.
*   **Global:** Allocates label in `.data`.
*   **Local:** Allocates offset in Stack Frame (decrements `rbp` offset).

### `Symbol* symbol_table_lookup(const char* name)`
Searches for a symbol.
1.  Checks Local Scope (Stack).
2.  Checks Global Scope (Data Section).
3.  Returns `NULL` if undefined.

---

## 5. Data Structures

### `struct Token`
Represents a single atomic unit of code.
```c
typedef struct {
    TokenType type; // Discriminator (TOKEN_IF, TOKEN_INT, etc.)
    char* value;    // Payload (Name for ID, ASCII Digits for INT)
    int line;       // Source line number
} Token;
```

### `struct Node` (AST)
The AST is a tagged union representing the grammatical structure.

#### Core Enum `NodeType`
*   `NODE_PROGRAM`: Root.
*   `NODE_FUNC_DEF`: Function Definition.
*   `NODE_BLOCK`: Code Block `{...}`.
*   `NODE_VAR_DECL`: Variable Definition.
*   `NODE_IF` / `NODE_WHILE`: Control Flow.
*   `NODE_CALL`: Function Call.
*   `NODE_RETURN` / `NODE_PRINT`: Statements.
*   `NODE_BIN_OP` / `NODE_INT` / `NODE_VAR_REF`: Expressions.
*   `NODE_STRING` / `NODE_CHAR`: Literals.

#### The `Node` Struct
```c
typedef struct Node {
    NodeType type;
    struct Node* next;  // Linked List (Next Statement or Declaration)
    
    union {
        // Root / Block
        struct { struct Node* statements; } block; // or program
        
        // Function Definition
        struct { 
            char* name; 
            struct Node* params; // Linked list of NODE_PARAM
            struct Node* body;   // NODE_BLOCK
            int stack_size;      // Calculated during codegen
        } func_def;

        // Function Call
        struct {
            char* name;
            struct Node* args;   // Linked list of expressions
        } call_expr;

        // Variable Declaration
        struct { 
            char* name; 
            struct Node* expression; 
            bool is_global;      // Flag for storage type
        } var_decl;

        // Control Flow
        struct { struct Node* condition; struct Node* body; } while_stmt;
        struct { struct Node* condition; struct Node* then_branch; } if_stmt;

        // Math
        struct { struct Node* left; struct Node* right; OpType op; } bin_op;
        
        // Leaf
        struct { int value; } integer;
        struct { char* value; int id; } string_lit;
        struct { int value; } char_lit;
        struct { char* name; } var_ref;
        struct { char* name; } param; // For parameter definition
    } data;
} Node;
```

### `struct Symbol`
Used for variable resolution.
```c
typedef enum { SCOPE_GLOBAL, SCOPE_LOCAL } ScopeType;

typedef struct {
    char name[64];
    ScopeType scope;
    int offset; // Locals: -8, -16... | Globals: 0 (Label based)
} Symbol;
```