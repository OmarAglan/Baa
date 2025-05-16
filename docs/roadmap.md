# Baa Language Roadmap (خارطة الطريق)

## Current Version: 0.1.10.0 (Working towards 0.2.0)

This roadmap outlines the high-level plan for the Baa language project. For detailed status and plans for specific components (Lexer, Parser, AST, Preprocessor), please refer to their respective roadmap documents (`LEXER_ROADMAP.md`, `PARSER_ROADMAP.md`, `AST_ROADMAP.md`, etc.).

**Note:** The compilation process now includes an explicit **Preprocessor** stage that handles directives (`#تضمين`, `#تعريف`) before the Lexer stage.

### Immediate Goals (0.2.0)

1.  **Preprocessor Enhancements:** Continue Preprocessor Enhancements: Implement remaining C99 features (Variadic Macros, `__func__`, `#error`, `#pragma`, etc. with Arabic syntax), and full macro expansion in conditional expressions. (Macro rescanning and basic conditionals are now implemented). (See `PREPROCESSOR_ROADMAP.md` for details).
2.  **Parser Enhancements:** Complete core control flow parsing, restore decimal number parsing, implement full operator precedence, enhance UTF-8 support, add robust error recovery, and implement comprehensive source position tracking. (See `PARSER_ROADMAP.md` for details).
3.  **Code Generation (Priority):** Complete LLVM integration, implement the basic code emission pipeline (expressions, statements, control flow), add debug information support, and optimize generated code for Arabic text handling.
4.  **Lexer Enhancements:**
    - **Completed:** Significantly improved numeric literal scanning (Arabic-Indic digits, binary/hex prefixes, underscores, Arabic decimal separator `٫`). String/char escape handling is robust (`\uXXXX`, standard C escapes; `\'` in strings is an error). Comment skipping (`//`, `/* */`, `#`) is functional. Error message for invalid string escapes clarified. Lexer modularization is complete.
    - **Ongoing/Next:** Implement planned Arabic syntax for literals (escape sequences, suffixes, float exponent `أ`). Focus on comprehensive testing of all token types, edge case robustness, and ensuring consistent error reporting quality. Review and refine `LEXER_ROADMAP.md`.
5.  **AST Improvements:** Enhance scope management, improve modularity, implement comprehensive control flow node handling (including `تعداد` for enum), and add robust error state tracking. (See `AST_ROADMAP.md` for details).

### Mid-term Goals (0.3.0)

1.  **Advanced Language Features:** Modules, custom types (structs/records with `::`/`->` access), exception handling, function overloading.
2.  **AST and Type System Improvements:** Type inference, generics/templates, user-defined types (unions, enums), improved memory management (ref counting/GC).
3.  **Lexer and Parser Enhancements:** Improved Unicode/RTL support, enhanced error recovery, better source location tracking.
4.  **Arabic Language Support:** Complete Arabization of planned language syntax elements (keywords, operators, literals), full RTL support, enhanced Arabic error messages, standard library documentation, formatting tools.
5.  **Development Tools:** IDE plugins, code completion, real-time syntax checking, debugging tools.

### Long-term Goals (0.4.0)

1.  **Standard Library:** Comprehensive I/O, string manipulation, file system operations, networking, concurrency features.
2.  **Testing Infrastructure:** Comprehensive test suite, automated pipeline, benchmarking, coverage reporting, specific tests for errors/Unicode/memory.
3.  **Code Generation and Optimization:** Optimization passes, target-specific optimizations, inline assembly/intrinsics, register allocation, vectorization/SIMD.
4.  **Error Handling and Diagnostics:** Improved error messages (context/suggestions), warning system, visual error highlighting, error categorization.
5.  **Documentation and Examples:** Comprehensive language specification, bilingual docs, more examples, tutorials, best practices.
6.  **Compiler Features:** Incremental compilation, plugin system, enhanced diagnostics, cross-platform support.

### Future Considerations (1.0.0)

1.  **Language Evolution:** Metaprogramming, generics, pattern matching, concurrency, functional programming features, contract programming.
2.  **Ecosystem Development:** Package manager, standard library repository, dependency management, community guidelines, build system integration.
3.  **Tools and Infrastructure:** Online playground, cloud IDE, CI/CD templates, documentation generator, Language Server Protocol (LSP) implementation.

This roadmap is subject to change based on community feedback and project needs. Regular updates will be made to reflect new requirements and priorities.
