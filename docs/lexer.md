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
  * Recognizes Arabic integer literal suffixes (`غ` for unsigned, `ط` for long, `طط` for long long, and their combinations like `غط`). The lexer includes these in the token's lexeme.
  * Parses floating-point numbers using `.` or the Arabic decimal separator `٫`.
  * Scientific notation for floats now uses the Arabic exponent marker `'أ'`, replacing `'e'/'E'`.
  * Support for hexadecimal float constants (e.g., `0x1.fp+2`) using Baa's `أ` exponent marker is planned.
* **String and Character Literals:**
  * Handles standard double-quoted strings (`"..."`), multiline triple-quoted strings (`"""..."""`), and raw strings (prefixed with `خ`, e.g., `خ"..."`, `خ"""..."""`).
  * Handles single-quoted character literals (`'...'`).
  * Processes Baa-specific Arabic escape sequences (`\س`, `\م`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`) using `\` as the escape character. (Standard C-style escapes like `\n`, `\t`, `\uXXXX` are replaced by their Arabic equivalents).
  * Fixed issues with tokenizing opening delimiters for multiline strings (`"""`) and raw strings (`خ"""`) in v0.1.31.0.
  * Enhanced escape sequence handling for multiline strings in v0.1.32.0.

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

### 4. Error Handling

* Identifies lexical errors such as unexpected characters, unterminated string/character/comment literals, and invalid escape sequences.
* When an error is encountered, it generates a `BAA_TOKEN_ERROR` token. The `lexeme` field of this token contains an error message (often in Arabic for number-related errors, English for others currently).
* Includes basic error synchronization to attempt to continue tokenizing after an error.

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

### `BaaToken` (Token Structure)

Represents a single token:

```c
typedef struct {
    BaaTokenType type;       // Enum identifying the token's type
    const wchar_t* lexeme;   // Pointer to the string of characters forming the token (dynamically allocated)
    // For string, char, and comment tokens, this is the *processed content*.
    // For other tokens (keywords, identifiers, numbers, operators), this is the raw source text.
    size_t length;           // Length of the lexeme (content length for processed types)
    size_t line;            // Line number where the token begins
    size_t column;          // Column number where the token begins
} BaaToken;
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

        if (token->type == BAA_TOKEN_ERROR) {
            // fwprintf(stderr, L"Lexical Error: %ls\n", token->lexeme);
        }

        BaaTokenType current_type = token->type; // Store before freeing
        baa_free_token(token);
        if (current_type == BAA_TOKEN_EOF || current_type == BAA_TOKEN_ERROR) {
            break;
        }
    } while (true);
    ```

## Future Improvements and Roadmap Items

* Support for hexadecimal float constants (e.g., `0x1.fp+2`) using Baa's `أ` exponent marker.
* Enhanced error recovery mechanisms and more specific error token types.
* Further Unicode support for identifiers based on UAX #31.
* Performance optimizations (e.g., for keyword lookup, string interning) as needed.
* Robust source mapping if preprocessor outputs `#line` directives.

*For a detailed list of ongoing tasks and future plans, refer to `docs/LEXER_ROADMAP.md`.*
