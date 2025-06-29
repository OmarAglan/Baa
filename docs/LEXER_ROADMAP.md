# Baa Language Lexer Roadmap (Comprehensive)

This roadmap outlines the planned improvements and current status of the Baa language lexer implementation.

**Note:** The lexer operates on source code that has **already been processed** by the `baa_preprocess` function. Directives like `#تضمين` and `#تعريف` are handled in that separate step before the lexer sees the code.

## 1. Core Lexer Functionality

* [x] Basic token recognition (`baa_lexer_next_token`).
* [x] Lexer initialization and management (`BaaLexer` struct, `baa_init_lexer`).
* [x] Source code navigation and tracking (current position, start of token).
* [x] Line and column tracking.
* [x] ~~Basic error token creation (`make_error_token`, `BAA_TOKEN_ERROR`).~~ **REPLACED**
* [x] **Enhanced error handling system** (`make_specific_error_token`, specific error types). ✅ **COMPLETED**
* [x] Token memory management (`make_token` allocates, `baa_free_token` frees).
* [x] Tokenize whitespace and newlines instead of skipping.
* [ ] **Future:** Add support for more token types for language extensions if Baa evolves significantly.
* [ ] **Future:** Configurable tab width for column calculation in `advance()`. (Roadmap Section 8 refinement)

## 2. Token Types (Feature Completion)

* **Comments:**
  * [x] Single line comments (`//`) - Tokenized as `BAA_TOKEN_SINGLE_LINE_COMMENT` (content only). (v0.1.20.0)
  * [x] Multi-line comments (`/* ... */`) - Tokenized as `BAA_TOKEN_MULTI_LINE_COMMENT` (content only, non-doc). (v0.1.20.0)
  * [x] Documentation comments (`/** ... */`) - Tokenized as `BAA_TOKEN_DOC_COMMENT`. (v0.1.11.0)
* **Literals:**
  * [x] Identifiers (`BAA_TOKEN_IDENTIFIER`) - Basic Arabic/ASCII support.
  * [x] Integer Literals (`BAA_TOKEN_INT_LIT`):
    * [x] Decimal, Hexadecimal (`0x`/`0X`), Binary (`0b`/`0B`). (v0.1.10.0)
    * [x] Arabic-Indic digits (`٠-٩`). (v0.1.10.0)
    * [x] Underscores (`_`) for readability. (v0.1.10.0)
    * [x] Arabic Suffixes (`غ`, `ط`, `طط`, combinations like `غط`, `ططغ`) - Lexer tokenizes as part of `BAA_TOKEN_INT_LIT`. (v0.1.13.0)
  * [x] Floating-Point Literals (`BAA_TOKEN_FLOAT_LIT`):
    * [x] Decimal representation with `.` or Arabic `٫` (U+066B). (v0.1.10.0)
    * [x] Float literal starting with `.` (e.g., `.456`). (Working)
    * [x] Scientific notation (now uses `أ`, `e`/`E` removed). (Implemented and working)
    * [x] Underscores for readability. (v0.1.10.0)
    * [x] Baa Arabic float suffix `ح`. (Implemented and working)
    * [x] Support C99 hexadecimal float constants (e.g., `0x1.fp+2`), adapting to Baa's `أ` exponent marker. (Implemented and working)
  * [x] Character Literals (`BAA_TOKEN_CHAR_LIT`):
    * [x] Unicode escape sequences (`\uXXXX` removed; now `\يXXXX`). (Conceptual: Implemented `\ي`, removed `\u`)
    * [x] Implement Baa-specific Arabic escape sequences (`\س`, `\م`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`) using `\` as the escape character. (Implemented and working)
  * [x] String Literals (`BAA_TOKEN_STRING_LIT`):
    * [x] Standard C escape sequences (now replaced by Baa Arabic equivalents).
    * [x] Unicode escape sequences (`\uXXXX` removed; now `\يXXXX`). (Conceptual: Implemented `\ي`, removed `\u`)
    * [x] Multiline strings (`"""..."""`). (v0.1.31.0) - Fixed bug where the opening `"""` delimiter was incorrectly tokenized as an empty string literal.
    * [x] Raw string literals (`خ"..."`, `خ"""..."""`), including single-line newline error handling. (v0.1.31.0) - Fixed bug where the opening `خ"""` delimiter was incorrectly tokenized.
    * [x] Implement Baa-specific Arabic escape sequences (`\س`, `\م`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`) in `scan_string` and `scan_multiline_string_literal`. (Working - v0.1.32.0)
