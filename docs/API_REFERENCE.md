# Baa Internal API Reference

This document details the C functions and structures defined in `src/baa.h`.

## 1. Lexer Module
Handles string processing and tokenization.

### `void lexer_init(Lexer* l, const char* src)`
Initializes a new Lexer instance.
*   **Parameters:**
    *   `l`: Pointer to a `Lexer` struct.
    *   `src`: Null-terminated string containing the source code.
*   **Behavior:** Sets up pointers and detects/skips UTF-8 BOM if present.

### `Token lexer_next_token(Lexer* l)`
Consumes the input stream and returns the next valid token.
*   **Returns:** A `Token` struct. If end of file, returns token type `TOKEN_EOF`.
*   **Behavior:**
    *   Skips whitespace and comments.
    *   Allocates memory for `token.value` (caller must free, though currently managed by OS cleanup).
    *   Handles Arabic-to-ASCII digit conversion.
    *   Exits program on invalid character.

---

## 2. Parser Module
Handles syntactic analysis and AST construction.

### `Node* parse(Lexer* l)`
Parses the entire token stream.
*   **Parameters:** `l`: Pointer to an initialized Lexer.
*   **Returns:** Pointer to the root `Node` (Type `NODE_PROGRAM`).
*   **Behavior:**
    *   Iterates until `TOKEN_EOF`.
    *   Allocates memory for Nodes using `malloc`.
    *   Exits program on syntax error.

---

## 3. Codegen Module
Handles Assembly generation.

### `void codegen(Node* node, FILE* file)`
Recursively generates assembly code.
*   **Parameters:**
    *   `node`: The AST node to process.
    *   `file`: An open file handle (write mode) to `out.s`.
*   **Behavior:**
    *   Traverses the AST.
    *   Manages the Symbol Table state.
    *   Writes formatted assembly strings to the file.

---

## 4. Data Structures

### `struct Lexer`
Maintains the state of the text scanning.
```c
typedef struct {
    const char* src; // Pointer to source string
    size_t pos;      // Current index
    size_t len;      // Total length
    int line;        // Current line number
} Lexer;
```

### `struct Token`
Represents a single atomic unit of code.
```c
typedef struct {
    TokenType type; // Discriminator enum
    char* value;    // String payload (for IDs and Numbers)
    int line;       // Debug info
} Token;
```

### `struct Node` (AST)
Represents a grammatical construct.
```c
typedef struct Node {
    NodeType type;      // Discriminator enum
    struct Node* next;  // Linked List pointer for statement sequences
    union {             // Polymorphic data based on 'type'
        // ... specific structs for If, VarDecl, etc.
    } data;
} Node;
```