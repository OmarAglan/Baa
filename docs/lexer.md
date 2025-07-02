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

### 9.2 Token Utility Functions

#### `const wchar_t* baa_token_type_to_string(BaaTokenType type)`

Converts a token type to its string representation.

**Parameters:**
- `type`: Token type enum value

**Returns:**
- Wide character string representation of the token type

#### `bool baa_token_is_keyword(BaaTokenType type)`

Checks if a token type represents a keyword.

**Parameters:**
- `type`: Token type to check

**Returns:**
- `true` if the token is a keyword, `false` otherwise

#### `bool baa_token_is_type(BaaTokenType type)`

Checks if a token type represents a data type keyword.

**Parameters:**
- `type`: Token type to check

**Returns:**
- `true` if the token is a type keyword (e.g., `عدد_صحيح`, `فراغ`), `false` otherwise

#### `bool baa_token_is_operator(BaaTokenType type)`

Checks if a token type represents an operator.

**Parameters:**
- `type`: Token type to check

**Returns:**
- `true` if the token is an operator, `false` otherwise

#### `bool baa_token_is_error(BaaTokenType type)`

Checks if a token type represents an error.

**Parameters:**
- `type`: Token type to check

**Returns:**
- `true` if the token is any error type, `false` otherwise

### 9.3 Error Handling Functions

#### `BaaErrorContext* baa_create_error_context(...)`

Creates an error context structure with detailed error information.

**Parameters:**
- `error_code`: Unique error code for internationalization
- `category`: Error category string ("string", "number", etc.)
- `suggestion`: Arabic suggestion text (can be NULL)
- `context_before`: Source context before error (can be NULL)
- `context_after`: Source context after error (can be NULL)

**Returns:**
- Pointer to allocated `BaaErrorContext` (must be freed with `baa_free_error_context()`)

#### `void baa_free_error_context(BaaErrorContext* context)`

Frees an error context structure.

#### `const wchar_t* baa_get_error_category_description(const char* category)`

Gets Arabic description for an error category.

#### `const wchar_t* baa_get_error_type_description(BaaTokenType error_type)`

Gets Arabic description for a specific error type.

### 9.4 Character Classification Functions

#### `bool is_arabic_letter(wchar_t c)`

Checks if a character is an Arabic letter.

#### `bool is_arabic_digit(wchar_t c)`

Checks if a character is an Arabic-Indic digit (٠-٩).

#### `bool is_baa_digit(wchar_t c)`

Checks if a character is a valid digit in Baa (ASCII or Arabic-Indic).

#### `bool is_baa_hex_digit(wchar_t c)`

Checks if a character is a valid hexadecimal digit.

#### `bool is_arabic_punctuation(wchar_t c)`

Checks if a character is Arabic punctuation.

## 10. Arabic Language Features

### 10.1 Arabic Keywords

The Baa lexer recognizes the following Arabic keywords:

#### Control Flow Keywords
| Arabic | English | Token Type | Description |
|--------|---------|------------|-------------|
| `إذا` | if | `BAA_TOKEN_IF` | Conditional statement |
| `وإلا` | else | `BAA_TOKEN_ELSE` | Alternative branch |
| `طالما` | while | `BAA_TOKEN_WHILE` | While loop |
| `لكل` | for | `BAA_TOKEN_FOR` | For loop |
| `افعل` | do | `BAA_TOKEN_DO` | Do-while loop |
| `اختر` | switch | `BAA_TOKEN_SWITCH` | Switch statement |
| `حالة` | case | `BAA_TOKEN_CASE` | Case label |
| `إرجع` | return | `BAA_TOKEN_RETURN` | Return statement |
| `توقف` | break | `BAA_TOKEN_BREAK` | Break statement |
| `استمر` | continue | `BAA_TOKEN_CONTINUE` | Continue statement |

#### Storage Class Keywords
| Arabic | English | Token Type | Description |
|--------|---------|------------|-------------|
| `ثابت` | const | `BAA_TOKEN_CONST` | Constant qualifier |
| `ساكن` | static | `BAA_TOKEN_STATIC` | Static storage |
| `خارجي` | extern | `BAA_TOKEN_EXTERN` | External linkage |
| `تلقائي` | auto | `BAA_TOKEN_AUTO` | Automatic storage |
| `سجل` | register | `BAA_TOKEN_REGISTER` | Register storage |
| `مضمن` | inline | `BAA_TOKEN_KEYWORD_INLINE` | Inline function |
| `مقيد` | restrict | `BAA_TOKEN_KEYWORD_RESTRICT` | Restrict qualifier |

