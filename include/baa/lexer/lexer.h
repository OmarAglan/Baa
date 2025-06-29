#ifndef BAA_LEXER_H
#define BAA_LEXER_H

#include <stdio.h>
#include <wchar.h>
#include <stdbool.h>
#include <stddef.h>

// Number token types
typedef enum
{
    BAA_NUM_INTEGER,   // عدد_صحيح - Integer number
    BAA_NUM_DECIMAL,   // عدد_عشري - Decimal number
    BAA_NUM_SCIENTIFIC // عدد_علمي - Scientific notation
} BaaNumberType;

// Number token structure
typedef struct
{
    BaaNumberType type;
    union
    {
        long long int_value;
        double decimal_value;
    };
    const wchar_t *raw_text; // Original text representation
    size_t text_length;
    // --- NEW SUFFIX FLAGS ---
    bool is_unsigned;        // 'غ' suffix
    bool is_long;           // 'ط' suffix  
    bool is_long_long;      // 'طط' suffix
    bool has_float_suffix;  // 'ح' suffix
} BaaNumber;

// Error codes for number parsing
typedef enum
{
    BAA_NUM_SUCCESS = 0,
    BAA_NUM_OVERFLOW,       // Number too large
    BAA_NUM_INVALID_CHAR,   // Invalid character in number
    BAA_NUM_MULTIPLE_DOTS,  // Multiple decimal points
    BAA_NUM_INVALID_FORMAT, // Invalid number format
    BAA_NUM_MEMORY_ERROR    // Memory allocation error
} BaaNumberError;

// Number parsing functions
BaaNumber *baa_parse_number(const wchar_t *text, size_t length, BaaNumberError *error);
void baa_free_number(BaaNumber *number);
const wchar_t *baa_number_error_message(BaaNumberError error);

/**
 * Token types for the Baa lexer
 */
typedef enum
{
    // Special tokens
    BAA_TOKEN_EOF,                 // End of file
    BAA_TOKEN_ERROR,               // Generic error token
    BAA_TOKEN_UNKNOWN,             // Unknown token
    
    // Specific error token types for better error handling
    BAA_TOKEN_ERROR_UNTERMINATED_STRING,    // Unterminated string literal
    BAA_TOKEN_ERROR_UNTERMINATED_CHAR,      // Unterminated character literal
    BAA_TOKEN_ERROR_UNTERMINATED_COMMENT,   // Unterminated comment
    BAA_TOKEN_ERROR_INVALID_ESCAPE,         // Invalid escape sequence
    BAA_TOKEN_ERROR_INVALID_NUMBER,         // Invalid number format
    BAA_TOKEN_ERROR_INVALID_CHARACTER,      // Invalid character
    BAA_TOKEN_ERROR_NUMBER_OVERFLOW,        // Number too large
    BAA_TOKEN_ERROR_INVALID_SUFFIX,         // Invalid literal suffix
    BAA_TOKEN_WHITESPACE,          // Sequence of spaces/tabs
    BAA_TOKEN_NEWLINE,             // Newline character(s)
    BAA_TOKEN_SINGLE_LINE_COMMENT, // Content of // comment
    BAA_TOKEN_MULTI_LINE_COMMENT,  // Content of /* ... */ comment (non-doc)
    BAA_TOKEN_DOC_COMMENT,         // Content of /** ... */ comment

    // Literals
    BAA_TOKEN_IDENTIFIER, // Variable/function name
    BAA_TOKEN_INT_LIT,    // Integer literal
    BAA_TOKEN_FLOAT_LIT,  // Float literal
    BAA_TOKEN_CHAR_LIT,   // Character literal
    BAA_TOKEN_STRING_LIT, // String literal
    BAA_TOKEN_BOOL_LIT,   // Boolean literal

    // Keywords
    BAA_TOKEN_CONST,            // ثابت
    BAA_TOKEN_KEYWORD_INLINE,   // مضمن (inline)
    BAA_TOKEN_KEYWORD_RESTRICT, // مقيد (restrict)
    BAA_TOKEN_IF,               // إذا
    BAA_TOKEN_ELSE,             // وإلا
    BAA_TOKEN_WHILE,            // طالما
    BAA_TOKEN_FOR,              // لكل
    BAA_TOKEN_DO,               // افعل
    BAA_TOKEN_CASE,             // حالة
    BAA_TOKEN_SWITCH,           // اختر
    BAA_TOKEN_RETURN,           // إرجع
    BAA_TOKEN_BREAK,            // توقف
    BAA_TOKEN_CONTINUE,         // استمر

    // Types
    BAA_TOKEN_TYPE_INT,   // عدد_صحيح
    BAA_TOKEN_TYPE_FLOAT, // عدد_حقيقي
    BAA_TOKEN_TYPE_CHAR,  // حرف
    BAA_TOKEN_TYPE_VOID,  // فراغ
    BAA_TOKEN_TYPE_BOOL,  // منطقي

    // Operators
    BAA_TOKEN_PLUS,          // +
    BAA_TOKEN_MINUS,         // -
    BAA_TOKEN_STAR,          // *
    BAA_TOKEN_SLASH,         // /
    BAA_TOKEN_PERCENT,       // %
    BAA_TOKEN_EQUAL,         // =
    BAA_TOKEN_EQUAL_EQUAL,   // ==
    BAA_TOKEN_BANG,          // !
    BAA_TOKEN_BANG_EQUAL,    // !=
    BAA_TOKEN_LESS,          // <
    BAA_TOKEN_LESS_EQUAL,    // <=
    BAA_TOKEN_GREATER,       // >
    BAA_TOKEN_GREATER_EQUAL, // >=
    BAA_TOKEN_AND,           // &&
    BAA_TOKEN_OR,            // ||

    // Compound assignment operators
    BAA_TOKEN_PLUS_EQUAL,    // +=
    BAA_TOKEN_MINUS_EQUAL,   // -=
    BAA_TOKEN_STAR_EQUAL,    // *=
    BAA_TOKEN_SLASH_EQUAL,   // /=
    BAA_TOKEN_PERCENT_EQUAL, // %=

    // Increment/decrement operators
    BAA_TOKEN_INCREMENT, // ++
    BAA_TOKEN_DECREMENT, // --

    // Delimiters
    BAA_TOKEN_LPAREN,    // (
    BAA_TOKEN_RPAREN,    // )
    BAA_TOKEN_LBRACE,    // {
    BAA_TOKEN_RBRACE,    // }
    BAA_TOKEN_LBRACKET,  // [
    BAA_TOKEN_RBRACKET,  // ]
    BAA_TOKEN_COMMA,     // ,
    BAA_TOKEN_DOT,       // .
    BAA_TOKEN_SEMICOLON, // ;
    BAA_TOKEN_COLON,     // :
} BaaTokenType;

