# Changelog

All notable changes to the B (باء) compiler project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.14.0] - 2025-05-14

### Added

- **Preprocessor:** Implemented predefined macros `__الدالة__` (expands to `L"__BAA_FUNCTION_PLACEHOLDER__"`) and `__إصدار_المعيار_باء__` (expands to `10010L`). (Files affected: `src/preprocessor/preprocessor.c`, `src/preprocessor/preprocessor_line_processing.c`).
- **Testing:** Added tests for `__الدالة__` and `__إصدار_المعيار_باء__` to `tests/resources/preprocessor_test_cases/consolidated_preprocessor_test.baa`.

## [0.1.13.0] - 2025-05-11

### Added

- **Lexer:** Implemented support for Arabic integer literal suffixes: `غ` (unsigned), `ط` (long), `طط` (long long), and their combinations (e.g., `غط`, `ططغ`). The lexer now correctly tokenizes these suffixes as part of `BAA_TOKEN_INT_LIT`. (File affected: `src/lexer/token_scanners.c`).
- **Lexer:** Added C99 keywords `مضمن` (inline) and `مقيد` (restrict) to the lexer. This includes new token types `BAA_TOKEN_KEYWORD_INLINE` and `BAA_TOKEN_KEYWORD_RESTRICT` and updates to the keyword recognition logic. (Files affected: `include/baa/lexer/lexer.h`, `src/lexer/lexer.c`).
- **Lexer:** Removed `دالة` (BAA_TOKEN_FUNC) and `متغير` (BAA_TOKEN_VAR) as keywords from the lexer, aligning with the language specification that Baa uses C-style function and variable declarations. These will now be tokenized as identifiers. (Files affected: `include/baa/lexer/lexer.h`, `src/lexer/lexer.c`).
- **Tools:** Added a new `baa_lexer_tester` tool (`tools/baa_lexer_tester.c`) to facilitate testing of the lexer by tokenizing a Baa source file and printing the token stream. Added to `CMakeLists.txt`.
- **Preprocessor:** Implemented `معرف` as the Arabic equivalent for the `defined` operator in conditional expressions. (File affected: `src/preprocessor/preprocessor_expr_eval.c`).
- **Preprocessor:** Implemented macro expansion rescanning. The preprocessor now correctly rescans the output of a macro expansion for further macro names to be replaced, adhering more closely to C99 standards. This includes handling nested expansions and using the macro expansion stack to prevent direct recursion during the rescan of a macro's own output. (Files affected: `src/preprocessor/preprocessor_line_processing.c`).
- **Preprocessor:** Corrected `__السطر__` (`__LINE__`) predefined macro to expand to an integer constant as per C standard, instead of a string literal. (File affected: `src/preprocessor/preprocessor_line_processing.c`).
- **Preprocessor:** Implemented C99-style Variadic Macros using Arabic syntax: `وسائط_إضافية` (for `...`) in parameter lists and `__وسائط_متغيرة__` (for `__VA_ARGS__`) in macro bodies. This includes parsing, argument collection, and substitution logic. (Files affected: `include/baa/preprocessor/preprocessor.h`, `src/preprocessor/preprocessor_macros.c`, `src/preprocessor/preprocessor_directives.c`, `src/preprocessor/preprocessor_expansion.c`, `src/preprocessor/preprocessor_line_processing.c`, `src/preprocessor/preprocessor.c`).
- **Testing:** Added comprehensive test cases for `معرف` operator, `#الغاء_تعريف` directive, various complex macro rescanning scenarios, and variadic macros to `tests/resources/preprocessor_test_cases/consolidated_preprocessor_test.baa`.
- **Testing:** Added test cases for new Arabic integer suffixes to `tests/resources/lexer_test_cases/lexer_test_suite.baa`.
- **Testing:** Added test cases for new keywords `مضمن` and `مقيد` to `tests/resources/lexer_test_cases/lexer_test_suite.baa`.

### Changed