#### Data Type Keywords
| Arabic | English | Token Type | Description |
|--------|---------|------------|-------------|
| `عدد_صحيح` | int | `BAA_TOKEN_TYPE_INT` | Integer type |
| `عدد_حقيقي` | float | `BAA_TOKEN_TYPE_FLOAT` | Floating-point type |
| `حرف` | char | `BAA_TOKEN_TYPE_CHAR` | Character type |
| `فراغ` | void | `BAA_TOKEN_TYPE_VOID` | Void type |
| `منطقي` | bool | `BAA_TOKEN_TYPE_BOOL` | Boolean type |

#### Boolean Literals
| Arabic | English | Token Type | Description |
|--------|---------|------------|-------------|
| `صحيح` | true | `BAA_TOKEN_BOOL_LIT` | Boolean true |
| `خطأ` | false | `BAA_TOKEN_BOOL_LIT` | Boolean false |

### 10.2 Arabic Identifiers

Arabic identifiers in Baa can contain:
- **Arabic letters**: From Unicode ranges U+0600-U+06FF, U+FB50-U+FDFF, U+FE70-U+FEFF
- **English letters**: a-z, A-Z
- **Arabic-Indic digits**: ٠١٢٣٤٥٦٧٨٩ (U+0660-U+0669)
- **ASCII digits**: 0-9
- **Underscores**: _

**Examples:**
```baa
عدد_صحيح العدد = ٥;
عدد_حقيقي المتوسط = ٣٫١٤;
حرف الحرف_الأول = 'أ';
```

### 10.3 Arabic Numeric Literals

#### Arabic-Indic Digits
The lexer supports Arabic-Indic digits (٠-٩) alongside ASCII digits (0-9):

```baa
عدد_صحيح رقم١ = ١٢٣;        // Arabic-Indic digits
عدد_صحيح رقم٢ = 456;         // ASCII digits
عدد_صحيح رقم٣ = ١٢٣456;     // Mixed (allowed)
```

#### Arabic Decimal Separator
Use the Arabic decimal separator `٫` (U+066B) for decimal numbers:

```baa
عدد_حقيقي pi = ٣٫١٤١٥٩;
عدد_حقيقي نصف = ٠٫٥;
```

#### Arabic Scientific Notation
Use `أ` (Arabic letter Alef) as the exponent marker:

```baa
عدد_حقيقي كبير = ١٫٢٣أ٦;     // 1.23 × 10^6
عدد_حقيقي صغير = ٥أ-٣;       // 5 × 10^-3
```

#### Arabic Number Suffixes
| Suffix | Arabic Name | English Equivalent | Description |
|--------|-------------|-------------------|-------------|
| `غ` | غير موقع | unsigned | Unsigned integer |
| `ط` | طويل | long | Long integer |
| `طط` | طويل طويل | long long | Long long integer |
| `ح` | حقيقي | float | Float type |

**Examples:**
```baa
عدد_صحيح عدد_غير_موقع = ١٠٠غ;
عدد_صحيح عدد_طويل = ١٠٠٠ط;
عدد_صحيح عدد_طويل_جداً = ١٠٠٠٠٠٠طط;
عدد_حقيقي رقم_حقيقي = ٣٫١٤ح;
```

### 10.4 Arabic Escape Sequences

Baa uses Arabic letters for escape sequences instead of English letters:

| Baa Sequence | C Equivalent | Arabic Name | Unicode |
|--------------|--------------|-------------|---------|
| `\س` | `\n` | سطر جديد | U+000A |
| `\م` | `\t` | علامة جدولة | U+0009 |
| `\ر` | `\r` | إرجاع العربة | U+000D |
| `\ص` | `\0` | محرف فارغ | U+0000 |
| `\يXXXX` | `\uXXXX` | يونيكود | Unicode character |
| `\هـHH` | `\xHH` | سداسي عشري | Hex byte value |

**Standard escapes still supported:**
- `\\` - Backslash
- `\'` - Single quote
- `\"` - Double quote

