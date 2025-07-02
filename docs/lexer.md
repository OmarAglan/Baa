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
* **Keywords:** Identifies reserved Arabic keywords. The current keyword set includes:
  - `إذا` (if) - `BAA_TOKEN_IF`
  - `وإلا` (else) - `BAA_TOKEN_ELSE`
  - `طالما` (while) - `BAA_TOKEN_WHILE`
  - `لكل` (for) - `BAA_TOKEN_FOR`
  - `إرجع` (return) - `BAA_TOKEN_RETURN`
  - `ثابت` (const) - `BAA_TOKEN_CONST`
  - `مضمن` (inline) - `BAA_TOKEN_INLINE`
  - `مقيد` (restrict) - `BAA_TOKEN_RESTRICT`
  - `ساكن` (static) - `BAA_TOKEN_STATIC`
  - `خارجي` (extern) - `BAA_TOKEN_EXTERN`
  - `تلقائي` (auto) - `BAA_TOKEN_AUTO`
  - `سجل` (register) - `BAA_TOKEN_REGISTER`
  - `متطاير` (volatile) - `BAA_TOKEN_VOLATILE`
  - `فراغ` (void) - `BAA_TOKEN_VOID`
  - `حرف` (char) - `BAA_TOKEN_CHAR`
  - `قصير` (short) - `BAA_TOKEN_SHORT`
  - `عدد_صحيح` (int) - `BAA_TOKEN_INT`
  - `طويل` (long) - `BAA_TOKEN_LONG`
  - `عائم` (float) - `BAA_TOKEN_FLOAT`
  - `مضاعف` (double) - `BAA_TOKEN_DOUBLE`

  **Note:** The keywords `دالة` (function) and `متغير` (variable) were removed in v0.1.17.0 as Baa uses C-style function and variable declarations. These are now tokenized as identifiers.
* **Numeric Literals:**
  * **Integer Formats:** Supports decimal, hexadecimal (`0x`/`0X`), and binary (`0b`/`0B`) formats
  * **Arabic-Indic Digits:** Full support for Arabic-Indic digits (`٠١٢٣٤٥٦٧٨٩`) in all numeric contexts
  * **Number Separators:** Allows underscores (`_`) as separators for readability in numbers
  * **Arabic Integer Suffixes:**
    - `غ` (unsigned) - equivalent to `u`/`U`
    - `ط` (long) - equivalent to `l`/`L`
    - `طط` (long long) - equivalent to `ll`/`LL`
    - Combinations like `غط` (unsigned long) are supported
  * **Arabic Float Suffix:** `ح` for floating-point literals - equivalent to `f`/`F`
  * **Decimal Separators:** Supports both `.` and Arabic decimal separator `٫`
  * **Scientific Notation:** Uses Arabic exponent marker `أ` instead of `e`/`E`
  * **Hexadecimal Floats:** Supports hex float constants (e.g., `0x1.aأ+2`) with mandatory exponent using `أ`
  * **Number Parser Integration:** The `number_parser.c` component handles conversion of lexemes to actual numeric values with full Arabic format support
* **String and Character Literals:**
  * **String Types:**
    - Standard double-quoted strings (`"..."`)
    - Multiline triple-quoted strings (`"""..."""`)
    - Raw strings prefixed with `خ` (e.g., `خ"..."`, `خ"""..."""`)
  * **Character Literals:** Single-quoted character literals (`'...'`)
  * **Arabic Escape Sequences:** Baa uses Arabic equivalents for standard C escape sequences:
    - `\س` → newline (replaces `\n`)
    - `\م` → tab (replaces `\t`)
    - `\ر` → carriage return (replaces `\r`)
    - `\ص` → null character (replaces `\0`)
    - `\يXXXX` → Unicode character (replaces `\uXXXX`)
    - `\هـHH` → hexadecimal byte (replaces `\xHH`)
  * **Processing:** All escape sequences work correctly in regular strings, character literals, and multiline strings
  * **Recent Fixes:** Enhanced tokenization of multiline and raw string delimiters (v0.1.31.0-v0.1.32.0)