- **Documentation:** Extensive updates across `docs/language.md`, `docs/c_comparison.md`, `docs/arabic_support.md`, `docs/architecture.md`, `README.md`, `docs/PREPROCESSOR_ROADMAP.md`, and `docs/LEXER_ROADMAP.md` to:
  - Finalize and document Arabic keywords for: `معرف` (for `defined`), `__إصدار_المعيار_باء__` (for Baa version), `__الدالة__` (for `__func__`), variadic macros (`وسائط_إضافية`, `__وسائط_متغيرة__`), other standard directives (`#خطأ`, `#تحذير`, `#سطر`, `أمر_براغما`, `#براغما`), `تعداد` (for `enum`), `لكل` (for C-style `for` loop).
  - Finalize and document Arabic syntax for: float exponent marker (`أ`), literal suffixes (`غ`, `ط`, `طط`, `ح`), and escape sequences (using `\` + Arabic letter).
  - Clarify struct/union member access (`::` for direct, `->` for pointer).
  - Remove `دالة` as a function declaration keyword and `متغير` as a variable declaration keyword from documentation and update examples to C-style.
  - Ensure consistency in logical operator representation (symbols `&&, ||, !` used in syntax).
  - General alignment of C99 features and their Baa equivalents, including keywords like `مستقر`, `خارجي`, `مضمن`, `مقيد`, `متطاير`, `نوع_مستخدم`, `حجم` and type `عدد_صحيح_طويل_جدا`.
- **Lexer:** Updated keyword list in `src/lexer/lexer.c` to use `لكل` for `for` and `أكمل` for `continue` to match documentation.
- **Tests:** Updated `tests/resources/preprocessor_test_cases/consolidated_preprocessor_test.baa` to use `معرف` instead of `defined`.

### Fixed

- **Preprocessor Build:** Resolved various compiler errors (including "undeclared function" and "lvalue" issues) in `src/preprocessor/preprocessor_line_processing.c` related to the rescanning implementation through code refactoring and build cleaning.

### Known Issues

- **Preprocessor:** Token pasting (`##`) during the rescanning phase (i.e., when `##` appears in the output of a previous expansion, not in an original macro body) is not fully correctly handled. Operands of `##` that are macro names might be expanded prematurely before pasting, or the `##` operator itself might be treated literally. This is a known limitation of the current rescanning implementation.

## [0.1.12.0] - 2025-05-09

### Major Refactoring

- **Core Components:** Removed old AST and Parser components to facilitate a fresh start and redesign. (Commit: `454a715`)

### Added

- **Preprocessor:** Implemented support for bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) in conditional compilation expressions (`#إذا`, `#وإلا_إذا`).

### Changed

- **Lexer:**
  - Consolidated the internal `keywords` array definition to `src/lexer/lexer.c`, making it globally accessible via `extern` declaration in `include/baa/lexer/lexer_internal.h`. This removes duplication from `src/lexer/token_scanners.c`.
  - The size of the keywords array (`NUM_KEYWORDS`) is now also globally available via `extern const size_t`.
  - Ensured user-facing lexer error messages are in Arabic.
- **Documentation:**
  - Updated `docs/lexer.md`: Removed outdated "Implement documentation comments" from "Future Improvements" as it's already implemented.
  - Updated `docs/preprocessor.md`:
    - Clarified initial statement on file encoding to reflect auto-detection (UTF-8 default, UTF-16LE).
    - Updated "Input Abstraction" in "Future Enhancements" to note current file/string support and `stdin` as future.
  - Updated `docs/PREPROCESSOR_ROADMAP.md`:
    - Reflected current status of file encoding and input abstraction.
    - Made "macro rescanning" and `#if` macro evaluation items more explicit.
    - Added "Other Standard Directives" (`#error`, `#warning`, `#line`, `#pragma`) as future work.
    - Added "Improve error recovery mechanisms" to implementation priorities.
- **Preprocessor Refactoring:**
  - Split `src/preprocessor/preprocessor_core.c` into:
    - `src/preprocessor/preprocessor_directives.c`: Handles directive logic (lines starting with `#`).
    - `src/preprocessor/preprocessor_line_processing.c`: Handles macro substitution for non-directive lines.
  - `preprocessor_core.c` now delegates to functions in these new files.
  - Updated `CMakeLists.txt` (root and `src/preprocessor/`) to include the new source files.
- **Build System:** Updated project version in root `CMakeLists.txt` to 0.1.12.0.
- **Testing:** Consolidated individual preprocessor test files from `tests/resources/preprocessor_test_cases/` into a single file `tests/resources/preprocessor_test_cases/consolidated_preprocessor_test.baa`. The original files were removed after successful testing of the consolidated file.

### Fixed

