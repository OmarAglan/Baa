# Changelog

All notable changes to the B (باء) compiler project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.30.0] - 2025-06-22

### Changed

- **Lexer:** Updated `src/lexer/number_parser.c` to replace `'e'/'E'` with the Arabic exponent marker `'أ'` for scientific notation in floating-point literals.

## [0.1.29.0] - 2025-06-21

### Added

- **Lexer: Float Literal Error Fix:**
  - Correctly tokenized inputs like `.456` as `BAA_TOKEN_FLOAT_LIT` instead of `BAA_TOKEN_INT_LIT`.
  - Commit: `fa153f171fe824ebdb92d187ecb3d1de9ef22d76`
- **Lexer: Multiline String Escape Error Fix (Partial):**
  - Refactored `scan_multiline_string_literal` to clarify escape sequence processing.
  - *Note: Further fixes are needed for correct multiline string tokenization and escape handling.*
  - Commit: `632b3e627b6eb5aa57e6b8b6e88c09a7e734b156`
- **Lexer: Numeric Lexeme Content Verification:**
  - Added debug output to `baa_lexer_tester` to print hexadecimal values of `wchar_t` lexemes, confirming that the lexer correctly preserves raw Arabic-Indic digits and Arabic decimal separators. The display issue is console-related.
  - Commit: `9e7db85e2ab57e5ba32c20e46d9b71875fb7cdfe`
- **Documentation Updates:**
  - Updated `docs/LEXER_ROADMAP.md` and `docs/lexer.md` to reflect current status of lexer tasks.
  - Commit: `e4939dc6862db597aba97d3b93701ccb20cce505`
  - Commit: `83efb68e92552606a6f095313625ae19893c6489`

## [0.1.28.0] - 2025-06-20 (Ternary Operator Support)

### Added

- **Preprocessor: Ternary Operator Support (`? :`):**
  - Added support for ternary conditional expressions (`condition ? true_value : false_value`) in preprocessor conditional expressions (`#إذا`, `#وإلا_إذا`).
  - Updated documentation and roadmap to reflect this feature.
  - Example usage and operator details are now included in [`docs/preprocessor.md`](docs/preprocessor.md).

## [0.1.27.0] - 2025-06-13 (Line Number Reporting Bug Fix)

### Fixed

- **Preprocessor: Line Number Reporting in Error Messages (Critical Fix):**
  - Fixed critical bug where all preprocessor errors and warnings incorrectly reported `line 1:1` instead of actual line numbers from the source files.
  - **Root Cause**: The location stack within the preprocessor was never updated during file processing, remaining at the initial position throughout preprocessing.
  - **Fix**: Added `update_current_location()` function to properly maintain the current location during line-by-line processing and enhanced fallback logic in `get_current_original_location()` to provide more accurate location reporting.
  - **Impact**: Error messages now display correct line numbers, dramatically improving debugging experience and making error reporting useful for developers.
  - This fix ensures that preprocessor diagnostics provide meaningful location information for all error and warning scenarios.

### Enhanced

- **Preprocessor: Location Tracking and Error Reporting:**
  - Improved location stack management to track current processing position accurately throughout file processing.
  - Enhanced error reporting fallback mechanisms to provide better location approximations when precise tracking is unavailable.
  - Maintained backward compatibility with existing error reporting interfaces while improving accuracy.

### Technical Details

- **Files Modified:**
  - `src/preprocessor/preprocessor_core.c`: Added `update_current_location()` function and integrated location tracking into main processing loop.
  - `src/preprocessor/preprocessor_utils.c`: Enhanced `get_current_original_location()` function with improved fallback logic.
  - `src/preprocessor/preprocessor_internal.h`: Added function declarations for new location tracking functionality.
- **Standards Compliance**: Improves diagnostic reporting to match expected preprocessor behavior with accurate source location information.
- **Testing**: Bug fix verified through comprehensive testing with various error scenarios across multiple source files.

### Benefits

- **Improved Developer Experience**: Developers can now quickly locate and fix preprocessor errors with accurate line number information.
- **Enhanced Debugging**: Error messages are now actionable with precise source location references.
- **Better IDE Integration**: Accurate location information enables proper error highlighting and navigation in development environments.
- **Increased Productivity**: Eliminates confusion and time wasted trying to locate errors reported at incorrect line numbers.

## [0.1.26.0] - 2025-06-13 (Zero-Parameter Function-Like Macro Bug Fix)

### Fixed

- **Preprocessor: Zero-Parameter Function-Like Macro Expansion (Critical Fix):**
  - Fixed critical bug where zero-parameter function-like macros (e.g., `GET_BASE()`) were expanding incorrectly to `()` instead of their macro body in conditional expressions and all other contexts.
  - **Root Cause**: The `parse_macro_arguments()` function in `src/preprocessor/preprocessor_expansion.c` was returning `NULL` for zero-parameter macros instead of allocating an empty arguments array.
  - **Fix**: Added proper allocation logic for empty arguments array when `named_param_count == 0` and `*actual_arg_count == 0`.
  - **Impact**: Resolves macro expansion failures in conditional expressions like `#إذا IS_EQUAL(GET_BASE(), 42)` where `GET_BASE()` now correctly expands to its defined value instead of empty parentheses.
  - This fix ensures C99-compliant behavior for zero-parameter function-like macros across all preprocessor contexts.

### Enhanced

