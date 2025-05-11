# Baa Language Lexer Roadmap

This roadmap outlines the planned improvements and current status of the Baa language lexer implementation.

**Note:** The lexer now operates on source code that has **already been processed** by the `baa_preprocess` function. Directives like `#تضمين` and `#تعريف` are handled in that separate step before the lexer sees the code.

## Core Lexer Functionality

- [x] Basic token recognition
- [x] Lexer initialization and management
- [x] Source code navigation and tracking
- [x] Line and column tracking
- [x] Error token creation
- [x] Token memory management
- [ ] Add support for more token types for language extensions (Future)

## Token Types

- [ ] Special tokens (Comment - Defined, but comments currently skipped, not tokenized. See Comment Support section)
- [x] Literals (Identifier - Basic Arabic/ASCII support)
- [x] Literals (Int - `BAA_TOKEN_INT_LIT`)
- [x] Literals (Int): Add support for Baa Arabic suffixes (`غ` for unsigned, `ط` for long, `طط` for long long, and combinations like `غط`, `غطط`). (Lexer part done; parser/type system to interpret them)
- [x] Literals (Float - Defined `BAA_TOKEN_FLOAT_LIT`, identified by lexer syntax checks; value parsing occurs later)
- [ ] Literals (Float): Add support for Baa Arabic suffix `ح` and Arabic exponent marker `أ` (e.g., `1.23أ4`).
- [ ] Literals (Float): Add support for C99 hexadecimal float constants (e.g., `0x1.fp+2`), adapting to Baa's `أ` exponent marker if applicable.
- [x] Literals (Char - Defined `BAA_TOKEN_CHAR_LIT`, currently scans standard C escapes)
- [ ] Literals (Char): Implement Baa Arabic escape sequences (e.g., `\س`, `\م`, `\يXXXX`) using `\` as the escape character.
- [x] Literals (String - `BAA_TOKEN_STRING_LIT`, currently scans standard C escapes)
- [ ] Literals (String): Implement Baa Arabic escape sequences (e.g., `\س`, `\م`, `\يXXXX`) using `\` as the escape character.
- [x] Literals (Boolean - Defined `BAA_TOKEN_BOOL_LIT`, keywords `صحيح`/`خطأ` scanned)
- [x] **Keywords Aligned**: `دالة` (FUNC) and `متغير` (VAR) are not Baa keywords. Baa uses C-style function declarations and type-prefixed variable declarations.
- [ ] **Add C99 Keywords**: Support `مضمن` (inline) and `مقيد` (restrict) (Tokenization for these Arabic keywords).
- [x] Types (TYPE_INT, TYPE_FLOAT, TYPE_CHAR, TYPE_VOID, TYPE_BOOL - Defined and added to `lexer.c` keyword list)
- [x] Operators (+, -, *, /, %, =, ==, !, !=, <, <=, >, >=)
- [x] Operators (&&, || - Defined and scanning logic added in `lexer.c`)
- [x] Compound assignment operators (+=, -=, *=, /=, %=)
- [x] Increment/decrement operators (++, --)
- [x] Delimiters ((, ), {, }, [, ], ,, ., ;, :)

## Number Parsing

- [x] Integer parsing
- [x] Decimal number parsing
- [x] Number error handling
- [x] Arabic-Indic digit support
- [x] Scientific notation support (using `أ` as exponent marker).
- [x] Binary (`0b`/`0B`) and hexadecimal (`0x`/`0X`) literals (prefixes unchanged).
- [x] Underscore in number literals (e.g., 1_000_000)

## String Handling

- [x] Basic string literal support (double quotes)
- [x] Character literal support (single quotes)
- [x] Basic escape sequences in strings/chars (currently standard C; planned to transition to Baa Arabic escapes like `\س`, `\م`).
- [x] Unicode escape sequences (currently `\uXXXX`; planned Baa equivalent `\يXXXX`).
- [x] Multiline strings (`"""..."""`)
- [x] Raw string literals (e.g., `خ"..."`, `خ"""..."""` - no escape processing)

## Comment Support

- [x] Single line comments (`#` style - Skipped, not tokenized)
- [x] Single line comments (`//` style - Skipped, not tokenized)
- [x] Multi-line comments (`/* ... */` - Skipped, not tokenized)
- [x] Documentation comments (`/** ... */` - Tokenized as `BAA_TOKEN_DOC_COMMENT`)

## Arabic Language Support

- [x] Arabic letter recognition
- [x] Arabic-Indic digit support
- [x] Arabic keyword recognition
- [ ] Improve Unicode support for identifiers (beyond basic letters)
- [ ] Implement better whitespace and comment handling for RTL text
- [ ] Right-to-left text handling improvements (Visual/Editor concern mostly)
- [ ] Arabic error messages (Only number parser errors are currently Arabic)
- [ ] Visual error highlighting for Arabic text (Editor integration?)
- [ ] Arabic language directives (If any remain after preprocessing)

## Preprocessing

- [x] **Handled by external `baa_preprocess` function:**
  - [x] Include directives (`#تضمين`)
  - [x] Basic Macro definitions (`#تعريف` - parameterless)
  - [x] File inclusion logic
  - [x] Conditional compilation (`#إذا_عرف`, etc. - *handled by preprocessor*)

## Error Handling and Reporting

- [x] Basic error reporting
- [ ] Detailed error messages with context
- [ ] Error recovery (Improve synchronization points)
- [ ] Warning system
- [ ] Suggestions for common errors
- [ ] Error categorization and filtering

## Performance Optimizations

- [ ] Keyword lookup table optimization
- [ ] Token memory pooling
- [ ] String interning
- [ ] Buffered file reading
- [ ] Parallel lexing for large files

## Testing and Validation

- [ ] Unit test coverage
- [ ] Integration tests with parser
- [ ] Benchmark suite
- [ ] Fuzz testing
- [ ] Edge case handling

## File Handling

- [x] Basic file content reading
- [x] File size determination
- [ ] Streaming large files
- [ ] Input source abstraction (file, string, stdin)
- [ ] UTF-8 to UTF-16 conversion (Currently assumes UTF-16LE input)

## Implementation Priorities (Revised after recent updates)

1. **Enhance Error Reporting and Recovery:** *[Done]*
    - Provide more detailed error messages with context.
    - Implement basic error recovery to continue lexing after simple errors.

2. **Preprocessor Development (External):** *[In Progress]*
    - Basic `#تضمين` and `#تعريف` handled externally by `baa_preprocess`.
    - **Next steps (for preprocessor):** Conditional compilation (`#إذا_عرف`, etc.).

3. **Testing:**
    - Develop comprehensive unit tests for lexer functionality.
    - Add integration tests with the parser.

4. **Advanced Features:**
    - [x] Underscores in number literals (`1_000`). (*Marked as done, was completed earlier*)
    - [x] Multiline strings (`"""..."""`)
    - [x] Raw string literals (using `خ"..."` and `خ"""..."""`)
    - [x] Documentation comments (`/** ... */`).

5. **Performance Optimizations:**
    - Keyword lookup optimization.
    - String interning.

6. **[Misc]** Implement remaining basic escape sequences (`\r`, `\0` in strings, `\"` in chars). *[Done]*