- **Build:** Resolved redefinition error for `struct KeywordMapping` by ensuring its definition resides only in `include/baa/lexer/lexer_internal.h`.
- **Preprocessor:**
  - Corrected handling of comments within `#elif` directive expressions.
  - Added stripping of trailing comments from `#define` macro bodies to prevent them from being part of the macro definition.
  - Fixed a crash related to `handle_preprocessor_directive` by ensuring a valid pointer was passed for the `is_conditional_directive` output parameter.
- **Build:**
  - Resolved various build errors related to the preprocessor refactoring and parser function visibility/definitions.
  - Addressed "undefined symbol" errors for `handle_preprocessor_directive` and `process_code_line_for_macros` by correctly adding new preprocessor files to executable targets.
  - Fixed "duplicate symbol" and "undeclared function" errors for `is_type_token` by:
    - Removing the duplicate definition from `src/parser/parser_helper.c` and its declaration from `include/baa/parser/parser_helper.h`.
    - Making the existing definition in `src/parser/parser.c` non-static.
    - Adding the declaration of `is_type_token` to `include/baa/parser/parser.h`.
    - Ensuring `src/parser/parser.c` includes `baa/parser/tokens.h` for visibility of `TOKEN_*` enum values.
  - Corrected `is_type_token` implementation in `src/parser/parser.c` to use the correct `BaaTokenType` members (e.g., `BAA_TOKEN_TYPE_INT`) instead of `TokenType` members, resolving enum comparison warnings.
  - Temporarily stubbed parser functions (`baa_parse_function_rest`, `baa_parse_variable_rest`, `baa_parse_import_directive`) in `src/parser/declaration_parser.c` to resolve "undefined symbol" linker errors and allow the project to build for preprocessor testing. These stubs are temporary, and the underlying parser logic for these declaration types will require further attention.

### Deprecated

- The `[0.1.11.0] - 2025-05-08` entry below is now superseded by this entry due to further changes and a more accurate date. Consider removing or merging.

## [0.1.11.0] - 2025-05-08 (Superseded)

### Added

- **Preprocessor:** Added Input Source Abstraction, allowing `baa_preprocess` to accept input from files (`BAA_PP_SOURCE_FILE`) or directly from wide character strings (`BAA_PP_SOURCE_STRING`) via the new `BaaPpSource` struct.
- **Preprocessor:** Added Input Source Abstraction, allowing `baa_preprocess` to accept input from files (`BAA_PP_SOURCE_FILE`) or directly from wide character strings (`BAA_PP_SOURCE_STRING`) via the new `BaaPpSource` struct.
- **Preprocessor:** Added support for reading UTF-8 encoded input files (with or without BOM), in addition to UTF-16LE. Encoding is auto-detected.
- **Lexer:** Added support for documentation comments (`/** ... */`), tokenized as `BAA_TOKEN_DOC_COMMENT`.
- **Lexer:** Added support for multiline string literals (`"""..."""`). (Commit: 9b4b8a0)
- **Lexer:** Added support for raw string literals (`خ"..."` and `خ"""..."""`), including single-line newline error handling. (Commit: 5caef53)

### Fixed

- **Compiler:** Fixed use-after-free bug and potential infinite loop in lexer token processing loop (`compiler.c`) by refactoring to a standard `for` loop, ensuring correct termination on EOF or ERROR tokens. (Commit: d08a915)
- **Lexer:** Corrected handling of single-line raw strings (`خ"..."`) to properly report an error if a newline is encountered before the closing quote. (Commit: 5caef53)

### Changed

- **Lexer:** Improved error reporting for multiline strings by tracking start line/column. (Commit: 9b4b8a0)

## [0.1.10.0] - 2025-05-07

### Changed

