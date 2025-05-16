# Baa Language Lexer Roadmap (Comprehensive)

This roadmap outlines the planned improvements and current status of the Baa language lexer implementation.

**Note:** The lexer operates on source code that has **already been processed** by the `baa_preprocess` function. Directives like `#تضمين` and `#تعريف` are handled in that separate step before the lexer sees the code.

## 1. Core Lexer Functionality

- [x] Basic token recognition (`baa_lexer_next_token`).
- [x] Lexer initialization and management (`BaaLexer` struct, `baa_init_lexer`).
- [x] Source code navigation and tracking (current position, start of token).
- [x] Line and column tracking.
- [x] Basic error token creation (`make_error_token`, `BAA_TOKEN_ERROR`).
- [x] Token memory management (`make_token` allocates, `baa_free_token` frees).
- [ ] **Future:** Add support for more token types for language extensions if Baa evolves significantly.

## 2. Token Types (Feature Completion)

- **Comments:**
  - [x] Single line comments (`//`) - Skipped.
  - [x] Multi-line comments (`/* ... */`) - Skipped.
  - [x] Documentation comments (`/** ... */`) - Tokenized as `BAA_TOKEN_DOC_COMMENT`.
- **Literals:**
  - [x] Identifiers (`BAA_TOKEN_IDENTIFIER`) - Basic Arabic/ASCII support.
  - [x] Integer Literals (`BAA_TOKEN_INT_LIT`):
    - [x] Decimal, Hexadecimal (`0x`), Binary (`0b`).
    - [x] Arabic-Indic digits.
    - [x] Underscores for readability.
    - [x] Arabic Suffixes (`غ`, `ط`, `طط`, combinations) - Lexer recognizes; parser/semantic analyzer interprets.
  - [x] Floating-Point Literals (`BAA_TOKEN_FLOAT_LIT`):
    - [x] Decimal representation with `.` or `٫`.
    - [x] Scientific notation (currently `e`/`E`).
    - [ ] Implement Baa Arabic float suffix `ح`.
    - [ ] Implement Baa Arabic exponent marker `أ` (e.g., `1.23أ4`) in `scan_number`.
    - [ ] Support C99 hexadecimal float constants (e.g., `0x1.fp+2`), adapting to Baa's `أ` exponent marker if applicable.
  - [x] Character Literals (`BAA_TOKEN_CHAR_LIT`):
    - [x] Standard C escape sequences (`\n`, `\t`, `\\`, `\'`, `\"`, `\r`, `\0`).
    - [x] Unicode escape sequences (`\uXXXX`).
    - [ ] Implement Baa-specific Arabic escape sequences (e.g., `\س` newline, `\م` tab, `\يXXXX` Unicode) using `\` as the escape character in `scan_char_literal`.
  - [x] String Literals (`BAA_TOKEN_STRING_LIT`):
    - [x] Standard C escape sequences.
    - [x] Unicode escape sequences (`\uXXXX`).
    - [x] Multiline strings (`"""..."""`).
    - [x] Raw string literals (`خ"..."`, `خ"""..."""`).
    - [ ] Implement Baa-specific Arabic escape sequences (e.g., `\س`, `\م`, `\يXXXX`) in `scan_string` and `scan_multiline_string_literal`.
  - [x] Boolean Literals (`BAA_TOKEN_BOOL_LIT`): `صحيح`, `خطأ`.
- **Keywords:**
  - [x] All current Baa keywords tokenized correctly (e.g., `إذا`, `لكل`, `ثابت`, `مضمن`, `مقيد`, type names).
  - [x] `دالة` and `متغير` are no longer keywords.
- **Operators & Delimiters:**
  - [x] All standard operators and delimiters tokenized.

## 3. Number Parsing (`src/lexer/number_parser.c` Interaction)

- [x] Integer parsing (decimal, hex, binary).
- [x] Float parsing (decimal, basic scientific).
- [x] Arabic-Indic digit support in lexemes.
- [x] Underscores in number lexemes.
- [ ] **Crucial Next Step:** Ensure `number_parser.c` (functions like `baa_parse_number`) can correctly interpret the full lexemes produced by `scan_number`, including:
  - [ ] Integer suffixes (`غ`, `ط`, `طط`) to determine actual type (unsigned, long, etc.) and value.
  - [ ] Float suffix `ح` (once implemented in lexer).
  - [ ] Arabic exponent marker `أ` (once implemented in lexer).
  - [ ] Hexadecimal float components (once implemented in lexer).

## 4. String & Character Literal Value Processing

- [x] Lexer's `scan_string`, `scan_char_literal`, `scan_multiline_string_literal` process standard C and `\uXXXX` escapes, storing the resulting value in `token->lexeme`.
- [ ] When Baa-specific Arabic escape sequences are implemented, ensure these scanner functions correctly process them into their corresponding character values.

## 5. Arabic Language Support & Unicode (Lexer-specific)

- [x] Arabic letter recognition for identifiers (`is_arabic_letter`).
- [x] Arabic-Indic digit support in numbers (`is_arabic_digit`).
- [x] Arabic keyword recognition.
- **Deeper Unicode Support for Identifiers:**
  - [ ] Research and define precise Unicode character categories allowed for Baa identifiers beyond current `is_arabic_letter` and `iswalpha` (e.g., based on UAX #31 for identifiers).
  - [ ] Update `scan_identifier` and character utility functions accordingly.
- **Unicode Normalization (Consideration):**
  - [ ] Evaluate if Unicode normalization (NFC, NFD, etc.) is needed for identifiers to handle canonically equivalent sequences. (Likely not for initial versions due to complexity).
- **Bidirectional Control Characters:**
  - [ ] Define lexer policy: Should Unicode bidirectional control characters (LRM, RLM, LRE, PDF, etc.) be preserved in string/char literals, or discarded? (Typically preserved).
  - [ ] Ensure they don't interfere with tokenization of other elements.

## 6. Error Handling and Reporting

- [x] Basic error reporting via `make_error_token` which stores a formatted `wchar_t*` message in `token->lexeme`.
- **More Specific Error Tokens/Messages:**
  - [ ] Consider introducing more specific error token types (e.g., `BAA_TOKEN_ERROR_UNTERMINATED_STRING`, `BAA_TOKEN_ERROR_INVALID_NUMBER_SUFFIX`) or an error code field within `BAA_TOKEN_ERROR` tokens.
  - [ ] Enhance error messages to provide suggestions where applicable (e.g., "Invalid escape sequence `\ق`. Did you mean `\س`?").
- **Enhanced Synchronization (`synchronize` function):**
  - [ ] Review and improve robustness of the `synchronize()` function.
  - [ ] Consider context-sensitive synchronization (e.g., different behavior if error occurs inside a string vs. a number vs. general code).
- **Maximum Error Count:**
  - [ ] Implement a mechanism in `BaaLexer` or `baa_lexer_next_token` to stop lexing and report a "too many errors" message after a configurable threshold to prevent error cascades.

## 7. Performance Optimizations (Longer-Term)

- **Keyword Lookup:**
  - [ ] Evaluate current linear scan in `scan_identifier`. If performance becomes an issue with more keywords, consider:
    - [ ] Hash table lookup.
    - [ ] Perfect hash function (e.g., generated by `gperf`).
    - [ ] Trie or DAWG structure.
- **String Interning:**
  - [ ] For identifiers and string literals, implement a string interning mechanism to store unique strings once, reducing memory and allowing faster comparisons. `BaaToken->lexeme` would point to interned strings.
- **Buffered File I/O:**
  - [ ] If lexing very large files becomes a requirement, investigate a streaming/buffered approach for `baa_file_content` or lexer input, instead of reading the entire file into memory.
- **Optimized Character Classification:**
  - [ ] Review performance of `is_arabic_digit`, `is_arabic_letter`, etc. If they become bottlenecks, consider lookup-table based classification for relevant Unicode ranges.

## 8. Lexer State and Configuration (Future)

- **Lexer Modes/States:**
  - [ ] (Not currently needed for Baa) If language features like complex string interpolation are added, the lexer might require explicit state management.
- **Configurable Lexer Options:**
  - [ ] (Not currently needed) If Baa develops dialects or version-specific lexical rules, `BaaLexer` could hold configuration flags.

## 9. Token Structure (`BaaToken`) Considerations

- **Value Storage:**
  - [x] String/Char literals: Lexer processes escapes and stores the final value in `token->lexeme`.
  - [ ] Numeric literals: Lexer captures the full textual lexeme. `number_parser.c` is responsible for converting this to a numeric value and interpreting suffixes/exponents. Ensure this division of responsibility is clear and `number_parser.c` is updated for all numeric literal features.
- **Source Location Precision (`BaaSourceLocation` in AST, `line`/`column` in Token):**
  - [x] Tokens store start line/column.
  - [ ] Review if end line/column or token length in `BaaToken` itself would simplify parser or error reporting logic, or if start location is sufficient. (Currently seems sufficient).

## 10. Integration with Preprocessor

- [x] Lexer operates on preprocessed UTF-16LE output.
- **Source Mapping:**
  - [ ] Investigate robust handling of source location mapping if the preprocessor outputs `#line` directives (or similar custom markers) to indicate original file/line changes due to includes or complex macros. The lexer (or a layer managing it) would need to interpret these to provide accurate locations for tokens originating from different files/macro expansions.

