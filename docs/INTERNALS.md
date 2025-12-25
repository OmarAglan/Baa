# Baa Compiler Internals (v0.0.9 Specification)

**Version:** 0.0.9 (Functions & Scoping)
**Target Architecture:** x86-64 (AMD64)
**Target OS:** Windows (MinGW-w64 Toolchain)
**Calling Convention:** Microsoft x64 ABI

This document details the internal architecture, data structures, memory models, and algorithms used in the Baa compiler.

---

## 1. Pipeline Architecture

The compiler operates as a multi-stage pipeline. Unlike previous versions, v0.0.8 introduces a **Semantic Analysis** phase (integrated into parsing/codegen) to handle scoping and symbol resolution.

```mermaid
graph TD
    A[Source File (.b)] -->|UTF-8 Stream| B(Lexer)
    B -->|Token Stream| C(Parser)
    C -->|Abstract Syntax Tree| D{Semantic Check}
    D -->|Symbol Tables| E(Code Generator)
    E -->|x86_64 Assembly| F(Output .s)
    F -->|GCC/AS| G(Object File .o)
    G -->|GCC/LD| H(Executable .exe)
```

---

## 2. Lexical Analysis (Lexer)

The Lexer (`src/lexer.c`) transforms raw bytes into `Token` structures.

### 2.1. State Machine
*   **Input:** `const char* src` (UTF-8 encoded string).
*   **BOM Handling:** Detects `0xEF 0xBB 0xBF` at index 0 and advances position by 3.
*   **Arabic Normalization:** 
    *   Arabic-Indic digits (`٠`-`٩`) are detected via UTF-8 sequences (`0xD9 0xA0` - `0xD9 0xA9`).
    *   They are immediately converted to ASCII `0`-`9` in the `Token.value` field to simplify the Parser and `atoi()` calls.

### 2.2. Token Structure
```c
typedef struct {
    TokenType type; // Discriminator (INT, IF, IDENTIFIER, etc.)
    char* value;    // Payload:
                    // - Identifiers: The name (UTF-8)
                    // - Numbers: ASCII representation
                    // - Keywords/Symbols: NULL
    int line;       // Source line number for error reporting
} Token;
```

---

## 3. Syntactic Analysis (Parser)

The Parser (`src/parser.c`) builds the AST. v0.0.8 introduces **Lookahead** to resolve the C-style "Type Name" ambiguity.

### 3.1. The Ambiguity Problem
In C (and Baa), `صحيح س` could be the start of:
1.  `صحيح س = ٥.` (Global Variable)
2.  `صحيح س() { ... }` (Function Definition)

### 3.2. Lookahead Solution
The Parser struct maintains a buffer of tokens to peek ahead without consuming.

```c
typedef struct {
    Lexer* lexer;
    Token current; // The token we are processing
    Token next;    // The lookahead token
} Parser;
```

**Algorithm:**
1.  Match Type (`صحيح`).
2.  Match Identifier (`Name`).
3.  **Peek** `parser->next.type`:
    *   If `TOKEN_LPAREN` `(` $\to$ Parse **Function**.
    *   If `TOKEN_ASSIGN` `=` or `TOKEN_DOT` `.` $\to$ Parse **Variable**.

### 3.3. Formal Grammar (BNF)
```text
Program       ::= Declaration* EOF
Declaration   ::= FuncDecl | GlobalVarDecl
GlobalVarDecl ::= "صحيح" ID ("=" Expr)? "."
FuncDecl      ::= "صحيح" ID "(" ParamList ")" Block
ParamList     ::= ε | Param ("," Param)*
Param         ::= "صحيح" ID
Block         ::= "{" Statement* "}"
Statement     ::= Return | Print | LocalVarDecl | Assign | If | While | CallStmt
LocalVarDecl  ::= "صحيح" ID "=" Expr "."
Assign        ::= ID "=" Expr "."
CallStmt      ::= CallExpr "."
Expr          ::= Term { Op Term }
Term          ::= INT | ID | CallExpr | "(" Expr ")" | Unary
CallExpr      ::= ID "(" ArgList ")"
ArgList       ::= ε | Expr ("," Expr)*
```