- **Lexer Enhancements & Fixes:**
  - **Numeric Literal Tokenization:**
    - Correctly tokenizes Arabic-Indic digits (٠-٩ / U+0660-U+0669) as part of integer and float literals.
    - Implemented support for binary (`0b`/`0B`) and hexadecimal (`0x`/`0X`) integer prefixes.
    - Added support for underscores (`_`) as separators in all valid positions within numeric literals (integers, floats, including prefixed numbers and exponent parts).
    - Recognizes the Arabic decimal separator (`٫`, U+066B) as a valid decimal point in float literals. (Note: Console display of `٫` in lexemes might appear as `?` due to terminal limitations, but internal processing is correct).
  - **Dispatch Logic:** Refined character type dispatch in `baa_lexer_next_token` to correctly prioritize digit types (Arabic-Indic, then Western) before general identifier characters, resolving misidentification of Arabic-Indic digits as identifiers.
  - **Error Messaging:** Clarified the error message for invalid escape sequences (e.g., `\'`) within string literals.
  - **Modularization:** Further modularized the lexer by moving token scanning logic (`scan_identifier`, `scan_number`, `scan_string`, `scan_char_literal`) into `src/lexer/token_scanners.c` and character utilities into `src/lexer/lexer_char_utils.c`. Introduced `src/lexer/lexer_internal.h` for internal declarations. (This completes earlier modularization efforts and resolves associated build issues).
- **Documentation:**
  - Significantly updated `docs/lexer.md` to reflect the current capabilities regarding numeric literal parsing, Arabic language support, and overall lexer functionality.

### Added

- **Testing:**
  - Created a new comprehensive lexer test suite: `tests/resources/lexer_test_cases/lexer_test_suite.baa` to cover a wide range of lexer features and edge cases.

## [0.1.9.9] - 2025-05-06

### Added

- Standalone preprocessor tester executable (`tools/baa_preprocessor_tester.c`) for isolated testing.
- Placeholder implementations for missing parser functions (`baa_parse_if_statement`, `baa_parse_while_statement`, `baa_parse_for_statement`, `baa_parse_return_statement`, `baa_create_compound_assignment_expr`, `baa_create_grouping_expr`) to allow the build to complete.
- Added Arabic predefined macros to the preprocessor:
  - `__الملف__` (equivalent to `__FILE__`)
  - `__السطر__` (equivalent to `__LINE__`)
  - `__التاريخ__` (equivalent to `__DATE__`)
  - `__الوقت__` (equivalent to `__TIME__`)
- Unified preprocessor error reporting to consistently use original source location (file, line, column) for all errors.

### Fixed

- Preprocessor: Fixed bug where `#إذا` directives using macros defined as simple integers (e.g., `#تعريف DEBUG 1`) were not evaluated correctly, causing the enclosed block to be skipped erroneously. The expression evaluator now handles this case.
- Build: Corrected mismatch between `baa_parse` and `baa_parse_program` definitions/declarations in `src/parser/parser.c`.
- Build: Added missing declaration for `baa_parse_for_statement` in `include/baa/parser/parser.h`.

### Changed

- **Parser Refactoring:** Split `src/parser/parser.c` into more modular components:
  - `src/parser/parser_helper.c`: Contains core utilities like `advance`, `peek`, `match_keyword`, error helpers, etc.
  - `src/parser/statement_parser.c`: Contains statement dispatching (`baa_parse_statement`) and block parsing (`baa_parse_block`).
  - `src/parser/declaration_parser.c`: Contains declaration dispatching (`baa_parse_declaration`) and import parsing (`baa_parse_import_directive`).
- **Code Synchronization:** Aligned parser implementation files and `docs/PARSER.md` documentation. Corrected public API documentation, added missing documentation for imports, resolved conflicting function definitions.

### Fixed

- **Build Errors:**
  - Resolved duplicate symbol linker errors by consolidating function implementations (error handling, parser helpers) and removing `src/parser/error_handling.c`.
  - Fixed math library linking error (`m.lib`) on Windows/Clang by adding `m` to `target_link_libraries` in the root `CMakeLists.txt`.

### Removed

- Obsolete parser source files (`src/parser/function_parser.c`, `src/parser/error_handling.c`) after refactoring.

### Added

- LLVM integration for code generation
  - Basic LLVM IR generation infrastructure
  - Function generation with proper return types
  - Support for multiple target platforms (x86-64, ARM64, WebAssembly)
  - Integration with existing compiler pipeline
  - Arabic error messages for code generation failures
- Enhanced compiler driver
  - Integrated code generation into the compilation pipeline
  - Added support for generating LLVM IR output files
  - Improved error handling with Arabic messages
- New parser components
  - Added control_flow_parser.c for handling control structures
  - Added declaration_parser.c for variable and function declarations
  - Added expression_parser.c for expression parsing
  - Added function_parser.c for function handling
  - Added type_parser.c for type system parsing
