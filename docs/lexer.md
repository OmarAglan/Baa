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

The Baa lexer features a comprehensive error handling system that provides specific error types, detailed error context, Arabic error messages, and intelligent suggestions for fixing errors. This system is designed to improve the developer experience by providing clear, actionable feedback when lexical errors occur.

#### 4.1 Specific Error Token Types

Instead of generic `BAA_TOKEN_ERROR` tokens, the lexer generates specific error types with unique error codes and categories:

| Error Type | Code | Category | Description |
|------------|------|----------|-------------|
| `BAA_TOKEN_ERROR_UNTERMINATED_STRING` | 1001 | string | Missing closing quote in string literals |
| `BAA_TOKEN_ERROR_INVALID_ESCAPE` | 1002 | escape | Invalid escape sequences in strings/characters |
| `BAA_TOKEN_ERROR_UNTERMINATED_CHAR` | 1003 | character | Missing closing quote in character literals |
| `BAA_TOKEN_ERROR_INVALID_CHARACTER` | 1004 | character | Invalid characters (e.g., newlines in char literals) |
| `BAA_TOKEN_ERROR_INVALID_NUMBER` | 1005 | number | Invalid number formats (e.g., `0x` without digits) |
| `BAA_TOKEN_ERROR_INVALID_SUFFIX` | 1006 | number | Invalid literal suffixes (e.g., `غغ`, `طططط`) |
| `BAA_TOKEN_ERROR_UNTERMINATED_COMMENT` | 1007 | comment | Missing closing `*/` in comments |
| `BAA_TOKEN_ERROR` | 9001 | memory | Memory allocation errors and system errors |

#### 4.2 Enhanced Error Information

Each error token includes rich contextual information through the `BaaErrorContext` structure:

```c
typedef struct {
    unsigned int error_code;        // Unique error code for internationalization
    const char* category;          // Error category ("string", "escape", "number", etc.)
    const wchar_t* suggestion;     // Helpful suggestion in Arabic
    const wchar_t* context_before; // Source context before error (30 chars)
    const wchar_t* context_after;  // Source context after error (30 chars)
} BaaErrorContext;
```

#### 4.3 Arabic Error Messages and Suggestions

All error messages and suggestions are provided in Arabic with enhanced context:

##### String Errors

**Unterminated String (1001)**
- Message: `"سلسلة نصية غير منتهية (بدأت في السطر %zu، العمود %zu)"`
- Suggestion: `"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة"`

##### Escape Sequence Errors

**Invalid Escape (1002)**
- Message: `"تسلسل هروب غير صالح '%lc' في [سلسلة نصية|قيمة حرفية] (بدأت في السطر %zu، العمود %zu)"`
- Suggestions:
  - General: `"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX"`
  - Specific: `"استخدم \\س بدلاً من \\n للسطر الجديد"`
  - Unicode: `"استخدم \\يXXXX للأحرف اليونيكود بدلاً من \\uXXXX"`
  - Hex: `"استخدم \\هـHH للبايتات السداسية عشرية بدلاً من \\xHH"`

##### Character Literal Errors

**Unterminated Character (1003)**
- Message: `"قيمة حرفية غير منتهية (بدأت في السطر %zu، العمود %zu)"`
- Suggestion: `"أضف علامة اقتباس مفردة ' في نهاية القيمة الحرفية"`

**Invalid Character (1004)**
- Message: `"قيمة حرفية غير صالحة (متعددة الأحرف أو علامة اقتباس مفقودة، بدأت في السطر %zu، العمود %zu)"`
- Suggestion: `"استخدم محرف واحد فقط بين علامتي الاقتباس المفردتين"`

##### Number Format Errors

**Invalid Number (1005)**
- Message: `"تنسيق رقم غير صالح (بدأ في السطر %zu، العمود %zu)"`
- Suggestions:
  - General: `"استخدم الأرقام العربية-الهندية (٠-٩) أو الفاصلة العربية ٫ للأعداد العشرية"`
  - Hex: `"أضف أرقام سداسية عشرية بعد 0x"`
  - Binary: `"أضف أرقام ثنائية (0 أو 1) بعد 0b"`

**Invalid Suffix (1006)**
- Message: `"لاحقة رقم غير صالحة '%ls' (بدأت في السطر %zu، العمود %zu)"`
- Suggestions:
  - `"استخدم لاحقة غ واحدة فقط للأعداد غير المُوقعة"`
  - `"استخدم لاحقة ط واحدة أو اثنتين (طط) للأعداد الطويلة"`
  - `"استخدم لاحقة ح للأعداد العشرية"`

##### Comment Errors

**Unterminated Comment (1007)**
- Message: `"تعليق متعدد الأسطر غير منته (بدأ في السطر %zu، العمود %zu)"`
- Suggestion: `"أضف */ في نهاية التعليق"`

#### 4.4 Source Context Extraction

The error handling system automatically extracts source context around error locations:

- **Context Length**: 30 characters before and after the error position
- **Context Cleaning**: Removes newlines and control characters for display
- **Memory Management**: Contexts are dynamically allocated and properly freed

#### 4.5 Smart Suggestions System

The lexer generates intelligent, contextual suggestions for fixing errors:

##### Baa-Specific Escape Sequences

| C-Style | Baa Equivalent | Arabic Name |
|---------|----------------|-------------|
| `\n` | `\س` | سطر جديد |
| `\t` | `\م` | علامة جدولة |
| `\r` | `\ر` | إرجاع العربة |
| `\0` | `\ص` | محرف فارغ |
| `\uXXXX` | `\يXXXX` | يونيكود |
| `\xHH` | `\هـHH` | سداسي عشري |

