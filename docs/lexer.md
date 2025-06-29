# Baa Lexer Documentation

## Overview

The Baa lexer, also known as a lexical analyzer or scanner, is a fundamental component of the Baa compiler. It is responsible for taking the raw Baa source code (which has **already been processed by the Baa Preprocessor**) and converting it into a sequence of meaningful units called **tokens**. These tokens are then passed to the Baa Parser for syntactic analysis.

The lexer operates on a UTF-16LE wide character stream, which is the output format of the Baa Preprocessor. It is designed with strong support for Arabic, including the ability to tokenize Arabic identifiers, keywords, and various literal forms.

## Features

### 1. Tokenization

The primary role of the lexer is to identify and categorize sequences of characters from the input stream into distinct tokens. Each token typically has:

* A **type** (e.g., identifier, keyword, integer literal, operator).
* A **lexeme** (the actual string of characters from the source that forms the token).
* **Source location information** (line number and starting column number).
  * Unlike many traditional lexers that skip whitespace and comments, the Baa lexer
  * tokenizes these elements. This provides a richer token stream that can be useful
  * for tools like formatters, linters, or IDEs that require full source fidelity.
  * The parser will then be responsible for deciding how to handle or ignore these tokens.

### 2. Arabic Language Support

* **Identifiers:** Recognizes identifiers composed of Arabic letters (from standard Unicode ranges for Arabic), English letters, Arabic-Indic digits (`٠`-`٩`), ASCII digits (`0-9`), and underscores (`_`).
* **Keywords:** Identifies reserved Arabic keywords (e.g., `إذا`, `لكل`, `ثابت`, `مضمن`, `مقيد`).
* **Numeric Literals:**
  * Parses integers in decimal, hexadecimal (`0x`/`0X`), and binary (`0b`/`0B`) formats.
  * Supports Arabic-Indic digits within all parts of numeric literals.
  * Allows underscores (`_`) as separators for readability in numbers.
  * Recognizes Arabic integer literal suffixes (`غ` for unsigned, `ط` for long, `طط` for long long, and their combinations like `غط`). The lexer includes these in the token's lexeme (implemented and working).
  * Recognizes Arabic float suffix `ح` for floating-point literals (implemented and working).
  * Parses floating-point numbers using `.` or the Arabic decimal separator `٫`.
  * Scientific notation for floats uses the Arabic exponent marker `'أ'` (implemented and working), replacing `'e'/'E'`.
  * Supports hexadecimal float constants (e.g., `0x1.aأ+2`), which must include a hexadecimal part (integer or fractional) and an exponent part prefixed with `أ` (implemented and working).