**Examples:**
```baa
نص رسالة = "مرحبا\سبالعالم";           // Hello\nWorld
حرف تاب = '\م';                      // Tab character
حرف همزة = '\ي0623';                // Unicode 'أ'
حرف بايت = '\هـ41';                  // Hex byte 'A'
```

### 10.5 Arabic Comments

All comment styles support Arabic text:

```baa
// تعليق من سطر واحد
/* تعليق متعدد
   الأسطر */
/** تعليق توثيقي
 * يدعم النص العربي
 */
```

### 10.6 Unicode Support

The lexer provides comprehensive Unicode support:

#### Character Classification
- **Arabic Letter Detection**: Covers all Arabic Unicode blocks
- **Arabic-Indic Digit Support**: Full support for ٠-٩
- **Arabic Punctuation**: Recognizes Arabic punctuation marks
- **Mixed Script Support**: Allows mixing Arabic and Latin scripts

#### Unicode Ranges Supported
- **Basic Arabic**: U+0600-U+06FF
- **Arabic Presentation Forms-A**: U+FB50-U+FDFF
- **Arabic Presentation Forms-B**: U+FE70-U+FEFF

#### Source File Encoding
- **Input**: UTF-8 or UTF-16LE source files
- **Internal Processing**: UTF-16LE for optimal Arabic text handling
- **Automatic Detection**: Preprocessor detects encoding automatically

## 11. Enhanced Error Handling System

### 11.1 Error Types and Categories

The Baa lexer provides specific error types for different categories of lexical errors:

#### String and Character Errors
| Error Type | Error Code | Category | Description |
|------------|------------|----------|-------------|
| `BAA_TOKEN_ERROR_UNTERMINATED_STRING` | 1001 | string | Missing closing quote in string literals |
| `BAA_TOKEN_ERROR_UNTERMINATED_CHAR` | 1002 | character | Missing closing quote in character literals |
| `BAA_TOKEN_ERROR_INVALID_ESCAPE` | 1003 | escape | Invalid escape sequence in strings/characters |

#### Number Errors
| Error Type | Error Code | Category | Description |
|------------|------------|----------|-------------|
| `BAA_TOKEN_ERROR_INVALID_NUMBER` | 1004 | number | Invalid number format (e.g., multiple decimal points) |
| `BAA_TOKEN_ERROR_NUMBER_OVERFLOW` | 1005 | number | Number too large to represent |
| `BAA_TOKEN_ERROR_INVALID_SUFFIX` | 1006 | number | Invalid literal suffix (e.g., `123xyz`) |

#### Comment Errors
| Error Type | Error Code | Category | Description |
|------------|------------|----------|-------------|
| `BAA_TOKEN_ERROR_UNTERMINATED_COMMENT` | 1007 | comment | Missing closing `*/` in multi-line comments |

#### General Errors
| Error Type | Error Code | Category | Description |
|------------|------------|----------|-------------|
| `BAA_TOKEN_ERROR_INVALID_CHARACTER` | 1008 | general | Unrecognized character in source |
| `BAA_TOKEN_ERROR` | 9001 | memory | Memory allocation and system errors |

### 11.2 Error Context Structure

Each error token includes comprehensive contextual information:

```c
typedef struct {
    wchar_t *suggestion;      // Arabic suggestion for fixing the error
    wchar_t *context_before;  // Source text before error (30 chars)
    wchar_t *context_after;   // Source text after error (30 chars)
    uint32_t error_code;      // Unique error identifier
    const char *category;     // Error category string
} BaaErrorContext;
```

### 11.3 Error Recovery Configuration

The lexer supports configurable error recovery behavior:

```c
typedef struct {
    size_t max_errors;                    // Maximum total errors (default: 50)
    size_t max_consecutive_errors;        // Maximum consecutive errors (default: 10)
    bool stop_on_unterminated_string;     // Stop on string errors (default: false)
    bool stop_on_invalid_number;          // Stop on number errors (default: false)
    bool continue_after_comment_errors;   // Continue after comment errors (default: true)
    size_t sync_search_limit;             // Max chars to search during sync (default: 1000)
} BaaErrorRecoveryConfig;
```

**Configuration Example:**
```c
BaaLexer lexer;
baa_init_lexer(&lexer, source, filename);

// Configure error recovery
baa_init_error_recovery_config(&lexer.recovery_config);
lexer.recovery_config.max_errors = 25;           // Stop after 25 errors
lexer.recovery_config.stop_on_unterminated_string = true;  // Stop on string errors
```