##### Arabic Number Formats

- **Arabic-Indic Digits**: `٠١٢٣٤٥٦٧٨٩`
- **Decimal Separator**: `٫` (Arabic decimal separator)
- **Scientific Notation**: `أ` (Arabic exponent marker)
- **Integer Suffixes**: `غ` (unsigned), `ط` (long), `طط` (long long)
- **Float Suffix**: `ح` (float)

#### 4.6 Enhanced Error Recovery and Synchronization

The lexer implements context-aware error recovery:

##### Recovery Functions

- `recover_from_string_error()` - Handles unterminated strings and invalid escapes
- `recover_from_number_error()` - Handles invalid number formats and suffixes
- `recover_from_comment_error()` - Handles unterminated comments
- `recover_from_character_error()` - Handles invalid character literals

##### Recovery Strategies

1. **String Errors**: Skip to next quote or newline
2. **Number Errors**: Skip to next non-digit character
3. **Comment Errors**: Skip to next `*/` or EOF
4. **Character Errors**: Skip to next quote or delimiter

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

        // Enhanced error handling with specific error types and context
        if (baa_token_is_error(token)) {
            // fwprintf(stderr, L"Lexical Error [%u]: %ls\n",
            //          token->error->error_code, token->lexeme);
            // if (token->error->suggestion) {
            //     fwprintf(stderr, L"Suggestion: %ls\n", token->error->suggestion);
            // }
            // if (token->error->context_before) {
            //     fwprintf(stderr, L"Context Before: %ls\n", token->error->context_before);
            // }
            // if (token->error->context_after) {
            //     fwprintf(stderr, L"Context After: %ls\n", token->error->context_after);
            // }
        }

        BaaTokenType current_type = token->type; // Store before freeing
        baa_free_token(token);
        if (current_type == BAA_TOKEN_EOF || current_type == BAA_TOKEN_ERROR) {
            break;
        }
    } while (true);
    ```

## Integration Guide

### Using Enhanced Error Handling

```c
#include "baa/lexer/lexer.h"

BaaLexer lexer;
baa_init_lexer(&lexer, source_code, filename);

BaaToken* token;
while ((token = baa_lexer_next_token(&lexer)) != NULL) {
    if (baa_token_is_error(token)) {
        // Handle enhanced error information
        fwprintf(stderr, L"Error [%u]: %ls\n",
                token->error->error_code, token->lexeme);

        if (token->error->suggestion) {
            fwprintf(stderr, L"Suggestion: %ls\n", token->error->suggestion);
        }

        if (token->error->context_before) {
            fwprintf(stderr, L"Context: ...%ls[ERROR]%ls...\n",
                    token->error->context_before,
                    token->error->context_after);
        }
    }

    baa_free_token(token);
    if (token->type == BAA_TOKEN_EOF) break;
}
```

### Memory Management

- Error contexts are automatically allocated and attached to error tokens
- Use `baa_free_token()` to properly free tokens and their error contexts
- Error contexts are freed automatically during error recovery

### Best Practices

1. **Always check for error tokens** using `baa_token_is_error()`
2. **Display error context** to help users locate and fix errors
3. **Use error codes** for internationalization and categorization
4. **Implement error limits** to prevent excessive error reporting
5. **Provide actionable suggestions** based on error context

## Recent Major Improvements

### ✅ Enhanced Error Handling System (Completed)

* **Specific Error Types**: Implemented 8 specific error token types instead of generic errors
* **Rich Error Context**: Added error codes, categories, and Arabic suggestions
* **Complete Migration**: All 48 error generation points now use the enhanced system
* **Memory Management**: Proper cleanup of error contexts and enhanced token structures
* **Arabic Localization**: All error messages and suggestions provided in Arabic

### ✅ Enhanced Error Context System (Completed)

* **Source Context Extraction**: Automatically extracts 30 characters before/after error locations
* **Smart Suggestions**: Generates contextual Arabic suggestions with correct Baa syntax
* **Context-Aware Recovery**: Implements sophisticated error recovery strategies
* **Baa-Specific Corrections**: Detects and corrects C-style syntax mistakes (e.g., `\n` → `\س`)
* **Enhanced Location Tracking**: Precise source location information with character offsets

### ✅ Complete Migration Statistics

The enhanced error handling system represents a comprehensive overhaul:

* **48 Error Calls Converted**: All `make_error_token` → `make_specific_error_token`
* **8 Specific Error Types**: Replaced generic errors with precise error classification
* **9 Error Codes**: Unique codes (1001-1009, 9001) for internationalization
* **6 Error Categories**: Organized by type (string, escape, character, number, comment, memory, operator)
* **48 Arabic Suggestions**: Actionable advice for fixing each error type
* **Legacy Cleanup**: Removed deprecated `make_error_token` function entirely

### Testing and Validation

The enhanced error handling system has been thoroughly tested with:

- All error scenarios covered with specific test cases
- Arabic error message formatting verified
- Source context extraction validated
- Memory management tested for leaks
- Integration with existing tools confirmed

## Future Improvements and Roadmap Items

* Further Unicode support for identifiers based on UAX #31
* Performance optimizations (e.g., for keyword lookup, string interning) as needed
* Robust source mapping if preprocessor outputs `#line` directives
* Integration with IDE tools for enhanced error display
* Configurable error limits and recovery strategies

*For a detailed list of ongoing tasks and future plans, refer to `docs/LEXER_ROADMAP.md`.*