### 3. Comment Handling

The lexer tokenizes all comment types, preserving their content for tools that require full source fidelity:

* **Single-Line Comments (`BAA_TOKEN_SINGLE_LINE_COMMENT`):**
  - Syntax: `// comment text`
  - Lexeme contains text after `//` up to newline (excluding `//`)
  - Example: `// هذا تعليق` → `Token(SINGLE_LINE_COMMENT, " هذا تعليق", line, col)`

* **Multi-Line Comments (`BAA_TOKEN_MULTI_LINE_COMMENT`):**
  - Syntax: `/* comment text */` (non-documentation comments)
  - Lexeme contains text between `/*` and `*/` (excluding delimiters)
  - Example: `/* تعليق متعدد الأسطر */` → `Token(MULTI_LINE_COMMENT, " تعليق متعدد الأسطر ", line, col)`

* **Documentation Comments (`BAA_TOKEN_DOC_COMMENT`):**
  - Syntax: `/** documentation text */`
  - Lexeme contains text between `/**` and `*/` (excluding delimiters)
  - Example: `/** تعليق توثيقي */` → `Token(DOC_COMMENT, " تعليق توثيقي ", line, col)`

**Note:** Preprocessor directives (`#include`, `#define`, etc.) are handled before the lexer stage.

### 4. Whitespace and Newline Handling

Unlike traditional lexers that skip whitespace, the Baa lexer tokenizes all whitespace for complete source fidelity:

* **Whitespace Tokens (`BAA_TOKEN_WHITESPACE`):**
  - Tokenizes sequences of spaces and/or tabs
  - Lexeme contains the exact whitespace sequence
  - Example: `" \t "` → `Token(WHITESPACE, " \t ", line, col)`

* **Newline Tokens (`BAA_TOKEN_NEWLINE`):**
  - Tokenizes all newline sequences: `\n`, `\r`, `\r\n`
  - Lexeme contains the actual sequence encountered
  - Line and column tracking updated automatically
  - Example: `"\r\n"` → `Token(NEWLINE, "\r\n", line, col)`

This approach enables tools like formatters, linters, and IDEs to maintain complete source fidelity.

### 5. Enhanced Error Handling System

The Baa lexer features a comprehensive error handling system designed to provide developers with clear, actionable feedback in Arabic. This system goes beyond generic error reporting to offer specific error types, detailed context, and intelligent suggestions.

#### 5.1 Specific Error Token Types

The lexer generates specific error types instead of generic `BAA_TOKEN_ERROR` tokens:

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

#### 5.2 Error Context Structure

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

#### 5.3 Source Context Extraction

The error handling system automatically extracts source context around error locations:

- **Context Length:** 30 characters before and after the error position
- **Context Cleaning:** Removes newlines and control characters for display
- **Smart Truncation:** Handles context at file boundaries gracefully

#### 5.4 Arabic Error Messages and Suggestions

All error messages and suggestions are provided in Arabic with context-aware recommendations:

##### String Errors

**Unterminated String (1001)**
- Message: `"سلسلة نصية غير منتهية (بدأت في السطر %zu، العمود %zu)"`
- Suggestion: `"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة"`

##### Escape Sequence Errors

**Invalid Escape (1002)**
- Message: `"تسلسل هروب غير صالح '%lc' في [سلسلة نصية|قيمة حرفية] (بدأت في السطر %zu، العمود %zu)"`
- Context-aware suggestions:
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
- Context-aware suggestions:
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

#### 5.6 Enhanced Error Recovery and Synchronization

The lexer implements context-aware error recovery strategies to prevent cascading errors and provide meaningful recovery:

##### Recovery Functions

- `baa_lexer_recover_from_string_error()`: Handles unterminated strings by finding closing quotes or logical boundaries
- `baa_lexer_recover_from_number_error()`: Skips invalid characters until valid token boundary
- `baa_lexer_recover_from_comment_error()`: Searches for comment terminators or EOF
- `baa_lexer_synchronize()`: General synchronization to statement boundaries

##### Recovery Strategies

