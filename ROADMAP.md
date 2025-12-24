# Baa Roadmap

## v0.0.4 (Current)
- [ ] Lexer: Parse Identifiers (Arabic names) and `=`.
- [ ] Parser: Handle Variable Declarations and Lookups.
- [ ] Codegen: Implement Stack Frame (RBP) and Local Variable offsets.

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