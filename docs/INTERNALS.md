# Baa Compiler Internals (v0.0.6)

This document provides a comprehensive technical overview of the Baa compiler's architecture, data structures, and algorithms. It is intended for compiler developers.

---

## 1. System Architecture

The compiler follows a traditional **Single-Pass Frontend, Single-Pass Backend** architecture.

```mermaid
[Source Code (.b)] -> [Lexer] -> [Token Stream] -> [Parser] -> [Abstract Syntax Tree (AST)] -> [Code Generator] -> [Assembly (.s)] -> [GCC/LD] -> [Executable (.exe)]
```

### 1.1. Design Philosophy
1.  **No Dependencies:** Uses standard C11 libraries only.
2.  **Platform:** Windows x86_64 (MinGW Toolchain).
3.  **Encoding:** Strictly UTF-8. The compiler manually handles multi-byte Arabic sequences.
4.  **Simplicity:** Favor readable code over premature optimization (e.g., linear symbol tables).

---

## 2. The Lexer (`src/lexer.c`)

The Lexer (or Scanner) converts raw bytes into meaningful `Token` structures.

### 2.1. UTF-8 Handling
Since Baa uses Arabic syntax, the Lexer must handle multi-byte characters.
*   **BOM detection:** It checks the first 3 bytes for `0xEF 0xBB 0xBF` and skips them.
*   **Arabic Digits:** Arabic-Indic digits (`٠`-`٩`) are 2 bytes in UTF-8 (`0xD9 0xA0` to `0xD9 0xA9`). The lexer normalizes these into ASCII strings (`"0"`-`"9"`) for the rest of the compiler.
*   **Keywords:** Keywords are detected by byte-comparison using `strncmp`. Example: `إرجع` is matched against the byte sequence `0xD8 0xA5 0xD8 0xB1 0xD8 0xAC 0xD8 0xB9`.

### 2.2. Token Structure
Defined in `baa.h`:
```c
typedef struct {
    TokenType type; // Enum identifying the token category (TOKEN_INT, TOKEN_IF, etc.)
    char* value;    // Null-terminated string. Used for Identifiers and Number literals.
    int line;       // Source line number for error reporting.
} Token;
```

---

## 3. The Parser (`src/parser.c`)

The Parser uses a **Recursive Descent** strategy. It builds the AST by predicting the grammatical structure based on the current token.

### 3.1. Grammar Rules (BNF-style)
```text
Program     -> Statement* EOF
Block       -> "{" Statement* "}"
Statement   -> ReturnStmt | PrintStmt | VarDecl | IfStmt
ReturnStmt  -> "إرجع" Expression "."
PrintStmt   -> "اطبع" Expression "."
VarDecl     -> "صحيح" Identifier "=" Expression "."
IfStmt      -> "إذا" "(" Expression ")" Statement
Expression  -> Term { ("+" | "-" | "==" | "!=") Term }
Term        -> Integer | Identifier | "(" Expression ")"
```

### 3.2. Precedence Climbing
To handle math logic (like `1 + 2 - 3`), the parser uses a loop inside `parse_expression`.
1.  Parse the Left side (`parse_primary`).
2.  While the current token is an Operator:
    *   Create a Binary Operation Node.
    *   Set `left` to the previous node.
    *   Parse the Right side.
    *   Set the new node as `left` for the next iteration.

---

## 4. The Abstract Syntax Tree (AST)

The AST is a tree of `Node` structs. It uses a **Tagged Union** pattern to represent different node types in C.

### 4.1. Node Types
| Node Type | Description | Data Field Used |
| :--- | :--- | :--- |
| `NODE_PROGRAM` | Root of the file. | `data.program.statements` (Linked List) |
| `NODE_BLOCK` | A scope `{...}`. | `data.block.statements` (Linked List) |
| `NODE_VAR_DECL` | Variable definition. | `data.var_decl` (`name`, `expression`) |
| `NODE_VAR_REF` | Variable usage. | `data.var_ref` (`name`) |
| `NODE_IF` | Conditional logic. | `data.if_stmt` (`condition`, `then_branch`) |
| `NODE_BIN_OP` | Math/Logic (`+`, `==`). | `data.bin_op` (`left`, `right`, `op`) |
| `NODE_INT` | Integer literal. | `data.integer` (`value`) |
| `NODE_PRINT` | Output statement. | `data.print_stmt` (`expression`) |
| `NODE_RETURN` | Exit statement. | `data.return_stmt` (`expression`) |

---

## 5. Code Generation (`src/codegen.c`)

This module traverses the AST and emits x86_64 Assembly (AT&T Syntax).

### 5.1. The Stack Machine Model
*   **Accumulator:** `RAX` is used to store the result of the current operation.
*   **Scratch:** `RBX` is used as a temporary register during binary operations.
*   **Operation Flow:**
    1.  Generate code for the Right operand (Result in `RAX`).
    2.  Push `RAX` to the stack.
    3.  Generate code for the Left operand (Result in `RAX`).
    4.  Pop stack into `RBX`.
    5.  Perform `OP RBX, RAX` (Result stays in `RAX`).

### 5.2. Memory Layout & Stack Frame
We follow the Windows x64 calling convention.

*   **RBP (Base Pointer):** Anchors the stack frame.
*   **RSP (Stack Pointer):** Moves as we push/pop data.
*   **Variable Storage:** Variables are stored at negative offsets from `RBP`.
    *   Variable 1: `[RBP - 8]`
    *   Variable 2: `[RBP - 16]`
*   **Shadow Space:** We reserve 32 bytes (`sub $160, %rsp`) at the start of `main`. This covers local variables (up to 16 `int64`s) and the 32-byte Shadow Space required by Windows functions like `printf`.

### 5.3. Control Flow Implementation
The `if` statement is implemented using **Label Generation**.
1.  **Generate ID:** A static counter creates a unique ID (e.g., `5`).
2.  **Compare:** `cmp $0, %rax` (Compare condition result to False).
3.  **Jump:** `je .Lend_5` (Jump if Equal/False).
4.  **Body:** Emit assembly for the block.
5.  **Label:** `.Lend_5:` marks the jump target.

### 5.4. Symbol Table
Currently implemented as a global array:
```c
typedef struct { char name[32]; int offset; } Symbol;
Symbol symbols[100];
```
*   **Lookup:** Linear search (`O(n)`).
*   **Scope:** Currently global scope only (flat namespace), though the parser supports blocks.
*   **Resolution:** Maps a string name to a stack offset integer.

---

## 6. Build & Link Process (`src/main.c`)

1.  **IO:** Reads the source file into a buffer.
2.  **Compile:** Calls Lexer -> Parser -> Codegen.
3.  **Output:** Writes `out.s` to disk.
4.  **System Call:** Invokes `gcc out.s -o out.exe`.
    *   GCC handles linking the C Runtime (CRT) for `printf`.
    *   GCC handles the PE (Portable Executable) headers for Windows.
```