- **Preprocessor: Macro Expansion Robustness:**
  - Improved argument parsing logic to handle edge cases in function-like macro expansion.
  - Enhanced memory management for macro argument arrays in zero-parameter scenarios.
  - Maintained backward compatibility with existing macro definitions and expansions.

### Technical Details

- **Files Modified:**
  - `src/preprocessor/preprocessor_expansion.c`: Fixed `parse_macro_arguments()` function to properly allocate empty arguments array for zero-parameter macros.
- **Standards Compliance**: Brings zero-parameter function-like macro behavior into full C99 compliance.
- **Testing**: Bug fix verified through comprehensive testing of zero-parameter macros in various contexts including conditional expressions.

### Benefits

- **Fixed Conditional Compilation**: Zero-parameter macros now work correctly in `#إذا` and `#وإلا_إذا` expressions.
- **Improved Standards Compliance**: Aligns Baa preprocessor behavior with C99 standard for function-like macro expansion.
- **Enhanced Reliability**: Eliminates unexpected empty parentheses expansion that could cause compilation errors.
- **Developer Experience**: Developers can now reliably use zero-parameter function-like macros in all contexts without workarounds.

## [0.1.25.0] - 2025-06-04 (Macro Redefinition Warnings/Errors)

### Added

- **Preprocessor: C99-Compliant Macro Redefinition Checking:**
  - Implemented comprehensive macro redefinition validation according to C99 standard (ISO/IEC 9899:1999 section 6.10.3).
  - **Macro Equivalence Detection**: Added sophisticated comparison system that checks macro compatibility by comparing normalized macro bodies and parameter signatures.
  - **Whitespace Normalization**: Implemented intelligent whitespace normalization for macro body comparison that:
    - Strips leading and trailing whitespace
    - Converts multiple consecutive whitespace characters to single spaces
    - Enables proper equivalence checking as per C99 requirements
  - **Parameter List Comparison**: Added parameter signature validation that compares:
    - Function-like vs object-like macro types
    - Parameter count matching
    - Variadic macro status (`وسائط_إضافية`) compatibility
    - C99-compliant behavior where parameter names don't need to match for equivalent macros
  - **Predefined Macro Protection**: Special handling for predefined macros (`__الملف__`, `__السطر__`, `__التاريخ__`, `__الوقت__`, `__الدالة__`, `__إصدار_المعيار_باء__`) with error reporting for redefinition attempts.

### Enhanced

- **Preprocessor: Redefinition Behavior:**
  - **Identical Redefinitions**: Silent acceptance of identical macro redefinitions as per C99 standard
  - **Incompatible Redefinitions**: Warning messages with Arabic text informing users of macro replacement
  - **Predefined Macro Errors**: Error reporting for attempts to redefine built-in macros with rejection of redefinition
  - **Memory Management**: Proper cleanup of macro parameters during redefinition scenarios

### Changed

- **Preprocessor: Macro Management System:**
  - Enhanced `add_macro()` function in `src/preprocessor/preprocessor_macros.c` to include redefinition checking logic
  - Integrated macro comparison with existing diagnostic reporting system
  - Added memory-safe handling of parameter arrays during macro redefinition scenarios
  - **Error Messages**: All redefinition warnings and errors use Arabic language with precise location information

### Added (Implementation Details)

- **New Helper Functions in `src/preprocessor/preprocessor_macros.c`:**
  - `normalize_macro_body()`: Normalizes whitespace in macro bodies for comparison
  - `are_parameter_lists_equivalent()`: Compares macro parameter signatures according to C99 rules
  - `are_macro_bodies_equivalent()`: Compares macro replacement text with normalization
  - `are_macros_equivalent()`: Main equivalence checking function combining parameter and body comparison
  - `is_predefined_macro()`: Identifies built-in macros that should not be redefined

### Fixed

- **Preprocessor: Standards Compliance:**
  - Fixed silent macro replacement behavior that didn't conform to C99 standard requirements
  - Resolved lack of warnings for incompatible macro redefinitions
  - Fixed missing protection for predefined macros against user redefinition

### Technical Details

- **Files Modified:**
  - `src/preprocessor/preprocessor_macros.c`: Complete redefinition checking implementation
  - Enhanced integration with existing error recovery and diagnostic systems
- **C99 Compliance**: Full adherence to ISO/IEC 9899:1999 section 6.10.3 for macro redefinition behavior
- **Memory Safety**: All new functions properly handle memory allocation/deallocation with error recovery
- **Arabic Language Support**: All user-facing messages use Arabic text with proper formatting

### Benefits

- **Standards Compliance**: Brings Baa preprocessor into full C99 compliance for macro redefinition behavior
- **Developer Experience**: Clear warnings help developers identify unintentional macro redefinitions
- **Code Quality**: Prevents silent behavior changes from incompatible macro redefinitions
- **IDE Integration**: Provides proper diagnostic information for language servers and development tools

## [0.1.24.0] - 2025-06-04 (Comprehensive Preprocessor Error Recovery System)

### Added

