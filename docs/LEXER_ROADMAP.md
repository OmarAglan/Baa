# Baa Language Lexer Roadmap (Comprehensive)

This roadmap outlines the planned improvements and current status of the Baa language lexer implementation.

**Note:** The lexer operates on source code that has **already been processed** by the `baa_preprocess` function. Directives like `#تضمين` and `#تعريف` are handled in that separate step before the lexer sees the code.

## 1. Core Lexer Functionality

* [x] Basic token recognition (`baa_lexer_next_token`).
* [x] Lexer initialization and management (`BaaLexer` struct, `baa_init_lexer`).
* [x] Source code navigation and tracking (current position, start of token).
* [x] Line and column tracking.
* [x] Basic error token creation (`make_error_token`, `BAA_TOKEN_ERROR`).
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
  * [ ] Floating-Point Literals (`BAA_TOKEN_FLOAT_LIT`):
    * [x] Decimal representation with `.` or Arabic `٫` (U+066B). (v0.1.10.0) - *Note: Lexer correctly captures `٫`, display issue in tester.*
    * [x] Float literal starting with `.` (e.g., `.456`). (v0.1.21.0)
    * [x] Scientific notation (now uses `أ`, `e`/`E` removed). (Conceptual: `أ` implemented, `e`/`E` removed)
    * [x] Underscores for readability. (v0.1.10.0)
    * [x] Implement Baa Arabic float suffix `ح`. (Conceptual: Implemented)
    * [ ] Support C99 hexadecimal float constants (e.g., `0x1.fp+2`), adapting to Baa's `أ` exponent marker. (Pending)
  * [x] Character Literals (`BAA_TOKEN_CHAR_LIT`):
    * [x] Unicode escape sequences (`\uXXXX` removed; now `\يXXXX`). (Conceptual: Implemented `\ي`, removed `\u`)
    * [x] Implement Baa-specific Arabic escape sequences (`\س`, `\م`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`) using `\` as the escape character. (Conceptual: Implemented)
  * [ ] String Literals (`BAA_TOKEN_STRING_LIT`):
    * [x] Standard C escape sequences (now replaced by Baa Arabic equivalents).
    * [x] Unicode escape sequences (`\uXXXX` removed; now `\يXXXX`). (Conceptual: Implemented `\ي`, removed `\u`)
    * [ ] Multiline strings (`"""..."""`). (v0.1.11.0) - *Known Bug: Escape sequences within multiline strings are not correctly processed, leading to "Unexpected character: '\'" errors. Lexer also incorrectly tokenizes the opening `"""` as an empty string literal.*
    * [x] Raw string literals (`خ"..."`, `خ"""..."""`), including single-line newline error handling. (v0.1.11.0)
    * [ ] Implement Baa-specific Arabic escape sequences (`\س`, `\م`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`) in `scan_string` and `scan_multiline_string_literal`. (Conceptual: Implemented, but bug in multiline string processing)
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
* [ ] **Crucial Next Step:** Ensure `number_parser.c` (functions like `baa_parse_number`) can correctly interpret the full lexemes produced by `scan_number`, including:
  * [ ] Integer suffixes (`غ`, `ط`, `طط`) to determine actual type (unsigned, long, etc.) and value. (Lexer tokenizes, parser needs to interpret).
  * [ ] Float suffix `ح` (once implemented in lexer).
  * [ ] Arabic exponent marker `أ` (now conceptually implemented in lexer).
  * [ ] Hexadecimal float components (once implemented in lexer).

## 4. String & Character Literal Value Processing

* [x] Lexer's `scan_string`, `scan_char_literal`, `scan_multiline_string_literal`, `scan_raw_string_literal` process standard C and `\uXXXX` escapes, storing the resulting value in `token->lexeme`.
* [x] Baa-specific Arabic escape sequences are now conceptually implemented in scanner functions to correctly process them into their corresponding character values.

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

## 6. Error Handling and Reporting

* [x] Basic error reporting via `make_error_token` which stores a formatted `wchar_t*` message in `token->lexeme`.
* [x] User-facing lexer error messages are in Arabic (e.g., from `number_parser.c`, `token_scanners.c`). (v0.1.12.0 and earlier)
* [x] Clarified error message for invalid escape sequences in string literals. (v0.1.10.0)
* [x] Improved error reporting for multiline strings by tracking start line/column. (v0.1.11.0)
* **More Specific Error Tokens/Messages:**
  * [ ] Consider introducing more specific error token types (e.g., `BAA_TOKEN_ERROR_UNTERMINATED_STRING`, `BAA_TOKEN_ERROR_INVALID_NUMBER_SUFFIX`) or an error code field within `BAA_TOKEN_ERROR` tokens.
  * [ ] Enhance error messages to provide suggestions where applicable (e.g., "Invalid escape sequence `\ق`. Did you mean `\س`?").
* **Enhanced Synchronization (`synchronize` function):**
  * * The current `synchronize` in `lexer.c` is quite basic.
  * [ ] Review and improve robustness of the `synchronize()` function.
  * [ ] Consider context-sensitive synchronization (e.g., different behavior if error occurs inside a string vs. a number vs. general code).
* **Maximum Error Count:**
  * [ ] Implement a mechanism in `BaaLexer` or `baa_lexer_next_token` to stop lexing and report a "too many errors" message after a configurable threshold to prevent error cascades.

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
* [ ] Numeric literals: Lexer captures the full textual lexeme. `number_parser.c` converts this. Ensure `number_parser.c` is updated for all planned numeric literal features (suffixes `ح`, exponent `أ`, hex floats).
* [x] Source Location Precision: Tokens store start line/column.
* [ ] **Enhance Token Location:** Add full source span (`BaaSourceSpan` or similar with start/end line/col) to `BaaToken` for more precise error reporting and tooling. This involves updating `BaaToken` struct and `make_token` logic.

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
3. **Enhance Error Handling & Reporting:** Focus on more specific errors, better messages, and robust synchronization.
4. **Improve Testing:** Expand unit tests, fuzz testing.
5. **Address Deeper Unicode Support:** For identifiers, as requirements become clearer.
6. **Performance Optimizations:** As needed, based on profiling.

## Known Issues / Bugs to Fix (from recent testing)
   **[BUG] Multiline String Escape Error:** Backslash (`\`) for Baa escapes (e.g., `\س`) within multiline strings is causing an "Unexpected character: '\'" error. Escape processing in `scan_multiline_string_literal` needs review. Additionally, the lexer incorrectly tokenizes the opening `"""` as an empty string literal.
   **[VERIFY] Numeric Lexeme Content:** Arabic-Indic digits (e.g., `٠١٢٣`) and the Arabic decimal separator (`٫`) in source code appear as Western digits or `?` in the `baa_lexer_tester`'s printout of token lexemes. This has been verified to be a display issue in the tester tool/console; the lexer correctly preserves the raw source characters in `BaaToken.lexeme`.
