# Baa Roadmap

## Core Language Development

### v0.1.0 (Current: Milestone: Text Support & Logic Operators)
The "Hello World" release. Shifting from a number-cruncher to a general-purpose language.
- [x] **String Literals:** `"text"` (Stored in `.rdata`).
- [x] **Char Literals:** `'x'`.
- [x] **Unary Minus:** `-5`.
- [x] **Polymorphic Print:** `اطبع` supports strings and integers.

### v0.1.1 (Next: Structured Data)
Moving beyond single variables to collections.
- [ ] **Arrays:** Fixed-size stack arrays (e.g., `صحيح مصفوفة[١٠].`).
- [ ] **Indexing:** Reading/Writing `مصفوفة[٠]`.
- [ ] **Advanced Loops:** `لكل` (For Loop) syntax to iterate easily over arrays.

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

### v0.1.0 (Done)
- [x] **Strings:** Added string literal support.
- [x] **Printing:** Updated `اطبع` to handle multiple types.
- [x] **Negative Numbers:** Added unary minus support.

### v0.0.9 (Done)
- [x] **Advanced Math:** Multiplication, Division, Modulo.
- [x] **Comparisons:** Greater/Less than logic.
- [x] **Parser:** Operator Precedence Climbing (PEMDAS).

### v0.0.8 (Done)
- [x] **Functions:** Function Definitions (`صحيح func(...) {...}`) and Calls (`func(...)`).
- [x] **Entry Point:** Mandatory `الرئيسية` function exported as `main`.
- [x] **Scoping:** Global Variables (Data Section) vs Local Variables (Stack).
- [x] **Windows x64 ABI:** 
    - [x] Register passing (RCX, RDX, R8, R9).
    - [x] Stack alignment (16-byte).
    - [x] Shadow Space allocation.
- [x] **Parser:** Lookahead logic to distinguish Variables from Functions.

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