- **Preprocessor: Comprehensive Error Recovery System:**
  - Implemented robust error recovery mechanisms that allow the preprocessor to continue processing after encountering errors, reporting multiple errors in a single compilation pass.
  - **Diagnostic System**: Added centralized error and warning collection with precise source location tracking (file, line, column).
  - **Error Recovery Utilities**:
    - `skip_to_next_line()`: Basic line-level recovery for malformed directives
    - `find_next_directive()`: Smart synchronization to next preprocessor directive
    - `attempt_directive_recovery()`: Intelligent recovery from directive parsing errors
    - `validate_and_recover_conditional_stack()`: Automatic cleanup of unmatched conditional directives
    - `should_abort_processing()`: Configurable error limits to prevent error flooding
    - `can_continue_after_error()`: Decision logic for recovery vs. abort scenarios
    - `attempt_include_recovery()`: File inclusion error recovery with alternative file extensions
  - **Enhanced Expression Evaluator**: Integrated error recovery into conditional expression evaluation (`#إذا`, `#وإلا_إذا`) with graceful handling of malformed expressions, division by zero protection, and undefined macro handling.
  - **Error Limits and Safeguards**: Configurable error thresholds (default: 25 errors per file, 50 globally) to prevent overwhelming users with error messages while detecting infinite loops and resource exhaustion.

### Enhanced

- **Preprocessor: Core Processing Pipeline:**
  - Updated `process_file()` and `process_string()` functions to integrate error recovery at each processing step
  - Added graceful degradation when errors occur, allowing continued processing of valid code sections
  - Enhanced conditional stack validation at file completion to detect unmatched `#إذا`/`#نهاية_إذا` pairs
- **Preprocessor: Expression Evaluation:**
  - Enhanced conditional expression evaluator with robust error handling for syntax errors, division by zero, and undefined macros
  - Improved error reporting with precise token location information within expressions
  - Added graceful fallback behavior for malformed expressions while maintaining conditional processing integrity
- **Preprocessor: Error Reporting:**
  - All error messages now include precise Arabic error descriptions with file, line, and column information
  - Multiple errors are collected and reported together, improving developer productivity
  - Enhanced error context preservation during recovery operations

### Changed

- **Preprocessor: Architecture:**
  - Refactored error handling throughout the preprocessor to use the new diagnostic accumulation system
  - Modified core processing loops to check error limits and attempt recovery instead of immediate termination
  - Enhanced internal state management to support continued processing after errors
- **Preprocessor: Error Messages:**
  - Standardized all error messages to use Arabic with consistent formatting
  - Improved error location precision for directive arguments and macro invocation errors
  - Added recovery status reporting to inform users when preprocessing continues after errors

### Fixed

- **Preprocessor: Robustness:**
  - Fixed potential crashes from malformed conditional directives by implementing proper error boundaries
  - Resolved issues with unmatched conditional blocks causing preprocessing to continue in wrong state
  - Fixed memory leaks during error recovery by ensuring proper cleanup of partially processed content
- **Preprocessor: Error Handling:**
  - Fixed cascading errors where one error would cause multiple follow-up errors for the same issue
  - Improved handling of circular include detection with better error messages
  - Fixed column number calculation errors in expression evaluation error reporting

### Added (Testing)

- **Comprehensive Test Suite:**
  - `tests/unit/preprocessor/test_error_recovery.c`: Unit tests covering all error recovery scenarios including malformed conditionals, missing endif statements, unknown directives, expression errors, and error count limits
  - `tests/resources/preprocessor_test_cases/error_recovery_test.baa`: Integration test file with various error scenarios to validate recovery behavior
  - Test coverage for continued processing after recoverable errors and validation of error limit mechanisms

### Added (Documentation)

- **Complete Documentation**: `docs/PREPROCESSOR_ERROR_RECOVERY.md` providing comprehensive coverage of:
  - System architecture and design principles
  - Implementation details for all recovery mechanisms
  - Usage examples and integration guidance
  - Testing strategies and methodologies
  - Configuration options and customization
  - Future enhancement roadmap

### Technical Details

- **Files Modified/Added:**
  - `src/preprocessor/preprocessor_utils.c`: Added all error recovery utilities and enhanced diagnostic system
  - `src/preprocessor/preprocessor_core.c`: Integrated error recovery into main processing loops
  - `src/preprocessor/preprocessor_expr_eval.c`: Enhanced with robust error handling and recovery
  - `src/preprocessor/preprocessor_internal.h`: Added function declarations for new error recovery system
  - Multiple test files and comprehensive documentation
- **Impact**: Significantly improves preprocessor robustness and user experience by enabling multiple error reporting and graceful error handling
- **Backward Compatibility**: Fully maintained - existing code continues to work with enhanced error reporting

### Benefits

- **Improved Developer Experience**: Users can see and fix multiple errors at once instead of iterative single-error fixing
- **Enhanced Robustness**: Preprocessor handles malformed code gracefully without crashes or undefined behavior
- **Better IDE Integration**: Comprehensive diagnostic information enables better language server implementation and IDE error display
- **Production Ready**: Memory-safe implementation with proper resource cleanup during error conditions

## [0.1.23.0] - 2025-06-03 (Function-Like Macro Expansion in Conditional Expressions)

### Added

