# Baa Language Lexer API Reference

**Available in:** [English](#) | [العربية](../05_مرجع_واجهة_البرمجة/واجهة_المحلل_اللفظي.md)

**Status:** ✅ Complete  
**Last Updated:** 2025-01-09  
**Version Compatibility:** v1.0.0+  

## Overview

This document provides a comprehensive API reference for the Baa language lexer component. The lexer is responsible for tokenizing source code and converting it from raw text into a stream of meaningful tokens for the parser.

> **Note:** The complete lexer documentation with implementation details is available in [Lexer Documentation](../02_COMPILER_ARCHITECTURE/LEXER.md). This document focuses specifically on the public API.

## Core Data Structures

### BaaLexer

```c
typedef struct BaaLexer BaaLexer;
```

The main lexer structure that maintains state during tokenization.

### BaaToken

```c
typedef struct {
    BaaTokenType type;         // Token type
    wchar_t* lexeme;          // Token content
    size_t length;            // Length of lexeme
    size_t line;              // Line number (1-based)
    size_t column;            // Column number (1-based)
    BaaSourceSpan span;       // Source location span
    BaaErrorContext* error;   // Error context (NULL for non-error tokens)
} BaaToken;
```

Represents a single token in the source code.

### BaaTokenType

```c
typedef enum {
    // Literals
    BAA_TOKEN_INT_LIT,        // Integer literal (e.g., ١٢٣, 0x1F)
    BAA_TOKEN_FLOAT_LIT,      // Float literal (e.g., ٣٫١٤, 1.5e10)
    BAA_TOKEN_CHAR_LIT,       // Character literal (e.g., 'أ', 'A')
    BAA_TOKEN_STRING_LIT,     // String literal (e.g., "مرحبا")
    BAA_TOKEN_BOOL_LIT,       // Boolean literal (صحيح, خطأ)
    
    // Identifiers and Keywords
    BAA_TOKEN_IDENTIFIER,     // User-defined identifier
    BAA_TOKEN_IF,             // إذا (if)
    BAA_TOKEN_ELSE,           // وإلا (else)
    BAA_TOKEN_WHILE,          // طالما (while)
    BAA_TOKEN_FOR,            // لكل (for)
    BAA_TOKEN_RETURN,         // إرجع (return)
    BAA_TOKEN_CONST,          // ثابت (const)
    BAA_TOKEN_STATIC,         // ساكن (static)
    BAA_TOKEN_EXTERN,         // خارجي (extern)
    
    // Types
    BAA_TOKEN_INT,            // عدد_صحيح (int)
    BAA_TOKEN_FLOAT,          // عدد_حقيقي (float)
    BAA_TOKEN_CHAR,           // حرف (char)
    BAA_TOKEN_VOID,           // فراغ (void)
    BAA_TOKEN_BOOL,           // منطقي (bool)
    
    // Operators
    BAA_TOKEN_PLUS,           // +
    BAA_TOKEN_MINUS,          // -
    BAA_TOKEN_MULTIPLY,       // *
    BAA_TOKEN_DIVIDE,         // /
    BAA_TOKEN_MODULO,         // %
    BAA_TOKEN_ASSIGN,         // =
    BAA_TOKEN_EQUAL,          // ==
    BAA_TOKEN_NOT_EQUAL,      // !=
    BAA_TOKEN_LESS_THAN,      // <
    BAA_TOKEN_GREATER_THAN,   // >
    BAA_TOKEN_LESS_EQUAL,     // <=
    BAA_TOKEN_GREATER_EQUAL,  // >=
    
    // Punctuation
    BAA_TOKEN_SEMICOLON,      // ;
    BAA_TOKEN_COMMA,          // ,
    BAA_TOKEN_LEFT_PAREN,     // (
    BAA_TOKEN_RIGHT_PAREN,    // )
    BAA_TOKEN_LEFT_BRACE,     // {
    BAA_TOKEN_RIGHT_BRACE,    // }
    BAA_TOKEN_LEFT_BRACKET,   // [
    BAA_TOKEN_RIGHT_BRACKET,  // ]
    
    // Special
    BAA_TOKEN_NEWLINE,        // \n, \r\n, \r
    BAA_TOKEN_WHITESPACE,     // Spaces, tabs
    BAA_TOKEN_SINGLE_LINE_COMMENT,  // // comment
    BAA_TOKEN_MULTI_LINE_COMMENT,   // /* comment */
    BAA_TOKEN_DOC_COMMENT,          // /** documentation */
    BAA_TOKEN_EOF,                  // End of file
    
    // Error tokens
    BAA_TOKEN_ERROR_UNTERMINATED_STRING,    // Missing closing quote
    BAA_TOKEN_ERROR_INVALID_ESCAPE,         // Invalid escape sequence
    BAA_TOKEN_ERROR_UNTERMINATED_CHAR,      // Missing closing quote in char
    BAA_TOKEN_ERROR_INVALID_CHARACTER,      // Invalid character
    BAA_TOKEN_ERROR_INVALID_NUMBER,         // Invalid number format
    BAA_TOKEN_ERROR_INVALID_SUFFIX,         // Invalid literal suffix
    BAA_TOKEN_ERROR_UNTERMINATED_COMMENT,   // Missing closing */
    BAA_TOKEN_ERROR                         // General error
} BaaTokenType;
```

### BaaErrorContext

```c
typedef struct {
    unsigned int error_code;        // Unique error code
    const char* category;          // Error category string
    const wchar_t* suggestion;     // Arabic suggestion for fixing
    const wchar_t* context_before; // Source context before error
    const wchar_t* context_after;  // Source context after error
} BaaErrorContext;
```

## Core Functions

### Lexer Creation and Destruction

#### `void baa_init_lexer(BaaLexer *lexer, const wchar_t *source, const wchar_t *filename)`

Initializes a lexer instance for stack allocation.

**Parameters:**
- `lexer`: Pointer to `BaaLexer` structure to initialize
- `source`: UTF-16LE source code string (preprocessed)
- `filename`: Source filename for error reporting (can be NULL)

**Example:**
```c
BaaLexer lexer;
baa_init_lexer(&lexer, source_code, L"program.ب");
```

#### `BaaLexer* baa_create_lexer(const wchar_t *source)`

Creates a heap-allocated lexer instance (legacy).

**Parameters:**
- `source`: UTF-16LE source code string

**Returns:** Pointer to allocated `BaaLexer` or NULL on failure

#### `void baa_free_lexer(BaaLexer *lexer)`

Frees a heap-allocated lexer.

**Parameters:**
- `lexer`: Pointer to lexer created with `baa_create_lexer()`

### Token Processing

#### `BaaToken* baa_lexer_next_token(BaaLexer *lexer)`

Retrieves the next token from the source.

**Parameters:**
- `lexer`: Pointer to initialized lexer

**Returns:** Dynamically allocated token (must be freed) or NULL on memory error

**Example:**
```c
BaaToken* token = baa_lexer_next_token(&lexer);
if (token) {
    // Process token
    baa_free_token(token);
}
```

#### `void baa_free_token(BaaToken *token)`

Frees a token and its associated resources.

**Parameters:**
- `token`: Token returned by `baa_lexer_next_token()`

## Utility Functions

### Token Type Checking

#### `bool baa_token_is_keyword(BaaTokenType type)`

Checks if a token type represents a keyword.

**Parameters:**
- `type`: Token type to check

**Returns:** `true` if token is a keyword

#### `bool baa_token_is_type(BaaTokenType type)`

Checks if a token type represents a data type keyword.

**Parameters:**
- `type`: Token type to check

**Returns:** `true` if token is a type keyword

#### `bool baa_token_is_operator(BaaTokenType type)`

Checks if a token type represents an operator.

**Parameters:**
- `type`: Token type to check

**Returns:** `true` if token is an operator

#### `bool baa_token_is_error(BaaTokenType type)`

Checks if a token type represents an error.

**Parameters:**
- `type`: Token type to check

**Returns:** `true` if token is any error type

### String Conversion

#### `const wchar_t* baa_token_type_to_string(BaaTokenType type)`

Converts a token type to its string representation.

**Parameters:**
- `type`: Token type to convert

**Returns:** Wide string representation of token type

**Example:**
```c
wprintf(L"Token type: %ls\n", baa_token_type_to_string(token->type));
```

## Character Classification Functions

### Arabic Character Support

#### `bool is_arabic_letter(wchar_t c)`

Checks if a character is an Arabic letter.

**Parameters:**
- `c`: Character to check

**Returns:** `true` if character is Arabic letter

#### `bool is_arabic_digit(wchar_t c)`

Checks if a character is an Arabic-Indic digit (٠-٩).

**Parameters:**
- `c`: Character to check

**Returns:** `true` if character is Arabic-Indic digit

#### `bool is_baa_digit(wchar_t c)`

Checks if a character is a valid digit in Baa (ASCII or Arabic-Indic).

**Parameters:**
- `c`: Character to check

**Returns:** `true` if character is valid digit

#### `bool is_baa_hex_digit(wchar_t c)`

Checks if a character is a valid hexadecimal digit.

**Parameters:**
- `c`: Character to check

**Returns:** `true` if character is hex digit

## Error Handling

### Error Context Functions

#### `BaaErrorContext* baa_create_error_context(...)`

Creates an error context with detailed information.

**Parameters:**
- `error_code`: Unique error code
- `category`: Error category string
- `suggestion`: Arabic suggestion text
- `context_before`: Source context before error
- `context_after`: Source context after error

**Returns:** Allocated error context or NULL

#### `void baa_free_error_context(BaaErrorContext* context)`

Frees an error context structure.

**Parameters:**
- `context`: Error context to free

#### `const wchar_t* baa_get_error_category_description(const char* category)`

Gets Arabic description for an error category.

**Parameters:**
- `category`: Error category string

**Returns:** Arabic description of category

## Usage Examples

### Basic Tokenization

```c
#include "baa/lexer/lexer.h"

int main() {
    const wchar_t* source = L"عدد_صحيح العدد = ١٢٣;";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.ب");
    
    BaaToken* token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        wprintf(L"Type: %ls, Lexeme: '%.*ls'\n",
                baa_token_type_to_string(token->type),
                (int)token->length, token->lexeme);
        
        baa_free_token(token);
        if (token->type == BAA_TOKEN_EOF) break;
    }
    
    return 0;
}
```

### Error Handling

```c
BaaToken* token = baa_lexer_next_token(&lexer);
if (token && baa_token_is_error(token->type)) {
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
```

### Token Classification

```c
void classify_token(BaaToken* token) {
    if (baa_token_is_keyword(token->type)) {
        wprintf(L"Keyword: %ls\n", token->lexeme);
    } else if (baa_token_is_type(token->type)) {
        wprintf(L"Type: %ls\n", token->lexeme);
    } else if (baa_token_is_operator(token->type)) {
        wprintf(L"Operator: %ls\n", token->lexeme);
    } else if (token->type == BAA_TOKEN_IDENTIFIER) {
        wprintf(L"Identifier: %ls\n", token->lexeme);
    }
}
```

## Best Practices

### Memory Management
1. Always call `baa_free_token()` for every token returned
2. Use stack allocation (`baa_init_lexer`) when possible
3. Check for NULL returns from token creation functions

### Error Handling
1. Always check if tokens are error tokens using `baa_token_is_error()`
2. Display error context and suggestions to help users
3. Handle memory allocation failures gracefully

### Performance
1. Reuse lexer instances for multiple source strings when possible
2. Process tokens immediately rather than storing large arrays
3. Use appropriate buffer sizes for your use case

## See Also

- [Lexer Documentation](../02_COMPILER_ARCHITECTURE/LEXER.md) - Complete lexer implementation details
- [Parser API](PARSER_API.md) - Parser functions that consume tokens
- [Arabic Features](../01_LANGUAGE_SPECIFICATION/ARABIC_FEATURES.md) - Arabic language support

---

*This document is part of the [Baa Language Documentation](../NAVIGATION.md)*