### 11.4 Smart Suggestion System

The lexer generates intelligent, contextual suggestions in Arabic:

#### Escape Sequence Suggestions
For invalid escape sequences, the lexer suggests Baa-specific alternatives:

| Invalid | Suggestion | Arabic Message |
|---------|------------|----------------|
| `\n` | `\س` | `"استخدم \س بدلاً من \n للسطر الجديد"` |
| `\t` | `\م` | `"استخدم \م بدلاً من \t لعلامة الجدولة"` |
| `\uXXXX` | `\يXXXX` | `"استخدم \يXXXX بدلاً من \uXXXX لليونيكود"` |
| `\xHH` | `\هـHH` | `"استخدم \هـHH بدلاً من \xHH للقيم السداسية"` |

#### Number Format Suggestions
For invalid numbers, the lexer provides specific guidance:

| Error | Suggestion |
|-------|------------|
| Multiple decimal points | `"استخدم نقطة عشرية واحدة فقط (. أو ٫)"` |
| Invalid suffix | `"استخدم اللواحق الصحيحة: غ (unsigned)، ط (long)، ح (float)"` |
| Scientific notation | `"استخدم أ بدلاً من e للترميز العلمي"` |

#### String Termination Suggestions
For unterminated strings:

| Context | Suggestion |
|---------|------------|
| Single-line string | `"أضف \" في نهاية السلسلة النصية"` |
| Multi-line string | `"أضف \"\"\" في نهاية السلسلة متعددة الأسطر"` |
| Raw string | `"أضف \" في نهاية السلسلة الخام"` |

### 11.5 Error Recovery Strategies

The lexer implements context-aware error recovery:

#### String Error Recovery
```c
void recover_from_string_error(BaaLexer *lexer) {
    // Search for closing quote or logical boundary
    while (!is_at_end(lexer)) {
        wchar_t c = peek(lexer);
        if (c == L'"' || c == L'\n' || c == L';') {
            if (c == L'"') advance(lexer);  // Consume closing quote
            break;
        }
        advance(lexer);
    }
}
```

#### Number Error Recovery
```c
void recover_from_number_error(BaaLexer *lexer) {
    // Skip invalid characters until token boundary
    while (!is_at_end(lexer)) {
        wchar_t c = peek(lexer);
        if (iswspace(c) || c == L';' || c == L',' || c == L')') {
            break;
        }
        advance(lexer);
    }
}
```

#### Comment Error Recovery
```c
void recover_from_comment_error(BaaLexer *lexer) {
    // Search for comment terminator or EOF
    size_t chars_searched = 0;
    while (!is_at_end(lexer) && chars_searched < sync_search_limit) {
        if (peek(lexer) == L'*' && peek_next(lexer) == L'/') {
            advance(lexer); // Consume *
            advance(lexer); // Consume /
            return;
        }
        advance(lexer);
        chars_searched++;
    }
}
```

#### General Synchronization
The lexer uses Arabic keywords and structural elements as synchronization points:

- **Arabic Keywords**: `إذا`, `لكل`, `إرجع`, `ثابت`, etc.
- **Statement Terminators**: `.` (period)
- **Block Delimiters**: `{`, `}`
- **Function Boundaries**: Function declarations
- **Preprocessor Directives**: `#` directives

### 11.6 Error Reporting Features

#### Multilingual Error Messages
All error messages are provided in Arabic with English fallbacks:

```c
const wchar_t* get_arabic_error_message(BaaTokenType error_type) {
    switch (error_type) {
        case BAA_TOKEN_ERROR_UNTERMINATED_STRING:
            return L"سلسلة نصية غير منتهية";
        case BAA_TOKEN_ERROR_INVALID_ESCAPE:
            return L"تسلسل هروب غير صالح";
        case BAA_TOKEN_ERROR_INVALID_NUMBER:
            return L"تنسيق رقم غير صالح";
        // ... more cases
    }
}
```

#### Source Context Extraction
The lexer automatically extracts source context around errors:

- **Context Length**: 30 characters before and after error location
- **Context Cleaning**: Removes newlines and control characters for display
- **Boundary Handling**: Gracefully handles context at file start/end
- **Memory Management**: Contexts are dynamically allocated and properly freed

## 12. Integration Examples

### 12.1 Basic Lexer Usage

