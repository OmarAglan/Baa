# Baa Roadmap

## v0.0.6 (Current)
- [ ] Lexer: Handle `{`, `}`, `(`, `)`, `==`, `!=`.
- [ ] Lexer: Keyword `إذا`.
- [ ] Parser: Recursive Block parsing.
- [ ] Codegen: Label generation and Conditional Jumps (`je`, `jne`).

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