0.1.32.0)
  * [x] Boolean Literals (`BAA_TOKEN_BOOL_LIT`): `صحيح`, `خطأ`.
* **Keywords:**
  * [x] All current Baa keywords tokenized correctly (e.g., `إذا`, `لكل`, `ثابت`).
  * [x] C99 keywords `مضمن` (inline) and `مقيد` (restrict) added. (v0.1.13.0)
  * [x] `دالة` and `متغير` are no longer keywords (tokenized as identifiers). (v0.1.13.0)
* **Operators & Delimiters:**
  * [x] All standard operators and delimiters tokenized.

## 3. Number Parsing (`src/lexer/number_parser.c` Interaction)

* [x] Integer parsing (decimal, hex, binary) by `number_parser.c`.
* [x] Float parsing (decimal, basic scientific) by `number_parser.c`.
* [x] Arabic exponent marker `'أ'` implemented for scientific notation, replacing `'e'/'E'`.
* [x] Arabic-Indic digit support in lexemes passed to `number_parser.c`.
* [x] Underscores in number lexemes passed to `number_parser.c`.
* [x] **Completed:** `number_parser.c` (functions like `baa_parse_number`) correctly interprets the full lexemes produced by `scan_number`, including:
  * [x] Integer suffixes (`غ`, `ط`, `طط`) to determine actual type (unsigned, long, etc.) and value.
  * [x] Float suffix `ح` (implemented and working in lexer).
  * [x] Arabic exponent marker `أ` (implemented and working in lexer).
  * [x] Hexadecimal float components (implemented and working in lexer).

## 4. String & Character Literal Value Processing

* [x] Lexer's `scan_string`, `scan_char_literal`, `scan_multiline_string_literal`, `scan_raw_string_literal` process standard C and `\uXXXX` escapes, storing the resulting value in `token->lexeme`.
* [x] Baa-specific Arabic escape sequences are implemented and working in scanner functions to correctly process them into their corresponding character values.

## 5. Arabic Language Support & Unicode (Lexer-specific)