```c
#include "baa/lexer/lexer.h"
#include <stdio.h>
#include <wchar.h>

int main() {
    // Arabic source code example
    const wchar_t* source = L"عدد_صحيح العدد = ١٢٣;\n"
                           L"إذا (العدد > ٠) {\n"
                           L"    إرجع صحيح;\n"
                           L"}";

    // Initialize lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"example.ب");

    // Process tokens
    BaaToken* token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        // Print token information
        wprintf(L"Token: %ls, Type: %ls, Line: %zu, Column: %zu\n",
                token->lexeme,
                baa_token_type_to_string(token->type),
                token->line,
                token->column);

        baa_free_token(token);
        if (token->type == BAA_TOKEN_EOF) break;
    }

    return 0;
}
```

### 12.2 Enhanced Error Handling

```c
#include "baa/lexer/lexer.h"
#include <stdio.h>
#include <wchar.h>

void handle_lexer_error(BaaToken* error_token) {
    // Display error type and message
    fwprintf(stderr, L"خطأ لفظي [%u]: %ls\n",
             error_token->error->error_code,
             error_token->lexeme);

    // Display error location
    fwprintf(stderr, L"الموقع: السطر %zu، العمود %zu\n",
             error_token->line,
             error_token->column);

    // Display error category in Arabic
    const wchar_t* category_desc = baa_get_error_category_description(
        error_token->error->category);
    fwprintf(stderr, L"الفئة: %ls\n", category_desc);

    // Display suggestion if available
    if (error_token->error->suggestion) {
        fwprintf(stderr, L"اقتراح: %ls\n", error_token->error->suggestion);
    }

    // Display source context
    if (error_token->error->context_before || error_token->error->context_after) {
        fwprintf(stderr, L"السياق: ");
        if (error_token->error->context_before) {
            fwprintf(stderr, L"...%ls", error_token->error->context_before);
        }
        fwprintf(stderr, L"[خطأ]");
        if (error_token->error->context_after) {
            fwprintf(stderr, L"%ls...", error_token->error->context_after);
        }
        fwprintf(stderr, L"\n");
    }
}

int lex_with_error_handling(const wchar_t* source, const wchar_t* filename) {
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, filename);

    // Configure error recovery
    baa_init_error_recovery_config(&lexer.recovery_config);
    lexer.recovery_config.max_errors = 10;
    lexer.recovery_config.stop_on_unterminated_string = true;

    int error_count = 0;
    BaaToken* token;

    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        if (baa_token_is_error(token->type)) {
            handle_lexer_error(token);
            error_count++;

            // Check if we should stop lexing
            if (!baa_should_continue_lexing(&lexer)) {
                fwprintf(stderr, L"تم إيقاف التحليل اللفظي بسبب كثرة الأخطاء\n");
                baa_free_token(token);
                break;
            }
        } else {
            // Process normal token
            wprintf(L"%ls ", token->lexeme);
        }

        baa_free_token(token);
        if (token->type == BAA_TOKEN_EOF) break;
    }

    return error_count;
}
```

### 12.3 Token Classification and Processing

```c
#include "baa/lexer/lexer.h"

void process_token_by_category(BaaToken* token) {
    if (baa_token_is_keyword(token->type)) {
        wprintf(L"Keyword: %ls\n", token->lexeme);
    }
    else if (baa_token_is_type(token->type)) {
        wprintf(L"Type: %ls\n", token->lexeme);
    }
    else if (baa_token_is_operator(token->type)) {
        wprintf(L"Operator: %ls\n", token->lexeme);
    }
    else if (token->type == BAA_TOKEN_IDENTIFIER) {
        wprintf(L"Identifier: %ls\n", token->lexeme);
    }
    else if (token->type == BAA_TOKEN_INT_LIT || token->type == BAA_TOKEN_FLOAT_LIT) {
        wprintf(L"Number: %ls\n", token->lexeme);
    }
    else if (token->type == BAA_TOKEN_STRING_LIT) {
        wprintf(L"String: %ls\n", token->lexeme);
    }
    else if (token->type == BAA_TOKEN_CHAR_LIT) {
        wprintf(L"Character: %ls\n", token->lexeme);
    }
    else if (token->type == BAA_TOKEN_BOOL_LIT) {
        wprintf(L"Boolean: %ls\n", token->lexeme);
    }
}

void analyze_source_structure(const wchar_t* source) {
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, NULL);

    int keyword_count = 0;
    int identifier_count = 0;
    int literal_count = 0;
    int operator_count = 0;

    BaaToken* token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        if (baa_token_is_error(token->type)) {
            // Skip error tokens for analysis
        }
        else if (baa_token_is_keyword(token->type)) {
            keyword_count++;
        }
        else if (token->type == BAA_TOKEN_IDENTIFIER) {
            identifier_count++;
        }
        else if (token->type >= BAA_TOKEN_INT_LIT && token->type <= BAA_TOKEN_BOOL_LIT) {
            literal_count++;
        }
        else if (baa_token_is_operator(token->type)) {
            operator_count++;
        }

        process_token_by_category(token);
        baa_free_token(token);
        if (token->type == BAA_TOKEN_EOF) break;
    }

    wprintf(L"\nإحصائيات التحليل:\n");
    wprintf(L"الكلمات المفتاحية: %d\n", keyword_count);
    wprintf(L"المعرفات: %d\n", identifier_count);
    wprintf(L"القيم الحرفية: %d\n", literal_count);
    wprintf(L"العمليات: %d\n", operator_count);
}
```

