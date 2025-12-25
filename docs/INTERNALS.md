# Baa Compiler Internals (v0.0.7)

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
4.  **Simplicity:** Favor readable code over premature optimization.

---

## 2. The Lexer (`src/lexer.c`)

The Lexer (or Scanner) converts raw bytes into meaningful `Token` structures.

### 2.1. UTF-8 Handling
Since Baa uses Arabic syntax, the Lexer must handle multi-byte characters.
*   **BOM detection:** It checks the first 3 bytes for `0xEF 0xBB 0xBF` and skips them.
*   **Arabic Digits:** Arabic-Indic digits (`٠`-`٩`) are 2 bytes in UTF-8 (`0xD9 0xA0` to `0xD9 0xA9`). The lexer normalizes these into ASCII strings (`"0"`-`"9"`) for the rest of the compiler.
*   **Keywords:** Keywords are detected by byte-comparison using `strncmp`.

### 2.2. Token Structure
Defined in `baa.h`:
```c
typedef struct {
    TokenType type; // Enum identifying the token category
    char* value;    // Null-terminated string (for IDs and Numbers)
    int line;       // Source line number
} Token;
```

---

## 3. The Parser (`src/parser.c`)

The Parser uses a **Recursive Descent** strategy. It builds the AST by predicting the grammatical structure based on the current token.

### 3.1. Grammar Rules (BNF-style)
```text
Program     -> Statement* EOF
Block       -> "{" Statement* "}"
Statement   -> ReturnStmt | PrintStmt | VarDecl | AssignStmt | IfStmt | WhileStmt
ReturnStmt  -> "إرجع" Expression "."
PrintStmt   -> "اطبع" Expression "."
VarDecl     -> "صحيح" Identifier "=" Expression "."
AssignStmt  -> Identifier "=" Expression "."
IfStmt      -> "إذا" "(" Expression ")" Statement
WhileStmt   -> "طالما" "(" Expression ")" Statement
Expression  -> Term { ("+" | "-" | "==" | "!=") Term }
Term        -> Integer | Identifier | "(" Expression ")"
```

---

## 4. The Abstract Syntax Tree (AST)

The AST is a tree of `Node` structs. It uses a **Tagged Union** pattern.

### 4.1. Node Types
| Node Type | Description | Data Field Used |
| :--- | :--- | :--- |
| `NODE_PROGRAM` | Root. | `data.program.statements` |
| `NODE_BLOCK` | Scope `{...}`. | `data.block.statements` |
| `NODE_VAR_DECL` | Declaration. | `data.var_decl` (`name`, `expression`) |
| `NODE_ASSIGN` | Assignment. | `data.assign_stmt` (`name`, `expression`) |
| `NODE_VAR_REF` | Usage. | `data.var_ref` (`name`) |
| `NODE_IF` | Logic. | `data.if_stmt` (`condition`, `then_branch`) |
| `NODE_WHILE` | Loop. | `data.while_stmt` (`condition`, `body`) |
| `NODE_BIN_OP` | Math. | `data.bin_op` (`left`, `right`, `op`) |
| `NODE_INT` | Literal. | `data.integer` (`value`) |
| `NODE_PRINT` | Output. | `data.print_stmt` (`expression`) |
| `NODE_RETURN` | Exit. | `data.return_stmt` (`expression`) |

---

## 5. Code Generation (`src/codegen.c`)

This module traverses the AST and emits x86_64 Assembly (AT&T Syntax).

### 5.1. The Stack Machine Model
*   **Accumulator:** `RAX` stores expression results.
*   **Scratch:** `RBX` is used as temporary storage.
*   **Stack:** Used for pushing operands during binary operations.

### 5.2. Memory Layout & Stack Frame
*   **Variable Storage:** Variables are stored at negative offsets from `RBP` (e.g., `[RBP - 8]`, `[RBP - 16]`).
*   **Shadow Space:** We reserve 32 bytes (`sub $160, %rsp`) to satisfy Windows x64 ABI requirements for calling functions like `printf`.

### 5.3. Control Flow Implementation

#### If Statement
1.  Generate Label ID `N`.
2.  Compare condition with False.
3.  `je .Lend_N` (Jump to end if False).
4.  Emit Body.
5.  Label `.Lend_N`.

#### While Loop
The loop uses **Two Labels**:
1.  **Start Label (`.Lstart_N`):** Emitted before the condition check.
2.  **End Label (`.Lend_N`):** Used to exit the loop.

**Logic Flow:**
1.  `.Lstart_N:`
2.  Evaluate Condition.
3.  `cmp $0, %rax` -> `je .Lend_N` (Exit if False).
4.  Emit Body.
5.  `jmp .Lstart_N` (Loop back).
6.  `.Lend_N:`

### 5.4. Symbol Table
Implemented as a global array `Symbol symbols[100]`. It maps string names to stack offsets. It handles both declarations (adding new entries) and assignments/references (looking up existing entries).

---

## 6. Build & Link Process (`src/main.c`)

1.  **IO:** Reads the source file into a buffer.
2.  **Compile:** Calls Lexer -> Parser -> Codegen.
3.  **Output:** Writes `out.s` to disk.
4.  **System Call:** Invokes `gcc out.s -o out.exe` to handle linking and PE header generation.
```