- **Preprocessor: Full Function-Like Macro Expansion in Conditional Expressions:**
  - Implemented complete support for function-like macro expansion within `#إذا` and `#وإلا_إذا` conditional expressions before evaluation.
  - Function-like macros with arguments are now properly expanded, including nested function-like macro calls within arguments.
  - Supports complex macro rescanning scenarios where function-like macros expand to content that contains other macro references.
  - Preserves correct `معرف` (defined) operator behavior - arguments to `معرف` are not macro-expanded before evaluation.
  - **Examples now working:**
    - `#إذا IS_ZERO(ADD_FOR_IF(5, -5))` → expands to `#إذا ((5+-5) == 0)` → evaluates correctly
    - `#إذا IS_GREATER(MAX_SIZE, CURRENT_SIZE)` → expands to `#إذا ((100) > (50))` → evaluates correctly
    - `#إذا NESTED_CALC(5, 3, 2) == 17` → expands to `#إذا ((((5) * (3))) + (2)) == 17` → evaluates correctly

### Fixed

- **Preprocessor: Enhanced Function-Like Macro Processing in Conditionals:**
  - Fixed limitation where function-like macros in conditional expressions were not being fully expanded before expression evaluation.
  - Improved macro expansion pipeline to properly handle complex nested function-like macro scenarios within conditional contexts.
  - Enhanced rescan logic to work correctly within conditional expression evaluation.

### Changed

- **Preprocessor: Conditional Expression Processing:**
  - Modified conditional expression evaluation pipeline to perform full macro expansion (including function-like macros) before parsing and evaluating the expression.
  - Enhanced macro expansion integration within `#إذا`/`#وإلا_إذا` directive processing while maintaining `معرف` operator special handling.

### Known Issues

- **Preprocessor: Zero-Parameter Function-Like Macro Bug:**
  - Zero-parameter function-like macros (e.g., `GET_BASE()`) may expand incorrectly to `()` instead of their macro body in conditional expressions.
  - This affects expressions like `#إذا IS_EQUAL(GET_BASE(), 42)` where `GET_BASE()` should expand to `BASE_VALUE_IF` but instead expands to empty parentheses.
- **Preprocessor: Ternary Operator Support (`? :`):**
  - The expression evaluator does not yet support ternary conditional expressions using `condition ? true_value : false_value` syntax.
  - Expressions containing ternary operators will result in syntax errors during conditional expression evaluation.

### Technical Details

- **Files Modified:**
  - `src/preprocessor/preprocessor_line_processing.c`: Enhanced macro expansion logic for conditional expressions
  - Existing macro expansion infrastructure leveraged for conditional expression contexts
- **Impact**: Significantly improves preprocessor C99 compliance and enables complex macro-based conditional compilation patterns.
- **Backward Compatibility**: Fully maintained - existing simpler conditional expressions continue to work as before.

## [0.1.22.0] - 2025-06-03 (Critical Preprocessor Fix: `معرف` Operator Compliance)

### Fixed

- **Preprocessor: `معرف` (defined) Operator C Standard Compliance (Critical Fix):**
  - Fixed major bug where arguments to the `معرف` operator were incorrectly being macro-expanded before evaluation, violating C standard behavior.
  - The `معرف MACRO_NAME` operator now correctly checks if `MACRO_NAME` is defined as a macro without expanding `MACRO_NAME` first.
  - **Example**: `#إذا معرف POINTER_MACRO` now correctly checks if `POINTER_MACRO` is defined, instead of incorrectly expanding `POINTER_MACRO` to its value and then checking if that value is defined as a macro.
  - This resolves a critical compliance issue that could cause incorrect conditional compilation behavior.
- **Preprocessor: Infinite Rescan Loop Prevention:**
  - Fixed infinite rescan loops that could occur during macro expansion in conditional expressions.
  - Removed erroneous `expansion_occurred_this_pass = true` flag that was causing endless rescanning cycles.
  - Added proper termination conditions to prevent preprocessor hangs.
- **Preprocessor: Expression Evaluator Column Calculation:**
  - Fixed corrupted column numbers in error messages from conditional expression evaluation.
  - Corrected tokenizer reference points to provide accurate error location reporting.
  - Column numbers now correctly point to the problematic token within `#إذا`/`#وإلا_إذا` expressions.
- **Preprocessor: `معرف معرف` Edge Case:**
  - Fixed handling of the edge case `معرف معرف` (checking if "معرف" itself is defined as a macro).
  - Now correctly evaluates to false when `معرف` is not defined as a macro, and true when it is defined as a macro.
  - Maintains proper C standard behavior where `معرف` can appear as both operator and operand.

### Changed

- **Preprocessor: Enhanced Error Recovery:**
  - Improved error handling in conditional expression evaluation to provide more informative error messages.
  - Enhanced diagnostic reporting to maintain processing context during error conditions.

### Technical Details

- **Files Modified:**
  - `src/preprocessor/preprocessor_line_processing.c`: Fixed `معرف` argument preservation logic
  - `src/preprocessor/preprocessor_expr_eval.c`: Fixed column calculation and `معرف معرف` handling
- **Impact**: This fix resolves a fundamental preprocessor compliance issue that could affect any conditional compilation using the `معرف` operator with macro arguments.
- **Backward Compatibility**: Fully maintained - existing code will now behave correctly according to C standards.

## [0.1.21.0] - 2025-05-31 (Conceptual Lexer Updates & Bug Identification)

### Added (Conceptual - Lexer)

- **Lexer: Arabic-Only Exponent Marker for Floats:**
  - The lexer's `scan_number` logic (in `src/lexer/token_scanners.c`) is conceptually updated to recognize `أ` (ALIF WITH HAMZA ABOVE, U+0623) as the exclusive exponent marker for floating-point literals.
  - Support for English `e`/`E` as exponent markers is conceptually removed.