---

## 4. The Abstract Syntax Tree (AST)

The AST is a polymorphic tree structure defined in `src/baa.h`.

### 4.1. Node Data Structures
The `Node` struct uses a tagged union. Below are the configurations for v0.0.8:

| Node Type | Description | Union Data (`data.*`) |
| :--- | :--- | :--- |
| **`NODE_PROGRAM`** | Root. | `struct Node* declarations;` (Linked List) |
| **`NODE_FUNC_DEF`** | Function Definition. | `char* name;`<br>`struct Node* params;`<br>`struct Node* body;`<br>`int stack_size;` (Calc during codegen) |
| **`NODE_PARAM`** | Parameter. | `char* name;` |
| **`NODE_BLOCK`** | Scope. | `struct Node* statements;` |
| **`NODE_VAR_DECL`** | Var Definition. | `char* name;`<br>`struct Node* expression;`<br>`bool is_global;` |
| **`NODE_CALL`** | Function Call. | `char* name;`<br>`struct Node* args;` |
| **`NODE_RETURN`** | Return Statement. | `struct Node* expression;` |
| **`NODE_BIN_OP`** | Math/Logic. | `left`, `right`, `op` (ADD, SUB, MUL, DIV, MOD, EQ, NEQ, LT, GT, LTE, GTE). |

---

## 5. Semantic Analysis (Symbol Tables)

To handle Scope (Global vs Local), we implement a **Hierarchical Symbol Table**.

### 5.1. Structures
```c
typedef enum { SCOPE_GLOBAL, SCOPE_LOCAL } ScopeType;

typedef struct {
    char name[64];
    ScopeType scope;
    int offset; // For Locals: Offset from RBP (e.g., -8)
                // For Globals: 0 (Addressed by label)
} Symbol;
```

### 5.2. Resolution Logic
The compiler maintains two lists: `GlobalSymbols` and `LocalSymbols`.

1.  **On Function Entry:** Clear `LocalSymbols`. Reset `CurrentStackOffset`.
2.  **On Parameter Parse:** Add param to `LocalSymbols`. Assign stack offset (`-8`, `-16`...).
3.  **On Variable Decl:** Add to `LocalSymbols` (or `Global` if outside function).
4.  **On Variable Usage (Look up):**
    1.  Search `LocalSymbols`. If found $\to$ Use `N(%rbp)`.
    2.  Search `GlobalSymbols`. If found $\to$ Use `Name(%rip)`.
    3.  If not found $\to$ **Compiler Error**.

---

## 6. Code Generation (Windows x64 ABI)

This is the strict contract required to run on Windows 64-bit architecture.

### 6.1. Register Usage
*   **`RAX`**: Accumulator / Return Value.
*   **`RCX`**, **`RDX`**, **`R8`**, **`R9`**: First 4 integer arguments.
*   **`RBP`**: Base Pointer (Frame Pointer).
*   **`RSP`**: Stack Pointer.
*   **`RBX`**, **`R12-R15`**: Callee-saved (Must preserve if used).
*   **`R10`**, **`R11`**: Volatile scratch registers.

### 6.2. Stack Frame Layout
Every function creates a stack frame. The stack grows **downwards** (towards lower addresses).