/**
 * Enhanced source span for better error reporting
 */
typedef struct
{
    size_t start_line;
    size_t start_column;
    size_t end_line;
    size_t end_column;
    size_t start_offset;  // Character offset from start of source
    size_t end_offset;    // Character offset from start of source
} BaaSourceSpan;

/**
 * Error context for enhanced error reporting
 */
typedef struct
{
    wchar_t *suggestion;      // Optional fix suggestion (may be NULL)
    wchar_t *context_before;  // Text before error location (may be NULL)
    wchar_t *context_after;   // Text after error location (may be NULL)
    uint32_t error_code;      // Unique error identifier
    const char *category;     // Error category ("string", "number", "character", etc.)
} BaaErrorContext;

/**
 * Token structure representing a lexical token
 */
typedef struct
{
    BaaTokenType type;        // Type of the token
    wchar_t *lexeme;          // The actual text of the token (parser will take ownership)
    size_t length;            // Length of the lexeme
    size_t line;              // Line number in source (for backward compatibility)
    size_t column;            // Column number in source (for backward compatibility)
    BaaSourceSpan span;       // Enhanced source location information
    BaaErrorContext *error;   // Enhanced error context (only for error tokens, may be NULL)
} BaaToken;

/**
 * Lexer structure for tokenizing source code
 */
typedef struct
{
    const wchar_t *source;     // Source code being lexed
    size_t source_length;      // Length of the source string
    size_t start;              // Start of current token
    size_t current;            // Current position in source
    size_t line;               // Current line number
    size_t column;             // Current column number
    size_t start_token_column; // Column where the current token started
} BaaLexer;

// Lexer functions
BaaLexer *baa_create_lexer(const wchar_t *source); // Consider removing if baa_init_lexer is preferred
void baa_free_lexer(BaaLexer *lexer);
// BaaToken* baa_scan_token(BaaLexer* lexer); // Removed, redundant with baa_lexer_next_token
void baa_free_token(BaaToken *token);
const wchar_t *baa_token_type_to_string(BaaTokenType type);

// Additional lexer functions (Main API)
void baa_init_lexer(BaaLexer *lexer, const wchar_t *source, const wchar_t *filename);
BaaToken *baa_lexer_next_token(BaaLexer *lexer);

// Token utilities
bool baa_token_is_keyword(BaaTokenType type);
bool baa_token_is_type(BaaTokenType type);
bool baa_token_is_operator(BaaTokenType type);

// Error handling (Removed, handled via BAA_TOKEN_ERROR)
// const wchar_t* baa_get_lexer_error(BaaLexer* lexer);
// void baa_clear_lexer_error(BaaLexer* lexer);

#endif /* BAA_LEXER_H */
