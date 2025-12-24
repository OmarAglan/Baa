# Baa Compiler Internals

This document describes the architecture of the Baa compiler implementation in C.

## 1. Pipeline Overview

1.  **Source Input:** Reads `.b` file (UTF-8).
2.  **Lexer (`lexer.c`):** Converts raw bytes into `Token` structs. Handles UTF-8 Arabic decoding.
3.  **Parser (`parser.c`):** Converts Tokens into an Abstract Syntax Tree (AST) of `Node` structs.
4.  **Codegen (`codegen.c`):** Traverses the AST and generates `out.s` (x86_64 Assembly).
5.  **Assembler/Linker:** Uses `gcc` to assemble `out.s` into `out.exe`.

## 2. Memory Model (Stack Machine)

Baa v0.0.4 uses the x86_64 System V ABI (adapted for Windows MinGW calls).

*   **Registers:**
    *   `RAX`: Primary accumulator. Stores results of expressions.
    *   `RBP`: Base Pointer (Start of Stack Frame).
    *   `RSP`: Stack Pointer (Current top of stack).
*   **Variable Storage:**
    *   Variables are stored on the stack relative to `RBP`.
    *   Example: First variable is at `[RBP - 8]`, second at `[RBP - 16]`.
*   **Shadow Space:**
    *   The compiler reserves 32 bytes of "Shadow Space" on the stack before calling Windows functions like `printf`.

## 3. C API Reference (`baa.h`)

### Data Structures

#### `Token`
Represents a lexical unit.
```c
typedef struct {
    TokenType type; // Enum (TOKEN_INT, TOKEN_PRINT, etc.)
    char* value;    // ASCII representation of value/name
    int line;       // Source line number for debugging
} Token;
```

#### `Node` (AST)
Represents a grammatical construct.
```c
typedef struct Node {
    NodeType type;      // NODE_VAR_DECL, NODE_BIN_OP, etc.
    struct Node* next;  // Next statement (Linked List)
    union {
        // Union of data specific to the Node Type
        // e.g., variable name, integer value, or child nodes
    } data;
} Node;
```

### Modules

#### Lexer
*   `void lexer_init(Lexer* l, const char* src)`: Prepares the lexer string stream.
*   `Token lexer_next_token(Lexer* l)`: Returns the next token. Handles BOM skipping and Arabic-to-ASCII digit normalization.

#### Parser
*   `Node* parse(Lexer* l)`: Consumes tokens and builds the AST root (Program Node).

#### Codegen
*   `void codegen(Node* node, FILE* file)`: Recursively emits assembly instructions to the file stream. Uses a simple Symbol Table array to map variable names to stack offsets.
```

---