* [x] Arabic letter recognition for identifiers (`is_arabic_letter`).
* [x] Arabic-Indic digit support in numbers (`is_arabic_digit`).
* [x] Arabic keyword recognition.
* **Deeper Unicode Support for Identifiers:**
  * [ ] Research and define precise Unicode character categories allowed for Baa identifiers beyond current `is_arabic_letter` and `iswalpha` (e.g., based on UAX #31 for identifiers).
  * [ ] Update `scan_identifier` and character utility functions accordingly.
* **Unicode Normalization (Consideration):**
  * [ ] Evaluate if Unicode normalization (NFC, NFD, etc.) is needed for identifiers to handle canonically equivalent sequences. (Likely not for initial versions due to complexity).
* **Bidirectional Control Characters:**
  * [ ] Define lexer policy: Should Unicode bidirectional control characters (LRM, RLM, LRE, PDF, etc.) be preserved in string/char literals, or discarded? (Typically preserved).
  * [ ] Ensure they don't interfere with tokenization of other elements.

## 6. Error Handling and Reporting ✅ **MAJOR ENHANCEMENT COMPLETED**

### ✅ Completed Features (Enhanced Error System)

* [x] ~~Basic error reporting via `make_error_token`~~ **REPLACED with enhanced system**
* [x] **Specific Error Token Types**: Implemented 8 distinct error types:
  * `BAA_TOKEN_ERROR_UNTERMINATED_STRING` (Code: 1001)
  * `BAA_TOKEN_ERROR_INVALID_ESCAPE` (Code: 1002)
  * `BAA_TOKEN_ERROR_UNTERMINATED_CHAR` (Code: 1003)
  * `BAA_TOKEN_ERROR_INVALID_CHARACTER` (Code: 1004)
  * `BAA_TOKEN_ERROR_INVALID_NUMBER` (Code: 1005)
  * `BAA_TOKEN_ERROR_INVALID_SUFFIX` (Code: 1006)
  * `BAA_TOKEN_ERROR_UNTERMINATED_COMMENT` (Code: 1007)
  * `BAA_TOKEN_ERROR` (Code: 9001) for memory/system errors
* [x] **Enhanced Error Context**: Rich error information with:
  * Unique error codes for internationalization
  * Error categories ("string", "escape", "character", "number", "comment", "memory", "operator")
  * Arabic suggestions for fixing errors
  * Enhanced source location tracking
* [x] **Complete Arabic Localization**: All error messages and suggestions in Arabic
* [x] **Legacy Function Removal**: Eliminated deprecated `make_error_token` function
* [x] **Complete Migration**: All 48 error generation points converted to enhanced system
* [x] **Memory Management**: Proper cleanup of error contexts and enhanced structures
* [x] **Invalid Suffix Validation**: Comprehensive validation for number suffixes (غغ, طططط, etc.)

### 🔄 Ongoing/Future Enhancements

* **Enhanced Synchronization (`synchronize` function):**
  * [x] Current `synchronize` in `lexer.c` works with enhanced error system
  * [ ] Review and improve robustness of the `synchronize()` function
  * [ ] Consider context-sensitive synchronization (e.g., different behavior if error occurs inside a string vs. a number vs. general code)
* **Maximum Error Count:**
  * [ ] Implement a mechanism in `BaaLexer` or `baa_lexer_next_token` to stop lexing and report a "too many errors" message after a configurable threshold to prevent error cascades
* **Source Context Extraction:**
  * [ ] Add source code snippets to error context for better IDE integration

## 7. Performance Optimizations (Longer-Term)

* **Keyword Lookup:**
  * [x] Current linear scan in `scan_identifier` (via `keywords` array in `lexer.c`).
  * [ ] If performance becomes an issue, consider: Hash table, Perfect hash function, Trie/DAWG.
* **String Interning:**
  * [ ] For identifiers and string literals, implement string interning.
* **Buffered File I/O:**
  * [ ] If lexing very large files becomes a requirement, investigate streaming/buffered input.
* **Optimized Character Classification:**
  * [ ] Review performance of `is_arabic_digit`, `is_arabic_letter`, etc. Consider lookup tables if bottlenecks.

## 8. Lexer State and Configuration (Future)

* [ ] (Not currently needed) Lexer Modes/States for complex string interpolation.
* [ ] (Not currently needed) Configurable Lexer Options for dialects/versions.

## 9. Token Structure (`BaaToken`) Considerations

* [x] String/Char literals: Lexer processes escapes and stores the final value in `token->lexeme`.
* [x] Comment tokens: Lexer stores content only (delimiters excluded) in `token->lexeme`.
* [x] Numeric literals: Lexer captures the full textual lexeme. `number_parser.c` converts this and supports all numeric literal features (suffixes `ح`, exponent `أ`, hex floats).
* [x] Source Location Precision: Tokens store start line/column.
* [x] **Enhanced Token Location:** Added full source span (`BaaSourceSpan` with start/end line/col and character offsets) to `BaaToken` for precise error reporting and tooling. ✅ **COMPLETED**

## 10. Integration with Preprocessor

* [x] Lexer operates on preprocessed UTF-16LE output.
* **Source Mapping:**
  * [ ] Investigate robust handling of source location mapping if the preprocessor outputs `#line` directives (or similar) to relate preprocessed code back to original files/lines from includes or complex macros.
*     * This would involve the lexer parsing `#line num "filename"` and updating its internal line/filename tracking.

## 11. Testing and Validation

* [x] Standalone `baa_lexer_tester.c` tool. (v0.1.13.0)
* [x] Comprehensive lexer test suite: `tests/resources/lexer_test_cases/lexer_test_suite.baa`. (v0.1.10.0)
* **Unit Test Coverage:**
  * [ ] Develop more granular unit tests for individual scanner functions (`scan_number`, `scan_string`, etc.).
  * [ ] Thoroughly test character utility functions in `lexer_char_utils.c`.
* **Integration Tests:**
  * [ ] Expand `lexer_test_suite.baa` for all new features and edge cases.
* **Error Case Testing:**
  * [ ] Systematically add test cases for all expected lexical errors.
* **Fuzz Testing:**
  * [ ] Implement or use a fuzz testing framework.
* **Benchmark Suite:**
  * [ ] Develop benchmarks for lexer performance.

## 12. File Handling (Primarily `src/utils/utils.c`)

* [x] `baa_file_content` (in utils) reads UTF-16LE. Preprocessor uses `read_file_content` which handles UTF-8 and UTF-16LE. Lexer receives UTF-16LE.
* * [ ] Review `BaaLexer` struct: Consider adding `const wchar_t* current_filename_for_reporting;` if `#line` directives are handled, for accurate token location.
* [ ] Review `baa_read_file` (in utils) usage/necessity if all pre-lexer input is handled by preprocessor's `read_file_content`.

## Implementation Priorities (High-Level View)

1. **Complete Core Language Features (Token Types section):**
    * Arabic float suffix `ح` and exponent `أ`.
    * C99 Hexadecimal Floats.
    * Baa-specific Arabic escape sequences for char/string literals.
2. **Ensure `number_parser.c` Compatibility:** Update `number_parser.c` to fully interpret all lexemes produced by the enhanced `scan_number`.
3. ~~**Enhance Error Handling & Reporting:** Focus on more specific errors, better messages, and robust synchronization.~~ ✅ **COMPLETED**
4. **Improve Testing:** Expand unit tests, fuzz testing.
5. **Address Deeper Unicode Support:** For identifiers, as requirements become clearer.
6. **Performance Optimizations:** As needed, based on profiling.

## 🎉 Major Recent Achievement: Enhanced Error Handling System

### ✅ Complete Error Handling Overhaul (Latest Update)

**MASSIVE IMPROVEMENT**: Successfully implemented a comprehensive enhanced error handling system that transforms the lexer's error reporting capabilities:

#### **Migration Statistics:**
- **48 Error Calls Converted**: All `make_error_token` → `make_specific_error_token`
- **8 Specific Error Types**: Replaced generic errors with precise error classification
- **9 Error Codes**: Unique codes (1001-1009, 9001) for internationalization
- **6 Error Categories**: Organized by type (string, escape, character, number, comment, memory, operator)
- **48 Arabic Suggestions**: Actionable advice for fixing each error type

#### **Enhanced Features:**
- **Rich Error Context**: Error codes, categories, suggestions, enhanced location tracking
- **Complete Arabic Localization**: All error messages and suggestions in Arabic
- **Memory Management**: Proper cleanup of error contexts and enhanced token structures
- **Invalid Suffix Validation**: Comprehensive validation for number suffixes (غغ, طططط, etc.)
- **Legacy Cleanup**: Removed deprecated `make_error_token` function entirely

#### **Developer Experience Improvements:**
- **Precise Error Identification**: Specific error types instead of generic errors
- **Actionable Suggestions**: Helpful Arabic suggestions for fixing errors
- **Better IDE Integration**: Enhanced source spans with start/end positions
- **Consistent Error Handling**: All error generation uses the same enhanced system

This enhancement represents a fundamental improvement in the lexer's error handling capabilities, providing developers with significantly better error diagnostics and suggestions when working with the Baa programming language.

---

## Known Issues / Bugs to Fix (from recent testing)
   **[RESOLVED] Numeric Lexeme Content:** Arabic-Indic digits (e.g., `٠١٢٣`) and the Arabic decimal separator (`٫`) in source code appear as Western digits or `?` in the `baa_lexer_tester`'s printout of token lexemes. This has been verified to be a display issue in the tester tool/console only; the lexer correctly preserves the raw source characters in `BaaToken.lexeme`.
