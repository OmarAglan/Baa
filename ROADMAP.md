# Baa Roadmap

# Baa Studio (Editor)
*A custom editor built from scratch in C.*

### Phase 1: The Linter (CLI)
- [ ] Build a tool that reads `.b` files and outputs colored text to the terminal using ANSI codes.

### Phase 2: The GUI (Windows API)
- [ ] Create a window using `windows.h`.
- [ ] Render text.
- [ ] Handle keyboard input.

# Core Language

### v0.0.8 (Next Steps)
- [ ] **Functions:** Defining functions (`دالة`).
- [ ] **Calls:** Calling functions with arguments.
- [ ] **Stack Frames:** Handling recursive calls properly.

# Completed

### v0.0.7 (Done)
- [x] **Loops:** `طالما` (While loop).
- [x] **Unary Operators:** `!` (Not), `-` (Negative numbers).

## v0.0.6 (Done)
- [x] Lexer: Handle `{`, `}`, `(`, `)`, `==`, `!=`.
- [x] Lexer: Keyword `إذا`.
- [x] Parser: Recursive Block parsing.
- [x] Codegen: Label generation and Conditional Jumps (`je`, `jne`).
- [x] Comprehensive Documentation Update (Internals & API).

## v0.0.5 (Done)
- [x] **Renamed** `رقم` to `صحيح` (int).
- [x] **Comments:** Support comments (`//` or `#`).
- [x] **Multi-digit Parsing:** Improve robustness of Arabic number parsing.
- [x] **Error Handling:** Better error messages ("Variable 'x' not found").

## v0.0.4 (Done)
- [x] Lexer: Parse Identifiers (Arabic names) and `=`.
- [x] Parser: Handle Variable Declarations and Lookups.
- [x] Codegen: Implement Stack Frame (RBP) and Local Variable offsets.

## v0.0.3 (Done)
- [x] Lexer: Handle `اطبع`.
- [x] Parser: Handle list of statements (Linked List).
- [x] Codegen: Define Data Section (Format string).
- [x] Codegen: Call `printf` using Windows x64 ABI.

## v0.0.2 (Done)
- [x] Lexer: Handle Arabic Digits (٠-٩).
- [x] Lexer: Handle `+` and `-`.
- [x] Parser: Handle Binary Expressions (1 + 2).
- [x] Codegen: Generate `add` and `sub` assembly.

## v0.0.1 (Done)
- [x] Basic Pipeline (Lexer -> Parser -> Codegen -> GCC).