- **Context-Aware Recovery**: Different strategies based on error type and surrounding context
- **Arabic Keyword Synchronization**: Uses Arabic keywords as reliable synchronization points
- **Cascading Error Prevention**: Prevents multiple errors from the same source issue
- **Smart Boundary Detection**: Recognizes logical stopping points in Arabic code structure

##### Synchronization Points

The lexer uses these elements as synchronization points during error recovery:
- Arabic keywords (`إذا`, `لكل`, `إرجع`, etc.)
- Statement terminators (`.`)
- Block delimiters (`{`, `}`)
- Function boundaries
- Preprocessor directives

## 6. Modular Architecture

The lexer is organized into several specialized source files for maintainability and clarity:

### 6.1 Core Files

* **`lexer.c`**: Core dispatch logic (`baa_lexer_next_token`), Arabic keyword table, and general helper functions
* **`token_scanners.c`**: Specialized scanning functions for different token categories:
  - `scan_identifier()`: Handles identifiers and keywords
  - `scan_number()`: Handles all numeric literal formats
  - `scan_string()`: Handles string literals and escape sequences
  - `scan_character()`: Handles character literals
  - `scan_comment()`: Handles all comment types

### 6.2 Utility Files

* **`lexer_char_utils.c`**: Character classification utilities for Arabic language support:
  - `is_arabic_letter()`: Arabic letter detection
  - `is_arabic_digit()`: Arabic-Indic digit detection
  - `is_baa_digit()`: Combined ASCII/Arabic digit detection
  - `is_arabic_punctuation()`: Arabic punctuation detection

* **`number_parser.c`**: Comprehensive number parsing utility:
  - Converts lexemes to actual numeric values
  - Handles all Arabic numeric formats and suffixes
  - Provides detailed error reporting for invalid numbers
  - Supports Arabic-Indic digits, scientific notation, and suffixes

## 7. Data Structures

### 7.1 BaaLexer (Lexer State)

The lexer state structure maintains all information needed for lexical analysis:

```c
typedef struct {
    const wchar_t* source;     // Source code (UTF-16LE) being lexed
    size_t source_length;      // Length of the source string
    size_t start;              // Start index of the current token in 'source'
    size_t current;            // Current index/position in 'source'
    size_t line;               // Current line number (1-based)
    size_t column;             // Current column number (0-based internally, 1-based in reports)
    const wchar_t* filename;   // Source filename for error reporting
} BaaLexer;
```

### 7.2 BaaToken (Enhanced Token Structure)

The token structure represents a single lexical unit with comprehensive error handling:

```c
typedef struct {
    BaaTokenType type;         // Token type (including specific error types)
    wchar_t* lexeme;          // Token content (dynamically allocated)
    size_t length;            // Length of the lexeme
    size_t line;              // Line number where token begins (1-based)
    size_t column;            // Column number where token begins (1-based)
    BaaSourceSpan span;       // Enhanced source location tracking
    BaaErrorContext* error;   // Error context (NULL for non-error tokens)
} BaaToken;
```

#### Lexeme Content by Token Type

- **String/Character/Comment tokens**: Processed content (escape sequences resolved, delimiters removed)
- **Keywords/Identifiers/Numbers/Operators**: Raw source text
- **Error tokens**: Arabic error message

### 7.3 BaaSourceSpan (Enhanced Location Tracking)

Provides precise source location information for tokens:

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

### 7.4 BaaNumber (Number Parser Structure)

Used by `number_parser.c` for detailed number representation:

```c
typedef struct {
    BaaNumberType type;         // INTEGER, DECIMAL, or SCIENTIFIC
    union {
        long long int_value;    // For integer numbers
        double decimal_value;   // For decimal and scientific numbers
    };
    const wchar_t* raw_text;    // Original lexeme text
    size_t text_length;         // Length of raw text
    // Arabic suffix flags
    bool is_unsigned;           // 'غ' suffix
    bool is_long;              // 'ط' suffix
    bool is_long_long;         // 'طط' suffix
    bool has_float_suffix;     // 'ح' suffix
} BaaNumber;
```