- **Lexer: Arabic Float Suffix `ح`:**
  - `scan_number` logic is conceptually updated to recognize and consume `ح` (HAH, U+062D) as a suffix for floating-point literals. The `ح` becomes part of the `BAA_TOKEN_FLOAT_LIT` lexeme.
- **Lexer: Baa-Specific Arabic Escape Sequences:**

- Scanner functions (`scan_char_literal`, `scan_string`, `scan_multiline_string_literal` in `src/lexer/token_scanners.c`) are conceptually updated to process Baa-specific Arabic escape sequences:
- `\س` for newline (`L'\n'`)
- `\م` for tab (`L'\t'`)
- `\ر` for carriage return (`L'\r'`)
- `\ص` for null character (`L'\0'`)
- `\يXXXX` for 4-digit hex Unicode (replaces `\uXXXX`)
- `\هـHH` for 2-digit hex byte value (Tatweel `ـ` is mandatory after `ه`)
- Standard C-style single-letter escapes (e.g., `\n`, `\t`) and `\uXXXX` are conceptually no longer supported and should generate errors if encountered after `\`.
- Standard `\\`, `\'`, `\"` remain for escaping themselves.

### Changed (Documentation)

- Updated `docs/LEXER_ROADMAP.md`, `docs/language.md`, `docs/arabic_support.md`, `docs/c_comparison.md`, `README.md`, and `docs/architecture.md` to reflect the conceptual shift to Arabic-only exponent markers (`أ`) and Arabic-only escape sequences (`\س`, `\يXXXX`, etc.) for character and string literals in the lexer.

### Identified Issues (Lexer Bugs - To Be Fixed)

- **Keyword Mismatch:** Keyword `وإلا` (else) is incorrectly tokenized as `BAA_TOKEN_IDENTIFIER` instead of `BAA_TOKEN_ELSE`.
- **Float Literal Error:** Input like `.456` (leading dot float) is incorrectly tokenized as `BAA_TOKEN_INT_LIT` instead of `BAA_TOKEN_FLOAT_LIT`.
- **Multiline String Escape Error:** Escape sequences (e.g., `\س`) within multiline string literals (`"""..."""`) are causing an "Unexpected character: '\'" error instead of being processed.
- **Numeric Lexeme Display vs. Content (Verification Needed):** Arabic-Indic digits (e.g., `٠١٢٣`) and the Arabic decimal separator (`٫`) in source appear as Western digits or `?` in the `baa_lexer_tester` output for token lexemes. This needs verification (e.g., hex dump of lexeme) to determine if it's a lexer internal conversion bug or a display issue in the tester/console. The lexer should store raw source characters in the lexeme.

### Known Issues (Carryover)

- Parser and AST components are still under active redesign and re-implementation.
- `number_parser.c` needs updates to correctly interpret new lexemes from `scan_number` (e.g., with `أ` exponent, `ح` suffix, and existing integer suffixes).

## [0.1.20.0] - 2025-05-30

### Added

- **Lexer Tokenization of Whitespace & Comments:**
  - Introduced new token types:
    - `BAA_TOKEN_WHITESPACE`: For sequences of spaces and/or tabs. Lexeme contains the exact whitespace sequence.
    - `BAA_TOKEN_NEWLINE`: For newline characters (`\n`, `\r`, `\r\n`). Lexeme is the normalized or actual sequence.
    - `BAA_TOKEN_SINGLE_LINE_COMMENT`: For `//` comments. Lexeme contains the comment content (after `//`, excluding trailing newline). Token location points to the start of `//`.
    - `BAA_TOKEN_MULTI_LINE_COMMENT`: For `/* ... */` comments (non-documentation). Lexeme contains the content between `/*` and `*/`. Token location points to the start of `/*`.
  - `BAA_TOKEN_DOC_COMMENT` (for `/** ... */`) now consistently has its lexeme as content-only, with token location pointing to the start of `/**`.
  - Updated `baa_token_type_to_string` to include string representations for these new token types.
- **New Lexer Scanners (`src/lexer/token_scanners.c`):**
  - `scan_whitespace_sequence()`: Implemented to scan and tokenize contiguous spaces and tabs.
  - `scan_single_line_comment()`: Implemented to scan and tokenize the content of `//` comments.
  - `scan_multi_line_comment()`: Implemented to scan and tokenize the content of `/* ... */` comments.

### Changed

- **Lexer Core (`src/lexer/lexer.c` -> `baa_lexer_next_token`):**
  - Major refactor to no longer skip whitespace or comments.
  - Now dispatches to specific scanner functions to create tokens for `BAA_TOKEN_WHITESPACE`, `BAA_TOKEN_NEWLINE`, `BAA_TOKEN_SINGLE_LINE_COMMENT`, `BAA_TOKEN_MULTI_LINE_COMMENT`, and `BAA_TOKEN_DOC_COMMENT`.
  - The internal `skip_whitespace` function was removed; its whitespace-only skipping logic is now part of the initial checks in `baa_lexer_next_token` before dispatching to `scan_whitespace_sequence` or handling newlines directly.
- **Lexer Scanners for Comments (`src/lexer/token_scanners.c`):**
  - `scan_single_line_comment`, `scan_multi_line_comment`, and `scan_doc_comment` now create tokens manually, ensuring their lexemes contain only the comment *content* (delimiters excluded) and their reported token location (line/column) corresponds to the start of the comment delimiter sequence (`//`, `/*`, `/**`). They use the lexer's internal `append_char_to_buffer` utility for building content.