### 12.4 Number Parser Integration

```c
#include "baa/lexer/lexer.h"

void process_numeric_literals(const wchar_t* source) {
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, NULL);

    BaaToken* token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        if (token->type == BAA_TOKEN_INT_LIT || token->type == BAA_TOKEN_FLOAT_LIT) {
            // Parse the number using the number parser
            BaaNumberError error;
            BaaNumber* number = baa_parse_number(token->lexeme, token->length, &error);

            if (number) {
                wprintf(L"Number: %ls\n", token->lexeme);
                wprintf(L"  Type: %s\n",
                       (number->type == BAA_NUM_INTEGER) ? "Integer" :
                       (number->type == BAA_NUM_DECIMAL) ? "Decimal" : "Scientific");

                if (number->type == BAA_NUM_INTEGER) {
                    wprintf(L"  Value: %lld\n", number->int_value);
                } else {
                    wprintf(L"  Value: %f\n", number->decimal_value);
                }

                // Check Arabic suffixes
                if (number->is_unsigned) wprintf(L"  Suffix: غ (unsigned)\n");
                if (number->is_long) wprintf(L"  Suffix: ط (long)\n");
                if (number->is_long_long) wprintf(L"  Suffix: طط (long long)\n");
                if (number->has_float_suffix) wprintf(L"  Suffix: ح (float)\n");

                baa_free_number(number);
            } else {
                fwprintf(stderr, L"Number parsing error: %ls\n",
                        baa_number_error_message(error));
            }
        }

        baa_free_token(token);
        if (token->type == BAA_TOKEN_EOF) break;
    }
}
```

### 12.5 File Processing Example

```c
#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h"

int process_baa_file(const wchar_t* filename) {
    // Read file content
    wchar_t* source = baa_read_file(filename);
    if (!source) {
        fwprintf(stderr, L"خطأ: لا يمكن قراءة الملف %ls\n", filename);
        return -1;
    }

    // Initialize lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, filename);

    // Configure for production use
    baa_init_error_recovery_config(&lexer.recovery_config);
    lexer.recovery_config.max_errors = 50;
    lexer.recovery_config.max_consecutive_errors = 5;

    // Process file
    int total_tokens = 0;
    int error_count = 0;
    BaaToken* token;

    wprintf(L"معالجة الملف: %ls\n", filename);

    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        total_tokens++;

        if (baa_token_is_error(token->type)) {
            error_count++;

            // Log error to file or console
            fwprintf(stderr, L"%ls:%zu:%zu: خطأ [%u]: %ls\n",
                    filename,
                    token->line,
                    token->column,
                    token->error->error_code,
                    token->lexeme);

            if (token->error->suggestion) {
                fwprintf(stderr, L"  اقتراح: %ls\n", token->error->suggestion);
            }
        }

        baa_free_token(token);
        if (token->type == BAA_TOKEN_EOF) break;

        // Check if we should stop
        if (!baa_should_continue_lexing(&lexer)) {
            fwprintf(stderr, L"تم إيقاف المعالجة بسبب كثرة الأخطاء\n");
            break;
        }
    }

    // Cleanup
    free(source);

    // Report results
    wprintf(L"انتهت المعالجة:\n");
    wprintf(L"  إجمالي الرموز: %d\n", total_tokens);
    wprintf(L"  عدد الأخطاء: %d\n", error_count);

    return error_count;
}
```

