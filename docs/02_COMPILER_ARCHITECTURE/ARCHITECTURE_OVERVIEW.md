# Baa Language Architecture

## Overview

Baa (باء) is designed with a modular architecture that separates concerns into distinct components. Each component is responsible for a specific aspect of the compilation process, making the system maintainable and extensible. **As of 2025-07-06, the core compiler infrastructure is production-ready with Priority 4 (Function Definitions and Calls) completed.**

## Core Components

### 0. Preprocessor (المعالج المسبق)

The initial stage that processes the source file before tokenization:

- **Directive Handling**: Processes directives like `#تضمين` (include) and `#تعريف` (define).
- **File Inclusion**: Merges included files into a single stream.
- **Macro Expansion**: Substitutes defined object-like and function-like macros. Supports stringification (`#`) and token pasting (`##`). Detects recursive expansion.
- **Input Encoding**: Expects UTF-16LE input files (checks BOM), or UTF-8 (with/without BOM, auto-detected and converted to UTF-16LE internally).
- **Output**: Produces a single `wchar_t*` stream (UTF-16LE) for the lexer.
- **Circular Include Detection**: Prevents infinite include loops.
- **Conditional Compilation**: Handles `#إذا`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`. Evaluates constant integer expressions for `#إذا`/`#وإلا_إذا` (supports arithmetic, comparison, logical, bitwise operators, `معرف()`).
- **Error Reporting**: Reports errors with file path and line number context. Accumulates multiple diagnostics.
- **Features & Status:**
  - Implemented as a separate stage.
  - Handles `#تضمين` (relative and standard paths).
  - Handles `#تعريف` (object-like and function-like macros, including `#`, `##`, variadic, and **rescanning of expansion results**).
  - Handles `#الغاء_تعريف` to undefine macros.
  - Handles conditional compilation (`#إذا_عرف`, `#إذا_لم_يعرف`, `#إلا`, `#نهاية_إذا`, `#إذا`, `#وإلا_إذا`) with expression evaluation (using `معرف` for `defined`).
  - Supports `#خطأ` and `#تحذير` directives.
  - Detects circular includes and recursive macro expansion.
  - Provides predefined macros (`__الملف__`, `__السطر__` (int), `__التاريخ__`, `__الوقت__`, `__الدالة__` (placeholder), `__إصدار_المعيار_باء__`).
  - *See `docs/PREPROCESSOR_ROADMAP.md` for latest status, including error recovery enhancements.*

### 1. Lexer

The lexical analyzer responsible for tokenizing source code. It has a modular structure:

- `lexer.c`: Core dispatch logic (`baa_lexer_next_token`) and helper functions.
- `token_scanners.c`: Implements specific scanning functions for identifiers, numbers, strings, and comments.
- `lexer_char_utils.c`: Provides character classification utilities (e.g., for Arabic letters, digits).
- **Token Generation**: Converts source text (UTF-16LE output from preprocessor) into a stream of tokens.
- **Unicode Support**: Full support for Arabic characters in identifiers, literals, and keywords. Recognizes Arabic-Indic digits.
- **Source Tracking**: Accurate line and column tracking for tokens and errors.
- **Enhanced Error Handling**: Comprehensive error detection and reporting system with:
  - **Specific Error Types**: 8 distinct error token types (unterminated strings/chars/comments, invalid escapes/numbers/suffixes/characters)
  - **Error Codes**: Unique codes (1001-1009, 9001) for internationalization and categorization
  - **Arabic Messages**: All error messages and suggestions provided in Arabic
  - **Rich Context**: Error categories, helpful suggestions, and precise source location tracking
  - **Error Recovery**: Robust synchronization to continue tokenizing after errors