- **`scan_char_literal` (`src/lexer/token_scanners.c`):**
  - Updated to consume its own opening delimiter (`'`) as it's now called when `peek(lexer)` is at the delimiter.
  - Modified to produce a token whose lexeme is the *processed character value* (e.g., for `'\n'`, the lexeme is a 1-char string containing the newline character), consistent with string literal tokenization. Token location points to the opening `'`.

### Fixed

- Ensured consistent handling of token location reporting for comment tokens, pointing to the start of their respective delimiters.
- Improved robustness of `baa_file_content` in `src/utils/utils.c` for reading UTF-16LE files, particularly around BOM handling and file size calculation. (This was a prerequisite fix identified during lexer testing).
- Enhanced `tools/baa_lexer_tester.c` to better handle file input and display Unicode output on Windows consoles for improved testing.

### Known Issues

- The parser does not yet handle these new whitespace/comment tokens; it will need significant updates to either filter them or incorporate them into its grammar rules if necessary.

## [0.1.19.0] - 2025-05-26

### Added

- **Parser Core Foundation (Phase 0 of Parser Redesign):**
  - Defined internal `BaaParser` state structure in `src/parser/parser_internal.h`. This structure includes:
    - A pointer to the `BaaLexer` instance.
    - `current_token` and `previous_token` (of type `BaaToken`) to manage the token stream.
    - `source_filename` (const char*) to store the name of the file being parsed for accurate error reporting.
    - Error state flags: `had_error` and `panic_mode`.
  - Implemented the public parser API in `include/baa/parser/parser.h`:
    - `BaaParser* baa_parser_create(BaaLexer* lexer, const char* source_filename)` for parser initialization. It fetches the first token.
    - `void baa_parser_free(BaaParser* parser)` for resource cleanup, including freeing owned token lexemes.
    - `BaaNode* baa_parse_program(BaaParser* parser)` as a stub function (actual parsing logic is pending).
    - `bool baa_parser_had_error(const BaaParser* parser)` to check for parsing errors.
  - Implemented core token consumption and management logic within `src/parser/parser.c` (as static internal functions):
    - `advance()`: Consumes `current_token`, makes the next token from the lexer current, manages `previous_token`, and takes ownership of the new `current_token`'s lexeme. Reports lexical errors encountered during token fetching from the lexer.
    - `check_token()`: Peeks at the type of `current_token` without consuming it.
    - `match_token()`: Consumes `current_token` if its type matches the expected type.
    - `consume_token()`: Expects `current_token` to be of a specific type; if so, consumes it. If not, reports a syntax error via `parser_error_at_token`.
  - Implemented parser error handling and basic recovery mechanisms in `src/parser/parser.c` (as static internal functions):
    - `parser_error_at_token()`: Centralized error reporting. Prints an error message to `stderr`, including source filename, line, and column of the problematic token. Sets `had_error` and `panic_mode`. Prevents cascading errors if already in panic mode.
    - `synchronize()`: Basic "panic mode" error recovery. When active, it discards tokens until a likely synchronization point (e.g., statement terminator '.', common statement-starting keywords, or EOF) is reached, then clears `panic_mode`.
  - Added the `baa_parser` static library component to the CMake build system (`src/parser/CMakeLists.txt`).
  - Integrated the new `baa_parser` library into `baa_compiler_lib` dependencies in `src/CMakeLists.txt`.

### Changed

- **BaaToken (`include/baa/lexer/lexer.h`):**
  - Modified `BaaToken.lexeme` from `const wchar_t*` to `wchar_t*`. This change allows the parser to take ownership of the lexeme string memory that was allocated by the lexer.
- **Tools (`tools/baa_parser_tester.c`):**
  - Updated to utilize the new parser core API (`baa_parser_create`, `baa_parser_free`).
  - The tester now demonstrates basic token iteration through the parser's internal state for observational purposes, as `baa_parse_program` is currently a stub. AST parsing and printing sections are commented out.

## [0.1.18.0] - 2025-05-25

### Added

- **AST Redesign (Phase 0 & Initial Phase 1):**
  - Introduced new core AST type definitions in `include/baa/ast/ast_types.h`:
    - `BaaSourceLocation` and `BaaSourceSpan` for precise source code location tracking.
    - Unified `BaaNodeKind` enum for all AST node types (initial kinds: `_UNKNOWN`, `_PROGRAM`, `_LITERAL_EXPR`).
    - Base `BaaNode` structure with `kind`, `span`, and `void* data`.
    - `BaaAstNodeModifiers` typedef and initial modifier flags (`BAA_MOD_NONE`, `BAA_MOD_CONST`, `BAA_MOD_STATIC`).
  - Implemented basic AST node lifecycle functions in `src/ast/ast_node.c` and `include/baa/ast/ast.h`:
    - `BaaNode* baa_ast_new_node(BaaNodeKind kind, BaaSourceSpan span)` for generic node creation.
    - `void baa_ast_free_node(BaaNode* node)` shell for recursive node deallocation.
  - Implemented Literal Expression AST Nodes (`BAA_NODE_KIND_LITERAL_EXPR`):
    - Defined `BaaLiteralKind` enum and `BaaLiteralExprData` struct in `include/baa/ast/ast_types.h`.
    - Implemented `BaaNode* baa_ast_new_literal_int_node(...)` and `BaaNode* baa_ast_new_literal_string_node(...)` in `src/ast/ast_expressions.c` (prototypes in `include/baa/ast/ast.h`).
    - Implemented internal helper `baa_ast_free_literal_expr_data(...)` in `src/ast/ast_expressions.c` (prototype in `src/ast/ast_expressions.h`).
    - Updated `baa_ast_free_node` dispatch to handle `BAA_NODE_KIND_LITERAL_EXPR`.
  - Added new `baa_ast` static library component to CMake (`src/ast/CMakeLists.txt`).