* **String and Character Literals:**
  * Handles standard double-quoted strings (`"..."`), multiline triple-quoted strings (`"""..."""`), and raw strings (prefixed with `خ`, e.g., `خ"..."`, `خ"""..."""`).
  * Handles single-quoted character literals (`'...'`).
  * Processes Baa-specific Arabic escape sequences (`\س`, `\م`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`) using `\` as the escape character. (Standard C-style escapes like `\n`, `\t`, `\uXXXX` are replaced by their Arabic equivalents). All escape sequences work correctly in regular strings, character literals, and multiline strings.
  * Fixed issues with tokenizing opening delimiters for multiline strings (`"""`) and raw strings (`خ"""`) in v0.1.31.0.
  * Enhanced escape sequence handling for multiline strings in v0.1.32.0 - now working correctly.

### 3. Comment Handling

* **Tokenizes all comment types**, preserving their content (excluding delimiters) as the token's lexeme. The token's location (line/column) points to the start of the comment delimiter.
* **`BAA_TOKEN_SINGLE_LINE_COMMENT`**: For comments starting with `//`. The lexeme contains the text after `//` up to the newline.
* Example: `// هذا تعليق` -> `Token(SINGLE_LINE_COMMENT, " هذا تعليق", line, col_of_slash)`
* **`BAA_TOKEN_MULTI_LINE_COMMENT`**: For comments enclosed in `/* ... */` (that are not doc comments). The lexeme contains the text between `/*` and `*/`.
* Example: `/* تعليق \n متعدد الأسطر */` -> `Token(MULTI_LINE_COMMENT, " تعليق \n متعدد الأسطر ", line, col_of_slash_star)`
* **`BAA_TOKEN_DOC_COMMENT`**: For documentation comments enclosed in `/** ... */`. The lexeme contains the text between `/**` and `*/`.
* Example: `/** تعليق توثيقي. */` -> `Token(DOC_COMMENT, " تعليق توثيقي. ", line, col_of_first_slash)`
* Note: Preprocessor directives (lines starting with `#`) are handled *before* the lexer stage.

### 3.5 Whitespace and Newline Handling

* **`BAA_TOKEN_WHITESPACE`**: Tokenizes sequences of one or more spaces and/or tabs. The lexeme contains the exact whitespace sequence.
* Example: ` \t ` -> `Token(WHITESPACE, "  \t ", line, col)`
* **`BAA_TOKEN_NEWLINE`**: Tokenizes newline sequences (`\n`, `\r`, `\r\n`). The lexeme is the actual sequence encountered (e.g., `"\n"` or `"\r\n"`), and the token's location points to the start of this sequence. Line and column tracking are updated accordingly by the lexer.

### 4. Enhanced Error Handling System

The Baa lexer features a comprehensive error handling system that provides specific error types, detailed error information, and helpful suggestions for fixing errors.

#### 4.1 Specific Error Token Types

Instead of generic `BAA_TOKEN_ERROR` tokens, the lexer now generates specific error types:

* **`BAA_TOKEN_ERROR_UNTERMINATED_STRING`** (Code: 1001): Missing closing quote in string literals
* **`BAA_TOKEN_ERROR_INVALID_ESCAPE`** (Code: 1002): Invalid escape sequences in strings/characters
* **`BAA_TOKEN_ERROR_UNTERMINATED_CHAR`** (Code: 1003): Missing closing quote in character literals
* **`BAA_TOKEN_ERROR_INVALID_CHARACTER`** (Code: 1004): Invalid characters (e.g., newlines in char literals)
* **`BAA_TOKEN_ERROR_INVALID_NUMBER`** (Code: 1005): Invalid number formats (e.g., `0x` without digits)
* **`BAA_TOKEN_ERROR_INVALID_SUFFIX`** (Code: 1006): Invalid literal suffixes (e.g., `غغ`, `طططط`)
* **`BAA_TOKEN_ERROR_UNTERMINATED_COMMENT`** (Code: 1007): Missing closing `*/` in comments
* **`BAA_TOKEN_ERROR`** (Code: 9001): Memory allocation errors and other system errors

#### 4.2 Enhanced Error Information

Each error token includes rich contextual information through the `BaaErrorContext` structure:

```c
typedef struct {
    unsigned int error_code;        // Unique error code for internationalization
    const char* category;          // Error category ("string", "number", etc.)
    const wchar_t* suggestion;     // Helpful suggestion in Arabic
    const wchar_t* context_before; // Source context before error (optional)
    const wchar_t* context_after;  // Source context after error (optional)
} BaaErrorContext;
```

#### 4.3 Arabic Error Messages and Suggestions

All error messages and suggestions are provided in Arabic:

* **Unterminated String**: `"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة"` (Add a double quote " at the end of the string)
* **Invalid Escape**: `"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX"` (Use a valid escape sequence like \س or \م or \يXXXX)
* **Invalid Suffix**: `"استخدم لاحقة غ واحدة فقط للأعداد غير المُوقعة"` (Use only one غ suffix for unsigned numbers)

#### 4.4 Error Recovery and Synchronization

* Includes robust error synchronization to continue tokenizing after errors
* Attempts to recover gracefully from lexical errors
* Provides precise source location information for all errors

### 5. Modular Structure

The lexer implementation is organized into several files:

* `lexer.c`: Core dispatch logic (`baa_lexer_next_token`), keyword table, and general helper functions.
* `token_scanners.c`: Contains specific functions for scanning different categories of tokens (e.g., `scan_identifier`, `scan_number`, `scan_string`).
* `lexer_char_utils.c`: Provides utility functions for character classification (e.g., `is_arabic_letter`, `is_arabic_digit`).
* `number_parser.c`: A utility (often used in conjunction with the lexer) for converting the textual representation of numbers (lexemes) into actual numeric values and handling various numeric formats and potential errors during conversion.

## Lexer State and Token Structures

### `BaaLexer` (Lexer State)

Holds the current state of the lexical analysis:

```c
typedef struct {
    const wchar_t* source;   // Source code (UTF-16LE) being lexed
    size_t source_length;  // Length of the source string
    size_t start;           // Start index of the current token in 'source'
    size_t current;         // Current index/position in 'source'
    size_t line;           // Current line number (1-based)
    size_t column;         // Current column number on the line (0-based for internal advance logic, reported 1-based)
} BaaLexer;
```

### `BaaToken` (Enhanced Token Structure)

Represents a single token with enhanced error handling capabilities:

```c
typedef struct {
    BaaTokenType type;       // Enum identifying the token's type (including specific error types)
    const wchar_t* lexeme;   // Pointer to the string of characters forming the token (dynamically allocated)
    // For string, char, and comment tokens, this is the *processed content*.
    // For other tokens (keywords, identifiers, numbers, operators), this is the raw source text.
    // For error tokens, this contains the Arabic error message.
    size_t length;           // Length of the lexeme (content length for processed types)
    size_t line;            // Line number where the token begins
    size_t column;          // Column number where the token begins
    BaaSourceSpan span;     // Enhanced source location tracking
    BaaErrorContext* error; // Error-specific information (NULL for non-error tokens)
} BaaToken;
```

#### Enhanced Source Location (`BaaSourceSpan`)

```c
typedef struct {
    size_t start_line;      // Starting line number (1-based)
    size_t start_column;    // Starting column number (1-based)
    size_t end_line;        // Ending line number (1-based)
    size_t end_column;      // Ending column number (1-based)
    size_t start_offset;    // Character offset from source start
    size_t end_offset;      // Character offset from source start
} BaaSourceSpan;
```

A comprehensive list of `BaaTokenType` values can be found in `include/baa/lexer/lexer.h`.

## Usage

1. **Initialization:**
    An instance of `BaaLexer` is initialized using `baa_init_lexer()`, providing it with the preprocessed source code (as a `wchar_t*`) and the source filename (for error reporting context).

    ```c
    #include "baa/lexer/lexer.h"
    #include "baa/utils/utils.h" // For baa_file_content or similar

    // wchar_t* preprocessed_source = ... (obtained from preprocessor)
    // const wchar_t* source_filename = L"my_program.ب";
    BaaLexer lexer_instance;
    baa_init_lexer(&lexer_instance, preprocessed_source, source_filename);
    ```

2. **Token Processing:**
    Tokens are retrieved sequentially by calling `baa_lexer_next_token()` in a loop until a `BAA_TOKEN_EOF` (End Of File) token is encountered or an unrecoverable error occurs. Each call returns a pointer to a dynamically allocated `BaaToken`, which must be freed by the caller using `baa_free_token()`.

    ```c
    BaaToken* token;
    do {
        token = baa_lexer_next_token(&lexer_instance);
        if (!token) { /* Handle memory allocation error for token */ break; }

        // Process token:
        // wprintf(L"Type: %ls, Lexeme: '%.*ls', Line: %zu, Col: %zu\n",
        //         baa_token_type_to_string(token->type),
        //         (int)token->length, token->lexeme,
        //         token->line, token->column);

        // Enhanced error handling with specific error types
        if (baa_token_is_error(token)) {
            // fwprintf(stderr, L"Lexical Error [%u]: %ls\n",
            //          token->error->error_code, token->lexeme);
            // if (token->error->suggestion) {
            //     fwprintf(stderr, L"Suggestion: %ls\n", token->error->suggestion);
            // }
        }

        BaaTokenType current_type = token->type; // Store before freeing
        baa_free_token(token);
        if (current_type == BAA_TOKEN_EOF || current_type == BAA_TOKEN_ERROR) {
            break;
        }
    } while (true);
    ```

## Recent Major Improvements

### ✅ Enhanced Error Handling System (Completed)

* **Specific Error Types**: Implemented 8 specific error token types instead of generic errors
* **Rich Error Context**: Added error codes, categories, and Arabic suggestions
* **Complete Migration**: All 48 error generation points now use the enhanced system
* **Memory Management**: Proper cleanup of error contexts and enhanced token structures
* **Arabic Localization**: All error messages and suggestions provided in Arabic

## Future Improvements and Roadmap Items

* Enhanced error recovery mechanisms with configurable error limits
* Source context extraction for better error reporting
* Further Unicode support for identifiers based on UAX #31
* Performance optimizations (e.g., for keyword lookup, string interning) as needed
* Robust source mapping if preprocessor outputs `#line` directives

*For a detailed list of ongoing tasks and future plans, refer to `docs/LEXER_ROADMAP.md`.*