- **Whitespace and Newlines**: Tokenizes spaces/tabs (`BAA_TOKEN_WHITESPACE`) and newlines (`BAA_TOKEN_NEWLINE`) separately.
- **Comment Handling**: Tokenizes single-line (`//` -> `BAA_TOKEN_SINGLE_LINE_COMMENT`), multi-line (`/*...*/` -> `BAA_TOKEN_MULTI_LINE_COMMENT`), and documentation comments (`/**...*/` -> `BAA_TOKEN_DOC_COMMENT`). Lexemes for comments contain the content excluding delimiters.
- **Numeric Literal Recognition**:
  - Identifies various number formats: integers (decimal, binary `0b`/`0B`, hexadecimal `0x`/`0X`), floats (using `.` or `٫` as decimal separator).
  - Uses Arabic exponent marker `أ` for scientific notation (English `e`/`E` are not supported) - fully implemented and working.
  - Supports Arabic-Indic digits (`٠`-`٩`) and Western digits (`0`-`9`) within all parts of numbers.
  - Supports underscores (`_`) as separators for readability in numbers.
  - Tokenizes Arabic integer literal suffixes (`غ`, `ط`, `طط`) and float suffix (`ح`) - fully implemented and working.
  - The lexer's `scan_number` function handles syntactic recognition and extracts the raw lexeme. The separate `number_parser.c` utility converts these lexemes to actual numeric values.
- **String/Char Literals**: Handles string (`"..."`, `"""..."""` multiline, `خ"..."` raw) and character (`'...'`) literals.
  - Processes **Baa-specific Arabic escape sequences** (e.g., `\س` for newline, `\م` for tab, `\يXXXX` for Unicode, `\هـHH` for hex byte) - fully implemented and working in all string types.
  - Standard C escapes like `\n`, `\t`, `\uXXXX` are **not** supported and will result in errors.
- **Features & Status:**
  - Core lexer functionality and modular structure are implemented.
  - Robust token handling for keywords, identifiers, operators, and various literal types.
  - String and character literal support with Baa-specific escapes (conceptually).
  - Source position tracking.
  - Error token generation.
  - *See `docs/LEXER_ROADMAP.md` for detailed status and planned enhancements.*

### 2. Parser

Transforms tokens into a structured AST:

- **Status**: New Design v2 - Core infrastructure implemented (v0.1.19.0), detailed parsing rules ongoing.
- **Approach**: Recursive descent.
- **Output**: Abstract Syntax Tree (AST) composed of `BaaNode`s.
- **Error Handling**: Syntax error detection, reporting, and basic panic mode recovery.
- *Refer to `docs/PARSER.md` and `docs/PARSER_ROADMAP.md` for details on the new design.*

### 3. Abstract Syntax Tree (AST)

The AST module provides the foundation for representing code structure using a unified `BaaNode` approach.

- **Status**: New Design v2 - Core types and basic literal nodes implemented (v0.1.18.0), ongoing development for other node types.
- **Structure**:
  - Unified `BaaNode` with `BaaNodeKind kind`, `BaaSourceSpan span`, and `void* data`.
  - Specific data structs (e.g., `BaaLiteralExprData`) for each node kind.
- **Memory Management**: `baa_ast_new_node()` for base creation, `baa_ast_free_node()` for recursive freeing.
- **Traversal**: Visitor pattern planned for AST traversal.
- *Refer to `docs/AST.md` and `docs/AST_ROADMAP.md` for details on the new design.*

### 4. Type System

A robust type system that supports both C compatibility and Arabic type names:

- **Basic Types**: `عدد_صحيح` (int), `عدد_حقيقي` (float), `حرف` (char), `منطقي` (bool), `فراغ` (void).
- **Type Checking**: Static type checking (planned for Semantic Analysis).
- **Type Conversion**: Implicit and explicit type conversion rules (planned for Semantic Analysis).
- **Features & Status:**
  - Core type definitions and structures implemented.
  - Array type structure (`BAA_TYPE_ARRAY`) defined.

### 5. Operators

Operator system with full Arabic support:

- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Comparison**: `==`, `!=`, `>`, `<`, `>=`, `<=`
- **Logical**: Uses symbols `&&` (و - AND), `||` (أو - OR), `!` (ليس - NOT).
- **Precedence**: Clear operator precedence rules defined.
- **Features & Status:**
  - Operator definitions, symbols, and precedence implemented.
  - Basic validation stubs exist.

### 6. Control Flow (Semantic and AST Representation)

Control structures with Arabic keywords:

- **Conditions**: `إذا` (if), `وإلا` (else)
- **Loops**: `طالما` (while), `لكل` (for)
- **Functions**: (Uses C-style declarations)
- **Return**: `إرجع` (return)
- **Status**: Keywords tokenized by Lexer. Parser rules and AST node definitions are part of the new Parser/AST design (ongoing). Semantic validation planned.

### 7. Utils

Utility functions and support features:

- **Error Handling**: Basic error enums and reporting functions.
- **Memory Management**: Safe memory allocation wrappers (`baa_malloc`, `baa_free`, `baa_strdup`).
- **String Handling**: UTF-16LE wide character string operations.
- **File Operations**: Source file handling with UTF-8/UTF-16LE auto-detection (in preprocessor) and UTF-16LE processing (in lexer).
- **Features & Status:** Implemented.

### 8. Semantic Analysis (Planned)

Responsible for verifying static semantics, name resolution, and type checking.

- **Status**: Basic flow analysis structure exists (`src/analysis/`), but full semantic analysis is pending the new AST.
- **Features (Planned):**
  - Symbol table management and scope handling.
  - Name resolution (linking identifiers to declarations).
  - Comprehensive type checking and inference.
  - Control flow analysis (reachability, return paths).
  - AST annotation with semantic information.
  - *See `docs/SEMANTIC_ANALYSIS.md` and `docs/SEMANTIC_ANALYSIS_ROADMAP.md`.*

### 9. Code Generation (Planned)

Transforms the (semantically analyzed) AST into executable code or intermediate representation.

- **Status**: Basic LLVM integration stubs and conditional build logic exist. Actual IR generation from AST is pending.
- **Features (Planned):**
  - LLVM IR generation from the new, semantically-analyzed AST.
  - Optimization passes, debug information, source mapping.
  - Support for multiple target platforms (x86-64, ARM64, Wasm).
  - *See `docs/LLVM_CODEGEN.md` and `docs/LLVM_CODEGEN_ROADMAP.md`.*

## Build System

The build system is based on CMake and provides:

- **Component Management**: Each component built as a separate static library.
- **Test Integration**: Automated testing for each component (via CTest).
- **Cross-Platform**: Support for multiple operating systems.
- **Configuration**: Flexible build configuration options (e.g., `USE_LLVM`).

## Testing Framework

Comprehensive testing infrastructure:

- **Unit Tests**: Tests for individual components (`tests/unit/`).
- **Integration Tests**: Tests for component interaction (planned in `tests/integration/`).
- **Arabic Test Cases**: Test cases with Arabic input (`tests/resources/`).
- **Framework**: Simple assertion-based test framework in `tests/framework/`.

## Error Handling

Comprehensive error handling system with enhanced lexical error support:

### Lexical Error Handling ✅ (Fully Implemented)
- **Specific Error Types**: 8 distinct error token types with unique error codes
- **Error Categories**: Organized by type (string, escape, character, number, comment, memory, operator)
- **Arabic Localization**: Complete Arabic error messages and actionable suggestions
- **Rich Context**: Error codes, categories, suggestions, and enhanced source location tracking
- **Error Recovery**: Robust synchronization mechanisms to continue after errors
- **Memory Management**: Proper cleanup of error contexts and enhanced token structures

### Other Error Types
- **Preprocessor Errors**: File inclusion, macro expansion, conditional compilation errors
- **Parser Errors**: Syntax error detection, reporting, and basic panic mode recovery (planned)
- **Semantic Errors**: Type checking, scope resolution errors (planned)
- **CodeGen Errors**: Code generation and optimization errors (planned)

### Error Reporting Features
- **Source Location**: Precise error reporting with file, line, and column information
- **Enhanced Spans**: Start/end positions and character offsets for better IDE integration
- **Error Codes**: Unique numeric codes for internationalization and tooling support

## Memory Management

Memory management strategy:

- **Manual Allocation**: Uses custom wrappers (`baa_malloc`, `baa_free`, `baa_strdup`) around standard library functions for potential tracking or replacement.
- **Ownership Rules**: Components are responsible for freeing memory they allocate (e.g., lexer allocates tokens, parser/AST manages AST nodes).

## Future Extensions

Planned enhancements:

- Comprehensive Semantic Analysis and Code Generation.
- Full Standard Library implementation.
- Optimizer passes.
- IDE Integration (LSP).