- Enhanced AST implementation
  - Added new node types and structures
  - Added visitor interface for AST traversal
  - Added scope management system
  - Added comprehensive statement handling
  - Added literal value handling
- Improved testing infrastructure
  - Added unit tests for lexer functionality
  - Added number parsing tests
  - Added control flow tests
  - Added type and operator tests
  - Added test framework with assertion macros
- Memory management improvements
  - Added baa_malloc/baa_free for consistent memory handling
  - Added baa_strdup/baa_strndup for string operations
  - Added proper memory cleanup in all components

### Changed

- Updated build system
  - Added LLVM package detection
  - Added conditional compilation for LLVM features
  - Reorganized component subdirectories
  - Enhanced test configuration
  - Added memory leak detection
- Improved type system
  - Updated parameter handling from BaaFuncParam to BaaParameter
  - Enhanced type checking and validation
  - Added support for user-defined types
  - Improved type conversion system
- Enhanced error handling
  - Added Arabic error messages
  - Improved error recovery mechanisms
  - Added detailed error context
  - Enhanced error reporting system
- Updated project structure
  - Reorganized source files into logical components
  - Moved test files to appropriate directories
  - Enhanced documentation structure
  - Improved code organization

### Fixed

- Memory management issues
  - Fixed memory leaks in parser functions
  - Fixed string handling in parameter creation
  - Fixed resource cleanup in AST operations
  - Fixed memory management in test framework
- Type system issues
  - Fixed type declarations in parser functions
  - Fixed parameter handling in function declarations
  - Fixed type checking in expressions
  - Fixed type conversion edge cases
- Parser improvements
  - Fixed compilation errors in declaration_parser.c
  - Fixed control flow parsing issues
  - Fixed expression parsing bugs
  - Fixed function parsing problems
- Test framework issues
  - Fixed test result reporting
  - Fixed memory leak detection
  - Fixed test file organization
  - Fixed test execution flow

### Removed

- Removed deprecated components
  - Removed old control_flow.h
  - Removed old parser.h
  - Removed old tokens.h
  - Removed old types.h
- Removed redundant files
  - Removed duplicate test files
  - Removed old documentation files
  - Removed unused header files
  - Removed obsolete test framework files

## [0.1.9.8] - 2025-02-09

### Added

- Complete decimal number parsing implementation
  - Support for both Western (0-9) and Arabic-Indic (٠-٩) digits
  - Support for Western (.) and Arabic (٫) decimal separators
  - Comprehensive error handling with Arabic messages
  - Memory-safe number token management
- Enhanced lexer functionality
  - Added number validation and format checking
  - Implemented overflow protection
  - Added support for preserving original text representation

### Changed

- Updated lexer to handle both integer and decimal numbers
- Enhanced error handling system with detailed Arabic messages
- Improved memory management for number tokens
- Updated build system to include number parser module

### Fixed

- Memory leaks in number parsing
- Decimal point validation
- Overflow handling in large numbers
- Arabic numeral conversion accuracy

## [0.1.9.7] - 2025-02-08

### Added

- Restored decimal number parsing with improved implementation
- Completed control flow parsing implementation
  - Full support for إذا (if) statements
  - Integrated وإلا (else) statements
  - Implemented طالما (while) loops
- Added comprehensive test suite for control flow structures

### Changed

- Updated parser to handle decimal numbers efficiently
- Enhanced error handling for control flow statements
- Improved documentation to reflect completed features

### Fixed

- Edge cases in decimal number parsing
- Control flow statement nesting issues
- Parser error recovery in complex control structures

## [0.1.9] - 2025-02-06

### Added

- Enhanced parser debug output for better troubleshooting
- Added test files for Arabic program parsing
- Added UTF-8 support for Arabic identifiers in parser

### Changed

- Refactored parser functions for better error handling
- Simplified number parsing (temporarily removed decimal support)
- Improved error messages with more context

### Fixed

- Fixed memory leaks in parser error handling
- Fixed function declaration parsing
- Fixed identifier parsing for UTF-8 characters

### Development

- Added more debug output in parse_declaration and parse_program
- Reorganized parser code structure
- Added test cases for basic language features

## [0.1.8] - 2025-02-01

### Added