## 11. Testing and Validation

- [x] `baa_lexer_tester.c` tool for basic token stream verification.
- **Unit Test Coverage:**
  - [ ] Develop more granular unit tests for individual scanner functions (`scan_number`, `scan_string`, `scan_identifier`, `scan_char_literal`, etc.) with a wide variety of valid and invalid inputs.
  - [ ] Test character utility functions in `lexer_char_utils.c` thoroughly.
- **Integration Tests:**
  - [x] Use `tests/resources/lexer_test_cases/lexer_test_suite.baa` for end-to-end lexing tests.
  - [ ] Expand test suite to cover all new features and edge cases identified.
- **Error Case Testing:**
  - [ ] Systematically add test cases for all expected lexical errors and verify the error messages and recovery.
- **Fuzz Testing:**
  - [ ] Implement or use a fuzz testing framework to feed the lexer with random/malformed inputs to uncover crashes or unexpected behavior.
- **Benchmark Suite:**
  - [ ] Develop benchmarks to measure lexer performance on various input sizes and types.

## 12. File Handling (Primarily `src/utils/utils.c`)

- [x] `baa_file_content` reads UTF-16LE (with BOM skipping).
- [ ] `baa_read_file` (older utility) reads and converts from MBSTOWCS. Review if this is still needed or if all source input should be strictly UTF-16LE for the lexer after preprocessing.
- [ ] Input source abstraction (file, string, stdin) - `BaaLexer` currently takes `const wchar_t* source`. If `stdin` or other stream-like inputs are needed, this interface might need to adapt or be wrapped.

## Implementation Priorities (High-Level View)

1. **Complete Core Language Features (Token Types section):**
    - Arabic float suffix `ح` and exponent `أ`.
    - C99 Hexadecimal Floats.
    - Baa-specific Arabic escape sequences for char/string literals.
2. **Ensure `number_parser.c` Compatibility:** Update `number_parser.c` to fully interpret all lexemes produced by the enhanced `scan_number`.
3. **Enhance Error Handling & Reporting:** Focus on more specific errors, better messages, and robust synchronization.
4. **Improve Testing:** Expand unit tests, fuzz testing.
5. **Address Deeper Unicode Support:** For identifiers, as requirements become clearer.
6. **Performance Optimizations:** As needed, based on profiling.