### 12.6 Memory Management Best Practices

```c
#include "baa/lexer/lexer.h"

// Safe token processing with proper cleanup
void safe_token_processing(const wchar_t* source) {
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, NULL);

    BaaToken* token = NULL;

    while (true) {
        // Get next token
        token = baa_lexer_next_token(&lexer);

        // Check for allocation failure
        if (!token) {
            fprintf(stderr, "Fatal: Memory allocation failed\n");
            break;
        }

        // Process token
        // ... your processing logic here ...

        // Store token type before freeing (if needed)
        BaaTokenType token_type = token->type;

        // Always free the token
        baa_free_token(token);
        token = NULL;  // Prevent double-free

        // Check for end of file
        if (token_type == BAA_TOKEN_EOF) {
            break;
        }
    }

    // No explicit lexer cleanup needed for stack-allocated lexer
    // Error contexts are automatically freed with tokens
}

// Error-safe batch processing
int process_multiple_sources(const wchar_t** sources, size_t count) {
    int total_errors = 0;

    for (size_t i = 0; i < count; i++) {
        BaaLexer lexer;
        baa_init_lexer(&lexer, sources[i], NULL);

        BaaToken* token;
        while ((token = baa_lexer_next_token(&lexer)) != NULL) {
            if (baa_token_is_error(token->type)) {
                total_errors++;
                // Handle error...
            }

            baa_free_token(token);
            if (token->type == BAA_TOKEN_EOF) break;
        }

        // Lexer automatically cleaned up (stack allocated)
    }

    return total_errors;
}
```

### 12.7 Integration with Parser

```c
#include "baa/lexer/lexer.h"
// #include "baa/parser/parser.h"  // Hypothetical parser header

typedef struct {
    BaaLexer* lexer;
    BaaToken* current_token;
    BaaToken* previous_token;
    int error_count;
} BaaParser;

BaaParser* create_parser(const wchar_t* source, const wchar_t* filename) {
    BaaParser* parser = malloc(sizeof(BaaParser));
    if (!parser) return NULL;

    parser->lexer = malloc(sizeof(BaaLexer));
    if (!parser->lexer) {
        free(parser);
        return NULL;
    }

    baa_init_lexer(parser->lexer, source, filename);
    parser->current_token = NULL;
    parser->previous_token = NULL;
    parser->error_count = 0;

    // Get first token
    advance_parser(parser);

    return parser;
}

void advance_parser(BaaParser* parser) {
    // Free previous token
    if (parser->previous_token) {
        baa_free_token(parser->previous_token);
    }

    // Move current to previous
    parser->previous_token = parser->current_token;

    // Get next token, skipping whitespace and comments if desired
    do {
        parser->current_token = baa_lexer_next_token(parser->lexer);

        if (parser->current_token && baa_token_is_error(parser->current_token->type)) {
            // Handle lexer error in parser context
            parser->error_count++;
            // Log error or add to error list
            // Continue parsing or abort based on error severity
        }
    } while (parser->current_token &&
             (parser->current_token->type == BAA_TOKEN_WHITESPACE ||
              parser->current_token->type == BAA_TOKEN_NEWLINE ||
              parser->current_token->type == BAA_TOKEN_SINGLE_LINE_COMMENT ||
              parser->current_token->type == BAA_TOKEN_MULTI_LINE_COMMENT));
}

void free_parser(BaaParser* parser) {
    if (parser) {
        if (parser->current_token) {
            baa_free_token(parser->current_token);
        }
        if (parser->previous_token) {
            baa_free_token(parser->previous_token);
        }
        free(parser->lexer);
        free(parser);
    }
}
```

### 12.8 Best Practices Summary

1. **Always check for NULL returns** from `baa_lexer_next_token()`
2. **Free every token** with `baa_free_token()` - no exceptions
3. **Use stack allocation** for lexer when possible (`baa_init_lexer`)
4. **Configure error recovery** based on your application's needs
5. **Handle error tokens appropriately** - don't ignore them
6. **Check error limits** with `baa_should_continue_lexing()`
7. **Use Arabic error messages** for better user experience
8. **Extract and display context** to help users locate errors
9. **Implement proper cleanup** in error paths
10. **Test with malformed input** to verify error handling

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