- Full implementation of AST (Abstract Syntax Tree)
- Complete type system implementation
- Operator system with precedence rules
- Control flow structures (if, while, return)
- Error handling with Arabic message support
- Memory management utilities
- Support for Arabic file extension `.ب`
- UTF-8 encoding support for source files
- Basic compiler infrastructure for Arabic source files
- Example program with Arabic file name (`برنامج.ب`)
- EditorConfig settings for Arabic source files
- `#تضمين` directive for imports
- Updated character type from `محرف` to `حرف`

### Changed

- Removed placeholder implementations
- Updated build system to exclude unimplemented components
- Improved project structure and organization
- Build system now handles Arabic file extensions
- Updated CMake configuration for UTF-8 support
- Enhanced file handling for Arabic source files
- Improved project documentation with Arabic examples
- Refined language specification with proper Arabic keywords
- Updated type system documentation to use `حرف` instead of `محرف`
- Added proper import syntax using `#تضمين`

### Fixed

- Corrected character type naming convention
- Standardized import directive syntax
- Aligned documentation with actual language specification

## [0.1.7] - 2025-02-01

### Added

- Basic control flow structures implementation:
  - إذا/وإلا (if/else) statement support
  - طالما (while) loop support
  - إرجع (return) statement support
- New control_flow.h header with comprehensive control flow API
- Memory-safe statement creation and management functions
- Unit tests for all control flow structures
- Integration with existing build system

### Changed

- Updated CMakeLists.txt to include control flow implementation
- Enhanced project structure with new control flow components
- Updated roadmap to reflect completed control flow milestone

## [0.1.6] - 2025-01-31

### Added

- Basic type system implementation with K&R C compatibility
- Core operator system with arithmetic and comparison operators
- Comprehensive test suite for type system and operators
- Type conversion and validation system
- Arabic operator names and error messages
- UTF-16 support for character type

### Changed

- Updated project structure with src/ and tests/ directories
- Enhanced type safety with strict conversion rules
- Improved error handling in type system

### Documentation

- Added type system implementation details
- Added operator system specifications
- Updated development guide with testing procedures
- Added code examples for type and operator usage

## [0.1.5] - 2025-01-31

### Added

- Comprehensive K&R C feature comparison in `docs/c_comparison.md`
- Detailed implementation roadmap for K&R C compatibility
- Arabic equivalents for all K&R C keywords and operators
- Standard library function mappings with Arabic names

### Changed

- Updated `docs/language.md` to align with K&R C specification
- Updated `docs/development.md` to follow K&R coding style
- Enhanced documentation with UTF-16LE and bidirectional text support details
- Improved error message documentation with Arabic language support

### Documentation

- Added detailed type system documentation
- Added memory model specifications
- Added preprocessor directive documentation
- Added comprehensive code examples
- Updated build system documentation

## [0.1.0] - 2025-01-31

### Added

- Initial project structure and build system
- Basic file reading with UTF-16LE support
- Support for Arabic text in source files
- Modular component architecture:
  - Lexer component with UTF-16 file handling
  - AST component with basic node structure
  - Parser component (placeholder)
  - Code generator component (placeholder)
  - Utils component with error handling and memory management
- Comprehensive test suite:
  - Lexer tests for file handling
  - Utils tests for memory and error handling
- Project documentation:
  - README with build instructions
  - API documentation
  - Example programs
- CMake-based build system with:
  - Cross-platform support
  - Test integration
  - Modular component builds
- Development tools:
  - Automated testing framework
  - Error handling system
  - Memory management utilities
- Initial project setup
- Basic file reading functionality
- UTF-16LE support for Arabic text
- CMake build system configuration
- Project documentation
- Code organization and structure

### Changed

- Organized project structure into logical components
- Improved build system with proper dependency management
- Enhanced file handling with proper Unicode support
- Improved project structure
- Enhanced build system configuration
- Updated documentation

### Fixed

- UTF-16LE file handling and BOM detection
- Memory management in AST operations
- File size calculations for wide character content
- File handling improvements
- Better error messages in Arabic

## [0.0.1] - 2025-01-31

### Added

- Initial release
- Basic file reading capabilities
- Command-line interface
- Simple error reporting

## [Unreleased]

### Planned

- Parser implementation
- Code generation
- Symbol table
- Type system
- Error recovery
- Optimization passes
- Arabic language syntax definition
- Comprehensive test suite expansion
- Documentation in both English and Arabic