| Memory Address | Content | Who Allocates? |
| :--- | :--- | :--- |
| `RBP + 16 + (N*8)` | **Argument N** (5, 6...) | Caller |
| ... | ... | Caller |
| `RBP + 48` | **Argument 5** | Caller |
| `RBP + 40` | Shadow Space (Arg 4 home) | Caller |
| `RBP + 32` | Shadow Space (Arg 3 home) | Caller |
| `RBP + 24` | Shadow Space (Arg 2 home) | Caller |
| `RBP + 16` | Shadow Space (Arg 1 home) | Caller |
| `RBP + 8` | **Return Address** | CPU (`call`) |
| `RBP` | **Old RBP** | Callee (`push rbp`) |
| `RBP - 8` | **Spilled Param 1** (RCX) | Callee |
| `RBP - 16` | **Spilled Param 2** (RDX) | Callee |
| `RBP - 24` | **Local Var 1** | Callee |
| ... | ... | ... |

### 6.3. Function Prologue (Standard)
Generated at the start of every function.

```asm
push %rbp           ; Save caller's frame
mov %rsp, %rbp      ; Set new frame pointer
sub $X, %rsp        ; Allocate X bytes. X must be 16-byte aligned.
```

**Calculating X:**
`X = (NumLocals * 8) + (NumParams * 8) + 32 (Shadow Space for calls inside)`.
Must round `X` up so `(X + 8)` is divisible by 16 (to account for the pushed RBP).

### 6.4. Parameter Spilling
Because arguments arrive in registers (`RCX`, `RDX`...), but we treat them as variables that can be modified, the Prologue immediately dumps them onto the stack.

```asm
; Assuming function(a, b)
mov %rcx, -8(%rbp)  ; Spill Arg 1 (a)
mov %rdx, -16(%rbp) ; Spill Arg 2 (b)
```
Now, `a` and `b` are just local variables at `-8` and `-16`.

### 6.5. Function Call Sequence (The Caller)
When generating a `NODE_CALL`:

1.  **Stack Alignment:** Ensure `%rsp` is 16-byte aligned before `call`.
2.  **Arguments > 4:** Push arguments 5, 6, etc., onto the stack in **Reverse Order**.
3.  **Arguments 1-4:** Move values into `%rcx`, `%rdx`, `%r8`, `%r9`.
4.  **Shadow Space:** Ensure 32 bytes are reserved at `(%rsp)` (GCC usually handles this via `sub` in prologue, or we `sub $32, %rsp` here).
5.  **Call:** `call FunctionName`.
6.  **Cleanup:** If we pushed args (5+), `add $N, %rsp` to clear them. Result is in `%rax`.

### 6.6. Arithmetic & Logic (New in v0.0.9)
 
**Multiplication (`*`):** Uses `imul %rbx, %rax`.

**Division (`/`) & Modulo (`%`):**
x86-64 Division is complex. It divides the 128-bit value in `RDX:RAX` by the operand.
1.  Ensure `RAX` holds the dividend.
2.  Execute `cqo` (Convert Quad to Oct) to sign-extend `RAX` into `RDX`.
3.  Execute `idiv %rbx`.
    *   Quotient (`/`) is stored in `RAX`.
    *   Remainder (`%`) is stored in `RDX`.

**Comparisons (`<`, `>`, etc.):**
1.  `cmp %rbx, %rax`
2.  Use conditional set instructions: `sete` (==), `setne` (!=), `setl` (<), `setg` (>), `setle` (<=), `setge` (>=).
3.  `movzbq %al, %rax` (Zero extend boolean byte to integer).

---

## 7. Global Data Section

Global variables are declared in the assembly `.data` section.

```asm
.section .data
global_var_1: .quad 0
```

*   **Reading:** `mov global_var_1(%rip), %rax`
*   **Writing:** `mov %rax, global_var_1(%rip)`

---

## 8. Naming Mangling & Entry Point

### 8.1. Entry Point
The compiler searches for `الرئيسية`.
It generates a `main` label that calls it:

```asm
.globl main
main:
    call الرئيسية  ; The user's main function
    ret
```

### 8.2. Name Mangling
Currently, Baa identifiers map 1:1 to Assembly labels.
*   Baa: `جمع` $\to$ Asm: `جمع` (MinGW handles UTF-8 labels correctly).
*   Future consideration: If we link with C, we might need standard ASCII names or explicit exports.