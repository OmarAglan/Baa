# Baa Roadmap

## Core Language Development

### v0.0.8 (Current Priority: Functions & Structure)
This version shifts the language from a linear script to a structured systems language.

#### Phase 1: Structure & Globals (v0.0.8-alpha)
- [ ] **AST Refactor:** Change Root Node to hold a list of *Declarations* (Functions/Globals) instead of *Statements*.
- [ ] **Parser Logic:** Implement `peek()` lookahead to resolve the ambiguity between `صحيح س = ...` (Variable) and `صحيح س(...)` (Function).
- [ ] **Global Variables:** Implement `.data` section generation for variables defined outside functions.

#### Phase 2: Function Definitions (v0.0.8-beta)
- [ ] **Entry Point:** Detect function `الرئيسية` and export it as `main`.
- [ ] **Stack Frames:** Implement standard x64 Prologue (`push rbp`, `mov rbp, rsp`, `sub rsp`) and Epilogue.
- [ ] **Scope Management:** Implement a Symbol Table stack (reset local offsets when entering a function).

#### Phase 3: Calls & ABI (v0.0.8-final)
- [ ] **Function Calls:** Parse `func(arg1, arg2)`.
- [ ] **Parameters:** Handle arguments in function definitions.
- [ ] **Windows x64 ABI:**
    - [ ] Pass first 4 args in `RCX`, `RDX`, `R8`, `R9`.
    - [ ] Spill registers to stack in the Prologue (Shadow backing).
    - [ ] Align Stack to 16 bytes before calls.
    - [ ] Allocate 32-byte "Shadow Space" for callees.

### v0.0.9 (Planned)
- [ ] **FFI (Foreign Function Interface):** Keyword `خارجي` to import C functions (e.g., `malloc`, `free`).
- [ ] **Recursion:** Stress test stack frames with recursive algorithms (Fibonacci).

---

## Baa Studio (Editor)
*A custom editor built from scratch in C.*

### Phase 1: The Linter (CLI)
- [ ] Build a tool that reads `.b` files and outputs colored text to the terminal using ANSI codes.

### Phase 2: The GUI (Windows API)
- [ ] Create a window using `windows.h`.
- [ ] Render text.
- [ ] Handle keyboard input.

---

## Completed Milestones

### v0.0.7 (Done)
- [x] **Loops:** `طالما` (While loop).
- [x] **Assignments:** Update existing variables (`x = 1.`).
- [x] **Codegen:** Implemented Loop Labels (Start/End).

### v0.0.6 (Done)
- [x] **Control Flow:** `إذا` (If), Blocks `{...}`.
- [x] **Logic:** Comparisons (`==`, `!=`).
- [x] **Codegen:** Label generation and Conditional Jumps (`je`).
- [x] **Docs:** Comprehensive Internals & API documentation.

### v0.0.5 (Done)
- [x] **Type System:** Renamed `رقم` to `صحيح` (int).
- [x] **Comments:** Added Single Line Comments (`//`).

### v0.0.4 (Done)
- [x] **Memory:** Variable Declarations and Stack Offsets.
- [x] **Symbol Table:** Basic global lookup.

### v0.0.3 (Done)
- [x] **I/O:** Added `اطبع` (Print) via Windows `printf`.
- [x] **Parser:** Support for multiple statements (Linked List).

### v0.0.2 (Done)
- [x] **Math:** Arabic Numerals (٠-٩) and Operators (+, -).

### v0.0.1 (Done)
- [x] **Core:** Basic Pipeline (Lexer -> Parser -> Codegen -> GCC).