**Note:** A comprehensive list of `BaaTokenType` values can be found in `include/baa/lexer/lexer.h`.

## 8. API Usage

### 8.1 Lexer Initialization

Initialize a lexer instance with source code and filename:

```c
#include "baa/lexer/lexer.h"

// Method 1: Stack-allocated lexer (recommended)
BaaLexer lexer;
baa_init_lexer(&lexer, preprocessed_source, source_filename);

// Method 2: Heap-allocated lexer (legacy)
BaaLexer* lexer = baa_create_lexer(preprocessed_source);
```

**Parameters:**
- `preprocessed_source`: UTF-16LE wide character string from preprocessor
- `source_filename`: Source file name for error reporting (can be NULL)

### 8.2 Token Processing

Process tokens sequentially until EOF or error:

```c
BaaToken* token;
do {
    token = baa_lexer_next_token(&lexer);
    if (!token) {
        // Handle memory allocation error
        break;
    }

    // Basic token information
    wprintf(L"Type: %ls, Lexeme: '%.*ls', Line: %zu, Col: %zu\n",
            baa_token_type_to_string(token->type),
            (int)token->length, token->lexeme,
            token->line, token->column);

    // Enhanced error handling
    if (baa_token_is_error(token)) {
        fwprintf(stderr, L"Lexical Error [%u]: %ls\n",
                 token->error->error_code, token->lexeme);

        if (token->error->suggestion) {
            fwprintf(stderr, L"Suggestion: %ls\n", token->error->suggestion);
        }

        if (token->error->context_before || token->error->context_after) {
            fwprintf(stderr, L"Context: ...%ls[ERROR]%ls...\n",
                    token->error->context_before ? token->error->context_before : L"",
                    token->error->context_after ? token->error->context_after : L"");
        }
    }

    BaaTokenType current_type = token->type;
    baa_free_token(token);
        if (current_type == BAA_TOKEN_EOF || current_type == BAA_TOKEN_ERROR) {
            break;
        }
    } while (true);
    ```

## 9. API Reference

### 9.1 Core Lexer Functions

#### `void baa_init_lexer(BaaLexer *lexer, const wchar_t *source, const wchar_t *filename)`

Initializes a lexer instance for tokenizing source code.

**Parameters:**
- `lexer`: Pointer to a `BaaLexer` structure to initialize
- `source`: UTF-16LE wide character string containing preprocessed source code
- `filename`: Source filename for error reporting (can be NULL)

**Usage:**
```c
BaaLexer lexer;
baa_init_lexer(&lexer, source_code, L"program.ب");
```

#### `BaaToken* baa_lexer_next_token(BaaLexer *lexer)`

Retrieves the next token from the source code.

**Parameters:**
- `lexer`: Pointer to an initialized `BaaLexer` instance

**Returns:**
- Pointer to a dynamically allocated `BaaToken` (must be freed with `baa_free_token()`)
- `NULL` on memory allocation failure

**Usage:**
```c
BaaToken* token = baa_lexer_next_token(&lexer);
if (token) {
    // Process token
    baa_free_token(token);
}
```

#### `BaaLexer* baa_create_lexer(const wchar_t *source)` (Legacy)

Creates a heap-allocated lexer instance.

**Parameters:**
- `source`: UTF-16LE wide character string containing source code

**Returns:**
- Pointer to a dynamically allocated `BaaLexer` (must be freed with `baa_free_lexer()`)
- `NULL` on memory allocation failure

**Note:** This function is considered legacy. Use `baa_init_lexer()` with stack allocation instead.

#### `void baa_free_lexer(BaaLexer *lexer)`

Frees a heap-allocated lexer instance.

**Parameters:**
- `lexer`: Pointer to a `BaaLexer` created with `baa_create_lexer()`

#### `void baa_free_token(BaaToken *token)`

Frees a token and its associated resources.

**Parameters:**
- `token`: Pointer to a `BaaToken` returned by `baa_lexer_next_token()`

**Note:** This function automatically frees error contexts attached to error tokens.

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