- **AST Tester Tool:**
  - Added `tools/baa_ast_tester.c` for incrementally testing AST node creation and memory management.
  - Integrated `baa_ast_tester` into the root `CMakeLists.txt`.

### Changed

- **Build System:**
  - `baa_ast_tester` now links against `baa_types` library due to usage of type system functions.
- **AST Internal Headers:**
  - `src/ast/ast_node.c` now includes `ast_expressions.h` (internal header) to resolve declarations for specific AST data freeing functions.

### Fixed

- Resolved linker errors for `baa_ast_tester` by adding `baa_types` to its linked libraries.
- Resolved compilation error in `src/ast/ast_node.c` for undeclared `baa_ast_free_literal_expr_data` by including the internal `ast_expressions.h`.

### Known Issues (AST/Parser Redesign)

- The new Parser is not yet implemented; AST nodes are currently only created manually in `baa_ast_tester`.
- Only a few AST node types (`UNKNOWN`, `LITERAL_EXPR`) are implemented. The vast majority are pending.
- Full recursive freeing logic in `baa_ast_free_node` depends on specific `_data` freeing helpers for all node types, most of which are not yet implemented.
- Type-safe accessor macros for AST node data are planned but not yet implemented.

## [0.1.17.0] - 2025-05-21

### Added

- **CMake Build System:**
  - Introduced `cmake/BaaCompilerSettings.cmake` module to centralize common compile definitions (`UNICODE`, `_UNICODE`, `_CRT_SECURE_NO_WARNINGS`) via an interface library `BaaCommonSettings`.
  - Added CMake policies (CMP0074, CMP0067, CMP0042) to the root `CMakeLists.txt` for modern CMake behavior.
  - Added enforcement for out-of-source builds in the root `CMakeLists.txt`.

- **Preprocessor Error Recovery Foundation:**
  - Implemented internal mechanisms to support accumulating multiple preprocessor errors and warnings instead of halting on the first one. (Files affected: `src/preprocessor/preprocessor_internal.h`, `src/preprocessor/preprocessor.c`, `src/preprocessor/preprocessor_utils.c`).
  - Added `PreprocessorDiagnostic` struct to store individual diagnostic details (message, location).
  - Added `add_preprocessor_diagnostic()` function to accumulate diagnostics within the `BaaPreprocessor` state.
  - The main `baa_preprocess()` function now collects all reported diagnostics and can return them (currently concatenated into the output `error_message` parameter if errors occurred).

### Changed

- **CMake Build System Refactoring (Major):**
  - **Target-Centric Approach:**
    - Removed global `include_directories()` and `add_definitions()` from the root `CMakeLists.txt`.
    - Compile definitions and include directories are now applied per-target using `target_compile_definitions()` and `target_include_directories()`.
  - **Executable Linking:**
    - Main `baa` executable and tool executables (`baa_lexer_tester`, `baa_preprocessor_tester`) now link against component static libraries (e.g., `baa_utils`, `baa_lexer`, `baa_preprocessor`) instead of compiling all sources directly.
    - Created `baa_compiler_lib` static library from `src/compiler.c`. The main `baa` executable now links `baa_compiler_lib`, which in turn links other necessary component libraries.
  - **Component Library Dependencies:**
    - Each component library (in `src/<component>/CMakeLists.txt`) now explicitly declares its dependencies on other `baa_*` libraries using `target_link_libraries()`.
    - Component libraries now link the `BaaCommonSettings` interface library to inherit common compile definitions.
  - **LLVM Handling in CMake:**
    - LLVM-specific source compilation, include directories, compile definitions (`LLVM_AVAILABLE`), and linking of LLVM libraries are now handled within `src/codegen/CMakeLists.txt` for the `baa_codegen` target, based on `USE_LLVM` option and `LLVM_FOUND` status from the root `CMakeLists.txt`.
  - **src/CMakeLists.txt:**
    - Ensured `add_subdirectory()` is present for all components, including `preprocessor`.
    - Adjusted order of `add_subdirectory()` calls for better clarity.
    - Added definition for `baa_compiler_lib`.

- **Preprocessor Error Reporting:**
  - Modified the core structure of error reporting within the preprocessor to use the new diagnostic accumulation system. This is a foundational step for allowing the preprocessor to continue and report multiple errors.
  - Enhanced column number calculation for errors originating within `#تعريف` directive argument parsing (e.g., for missing macro name or malformed parameters) to improve location precision. (Files affected: `src/preprocessor/preprocessor_directives.c`).
  - Improved column number calculation for errors occurring during the parsing of function-like macro invocation arguments (e.g., mismatched parentheses, missing commas within the argument list). (Files affected: `src/preprocessor/preprocessor_expansion.c`).
- **Preprocessor Testing:** Added (commented-out) test cases to `tests/resources/preprocessor_test_cases/preprocessor_test_all.baa` for testing error location precision for directive arguments and macro invocation arguments.

### Known Issues (Preprocessor)

- **Full Error Recovery Implementation:** While the foundation for accumulating multiple errors is in place, most error-handling sites in the preprocessor still need to be updated to fully utilize this by attempting synchronization and continuation instead of immediately halting their specific task or causing the whole preprocessor to stop. This is an ongoing task detailed in `docs/PREPROCESSOR_ROADMAP.md`.
- **`معرف` Operator Argument Expansion:** The argument to the `معرف` (defined) operator within `#إذا`/`#وإلا_إذا` expressions is still (pending further specific fix and verification) being macro-expanded before `معرف` evaluates it.
- **Error/Warning Location Precision (Remaining Areas):** Further refinement for precise column reporting is an ongoing effort.
- **Full Macro Expansion in Conditional Expressions:** Still pending.
- **Token Pasting (`##`) during Rescanning (Complex Cases):** Still pending.

### Planned (Next Steps for Preprocessor Error Recovery)

- Systematically refactor error handling in:
  - Directive parsing (`preprocessor_directives.c` for `#تضمين`, `#تعريف` body, etc.).
  - Macro expansion logic (`preprocessor_expansion.c`, `preprocessor_line_processing.c`).
  - Conditional expression evaluation (`preprocessor_expr_eval.c`) to robustly handle evaluation failures while maintaining conditional stack integrity.
- Implement clear synchronization strategies (e.g., skip to end of directive line, attempt to find matching `#نهاية_إذا`) after reporting a recoverable error.

## [0.1.16.0] - 2025-05-20

### Changed

- **Preprocessor Internals:** Refactored aspects of the conditional expression evaluator (`#إذا`, `#وإلا_إذا`) to improve the precision of column number tracking for tokens within the expression string. This is a step towards more accurate error reporting for syntax errors occurring inside conditional expressions. (Files affected: `src/preprocessor/preprocessor_internal.h`, `src/preprocessor/preprocessor_expr_eval.c`, `src/preprocessor/preprocessor_directives.c`).
- **Preprocessor Testing:** Updated `tests/resources/preprocessor_test_cases/preprocessor_test_all.baa` with more structured sections and added (commented-out) test cases for detailed error location precision checking in conditional expressions, directive arguments, and macro invocation arguments.

### Known Issues (Preprocessor)

- **`معرف` Operator Argument Expansion:** The argument to the `معرف` (defined) operator within `#إذا`/`#وإلا_إذا` expressions is currently being macro-expanded before `معرف` evaluates it, which is incorrect according to C standards. This can lead to evaluation errors if the expanded macro is not an identifier.
- **Error/Warning Location Precision (Directive Arguments & Macro Calls):** While improvements were made for conditional expressions, precise column reporting for errors within directive arguments (e.g., `#تعريف NAME BODY`) and during macro call argument parsing is still an area for ongoing refinement.
- **Full Macro Expansion in Conditional Expressions:** Full expansion of function-like macros and their rescanned results *within* `#إذا`/`#وإلا_إذا` expressions before evaluation is not yet complete.
- **Token Pasting (`##`) during Rescanning:** Complex interactions of `##` when it appears as part of a macro expansion output, or when its operands are complex macros, may not be fully robust.

## [0.1.15.0] - 2025-05-19

### Added

- **Preprocessor:** Implemented `#خطأ "message"` (error) directive. This directive halts preprocessing and reports the specified message as a fatal error. (Files affected: `src/preprocessor/preprocessor_directives.c`, `src/preprocessor/preprocessor_utils.c`, `src/preprocessor/preprocessor_internal.h`).
- **Preprocessor:** Implemented `#تحذير "message"` (warning) directive. This directive prints the specified message to `stderr` and preprocessing continues. (Files affected: `src/preprocessor/preprocessor_directives.c`, `src/preprocessor/preprocessor_utils.c`, `src/preprocessor/preprocessor_internal.h`).
- **Preprocessor:** Conditional expression evaluator (`#إذا`, `#وإلا_إذا`) now supports hexadecimal (`0x...`) and binary (`0b...`) integer literals. (File affected: `src/preprocessor/preprocessor_expr_eval.c`).

### Changed

- **Preprocessor:** Updated the value of the predefined macro `__إصدار_المعيار_باء__` to reflect current development (e.g., to `10150L` - adjust as per your versioning scheme). (File affected: `src/preprocessor/preprocessor.c`).
- **Documentation:** Updated `README.md`, `docs/language.md`, and `docs/PREPROCESSOR_ROADMAP.md` to reflect new preprocessor features and current status.

### Known Issues (Preprocessor)

- **Error/Warning Location Precision:** Line and column numbers reported for errors/warnings originating from within directives (e.g., `#إذا`, `#تحذير`) may not always point to the precise location in the original source file.
- **Full Macro Expansion in Conditional Expressions:** Full expansion of function-like macros and their rescanned results *within* `#إذا`/`#وإلا_إذا` expressions before evaluation is not yet complete.
- **Token Pasting (`##`) during Rescanning:** Complex interactions of `##` when it appears as part of a macro expansion output, or when its operands are complex macros, may not be fully robust.

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
- **Lexer:** Updated keyword list in `src/lexer/lexer.c` to use `لكل` for `for` and `استمر` for `continue` to match documentation.
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
