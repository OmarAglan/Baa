#include "baa/lexer/lexer.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h> // For error messages
#include <string.h>
#include <wctype.h>
#include <stdarg.h> // Needed for va_list, etc.
// lexer_char_utils.h is included via lexer_internal.h
#include "baa/lexer/lexer_internal.h" // For internal helper declarations
#include "baa/lexer/token_scanners.h" // For scan_* function declarations (public as requested)

// Array of keywords and their corresponding token types
// struct KeywordMapping is defined in lexer_internal.h

// Define the actual keywords array (no longer static)
// The type 'struct KeywordMapping' is known via lexer_internal.h
struct KeywordMapping keywords[] = {
    {L"إرجع", BAA_TOKEN_RETURN},
    {L"إذا", BAA_TOKEN_IF},
    {L"إلا", BAA_TOKEN_ELSE},
    {L"طالما", BAA_TOKEN_WHILE},
    {L"لكل", BAA_TOKEN_FOR}, // Note: docs/language.md uses لكل for "for" - lexer uses لأجل
    {L"افعل", BAA_TOKEN_DO},
    {L"اختر", BAA_TOKEN_SWITCH},
    {L"حالة", BAA_TOKEN_CASE},
    {L"توقف", BAA_TOKEN_BREAK},
    {L"أكمل", BAA_TOKEN_CONTINUE}, // Note: docs/language.md uses أكمل for "continue" - lexer uses استمر
    {L"ثابت", BAA_TOKEN_CONST},            // Keyword for constant declaration
    {L"مضمن", BAA_TOKEN_KEYWORD_INLINE},   // Keyword for inline
    {L"مقيد", BAA_TOKEN_KEYWORD_RESTRICT}, // Keyword for restrict
    {L"صحيح", BAA_TOKEN_BOOL_LIT},         // Boolean literal true
    {L"خطأ", BAA_TOKEN_BOOL_LIT},          // Boolean literal false
    {L"عدد_صحيح", BAA_TOKEN_TYPE_INT},     // Type keyword: integer
    {L"عدد_حقيقي", BAA_TOKEN_TYPE_FLOAT},  // Type keyword: float
    {L"حرف", BAA_TOKEN_TYPE_CHAR},         // Type keyword: char
    {L"فراغ", BAA_TOKEN_TYPE_VOID},        // Type keyword: void
    {L"منطقي", BAA_TOKEN_TYPE_BOOL}        // Type keyword: boolean
};
const size_t NUM_KEYWORDS = sizeof(keywords) / sizeof(keywords[0]);

// --- Implementations of core lexer helper functions (no longer static) ---
bool is_at_end(BaaLexer *lexer)
{
    return lexer->current >= lexer->source_length;
}

wchar_t peek(BaaLexer *lexer)
{
    if (is_at_end(lexer))
        return L'\0';
    return lexer->source[lexer->current];
}

wchar_t peek_next(BaaLexer *lexer)
{
    if (is_at_end(lexer))
        return L'\0';
    return lexer->source[lexer->current + 1];
}

wchar_t advance(BaaLexer *lexer)
{
    if (is_at_end(lexer))
        return L'\0';
    wchar_t c = lexer->source[lexer->current++];
    if (c == L'\n')
    {
        lexer->line++;
        lexer->column = 0;
    }
    else
    {
        lexer->column++;
    }
    return c; // Return the character that was consumed
}

bool match(BaaLexer *lexer, wchar_t expected)
{
    if (is_at_end(lexer))
        return false;
    if (lexer->source[lexer->current] != expected)
        return false;
    advance(lexer);
    return true;
}

// Corresponds to baa_init_lexer in header
void baa_init_lexer(BaaLexer *lexer, const wchar_t *source, const wchar_t *filename) // Added definition
{
    // filename parameter is currently unused in the struct, but included for signature match
    (void)filename; // Mark as unused to prevent compiler warnings

    if (!lexer || !source)
        return; // Basic validation

    lexer->source = source;
    lexer->source_length = wcslen(source); // Calculate and store source length
    lexer->start = 0;
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 0;
    // Note: No had_error field in the struct per lexer.h
}

// Creates a token by copying the lexeme from the source
BaaToken *make_token(BaaLexer *lexer, BaaTokenType type)
{
    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token)
    {
        // Major issue: Cannot even allocate memory for a token
        fprintf(stderr, "FATAL: Failed to allocate memory for token.\n");
        // In a real scenario, might need a more robust way to signal this
        // For now, returning NULL, caller should check.
        return NULL;
    }

    token->type = type;
    token->length = lexer->current - lexer->start;
    token->lexeme = malloc((token->length + 1) * sizeof(wchar_t));
    if (!token->lexeme)
    {
        fprintf(stderr, "FATAL: Failed to allocate memory for token lexeme.\n");
        free(token);
        return NULL;
    }
    wcsncpy((wchar_t *)token->lexeme, &lexer->source[lexer->start], token->length);
    ((wchar_t *)token->lexeme)[token->length] = L'\0'; // Null-terminate
    token->line = lexer->line;
    // Calculate column based on start position, accounting for line breaks within token?
    // Simple approach: column where token starts.
    // A more precise column might require tracking start column explicitly.
    token->column = lexer->column - token->length; // Approximate start column

    return token;
}

// Creates an error token with a formatted message (dynamically allocated)
BaaToken *make_error_token(BaaLexer *lexer, const wchar_t *format, ...)
{
    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token)
    {
        fprintf(stderr, "FATAL: Failed to allocate memory for error token struct.\n");
        return NULL;
    }

    wchar_t *buffer = NULL;
    size_t buffer_size = 0;
    va_list args, args_copy;

    va_start(args, format);
    va_copy(args_copy, args); // Copy va_list for the size calculation

    size_t initial_size = 256;
    buffer = malloc(initial_size * sizeof(wchar_t));
    if (!buffer)
    {
        fprintf(stderr, "FATAL: Failed to allocate initial memory for error message.\n");
        va_end(args_copy);
        va_end(args);
        free(token);
        return NULL;
    }

#ifdef _WIN32
    int needed = _vsnwprintf(buffer, initial_size, format, args);
    if (needed < 0)
    {
        wcscpy(buffer, L"خطأ غير معروف في تنسيق رسالة الخطأ الداخلية.");
        needed = wcslen(buffer);
    }
#else
    int needed = vswprintf(buffer, initial_size, format, args);
    if (needed < 0 || (size_t)needed >= initial_size)
    {
        va_end(args);
        needed = vswprintf(NULL, 0, format, args_copy);
        va_end(args_copy);

        if (needed < 0)
        {
            wcscpy(buffer, L"خطأ غير معروف في تنسيق رسالة الخطأ الداخلية.");
            needed = wcslen(buffer);
        }
        else
        {
            buffer_size = (size_t)needed + 1;
            wchar_t *new_buffer = realloc(buffer, buffer_size * sizeof(wchar_t));
            if (!new_buffer)
            {
                fprintf(stderr, "FATAL: Failed to reallocate memory for error message.\n");
                free(buffer);
                free(token);
                return NULL;
            }
            buffer = new_buffer;
            va_start(args, format);
            needed = vswprintf(buffer, buffer_size, format, args);
            va_end(args);
            if (needed < 0)
            {
                wcscpy(buffer, L"خطأ فادح في تنسيق رسالة الخطأ الداخلية.");
                needed = wcslen(buffer);
            }
        }
    }
    else
    {
        va_end(args_copy);
        va_end(args);
    }
#endif

    token->type = BAA_TOKEN_ERROR;
    token->lexeme = buffer;
    token->length = wcslen(buffer);
    token->line = lexer->line;
    token->column = lexer->column;
    return token;
}

static BaaToken *skip_whitespace(BaaLexer *lexer)
{
    for (;;)
    {
        wchar_t c = peek(lexer);
        switch (c)
        {
        case L' ':
        case L'\r':
        case L'\t':
            advance(lexer);
            break;
        case L'\n':
            advance(lexer);
            break;
        case L'/':
            if (peek_next(lexer) == L'*')
            {
                size_t start_line = lexer->line;
                size_t start_col = lexer->column;
                advance(lexer);
                advance(lexer);

                if (peek(lexer) == L'*' && peek_next(lexer) != L'/')
                {
                    advance(lexer);
                    return scan_doc_comment(lexer, start_line, start_col);
                }
                else
                {
                    while (!(peek(lexer) == L'*' && peek_next(lexer) == L'/') && !is_at_end(lexer))
                    {
                        if (peek(lexer) == L'\n')
                        {
                            advance(lexer);
                        }
                        else
                        {
                            advance(lexer);
                        }
                    }
                    if (!is_at_end(lexer))
                    {
                        advance(lexer);
                        advance(lexer);
                    }
                    else
                    {
                        BaaToken *error_token = make_error_token(lexer, L"تعليق متعدد الأسطر غير منتهٍ (بدأ في السطر %zu، العمود %zu)", start_line, start_col);
                        return error_token;
                    }
                }
            }
            else if (peek_next(lexer) == L'/')
            {
                while (peek(lexer) != L'\n' && !is_at_end(lexer))
                    advance(lexer);
            }
            else
            {
                return NULL;
            }
            break;
        default:
            return NULL;
        }
    }
}

BaaLexer *baa_create_lexer(const wchar_t *source)
{
    BaaLexer *lexer = malloc(sizeof(BaaLexer));
    if (!lexer)
        return NULL;
    lexer->source = source;
    lexer->start = 0;
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 0;
    return lexer;
}

void baa_free_lexer(BaaLexer *lexer)
{
    if (lexer)
    {
        free(lexer);
    }
}

void baa_free_token(BaaToken *token)
{
    if (token)
    {
        if (token->lexeme)
        {
            free((wchar_t *)token->lexeme);
        }
        free(token);
    }
}

int scan_hex_escape(BaaLexer *lexer, int length)
{
    int value = 0;
    for (int i = 0; i < length; i++)
    {
        if (is_at_end(lexer))
            return -1;
        wchar_t c = peek(lexer);
        int digit;
        if (c >= L'0' && c <= L'9')
        {
            digit = c - L'0';
        }
        else if (c >= L'a' && c <= L'f')
        {
            digit = 10 + (c - L'a');
        }
        else if (c >= L'A' && c <= L'F')
        {
            digit = 10 + (c - L'A');
        }
        else
        {
            return -1;
        }
        value = (value * 16) + digit;
        advance(lexer);
    }
    return value;
}

void append_char_to_buffer(wchar_t **buffer, size_t *len, size_t *capacity, wchar_t c)
{
    if (*len + 1 >= *capacity)
    {
        size_t new_capacity = (*capacity == 0) ? 8 : *capacity * 2;
        wchar_t *new_buffer = realloc(*buffer, new_capacity * sizeof(wchar_t));
        if (!new_buffer)
        {
            fprintf(stderr, "LEXER ERROR: Failed to reallocate string buffer.\n");
            free(*buffer);
            *buffer = NULL;
            *capacity = 0;
            *len = 0;
            return;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    (*buffer)[(*len)++] = c;
}

void synchronize(BaaLexer *lexer)
{
    while (!is_at_end(lexer))
    {
        if (peek(lexer) == L'.')
        {
            advance(lexer);
            return;
        }

        switch (peek(lexer))
        {
        case L'د':
        case L'إ':
        case L'ط':
        case L'ل':
        case L'ا':
        case L'ح':
        case L'ت':
        case L'س':
        case L'م':
        case L'ث':
        case L'#':
        case L'{':
        case L'(':
            return;

        case L'\n':
            advance(lexer);
            break;
        }
        advance(lexer);
    }
}

BaaToken *baa_lexer_next_token(BaaLexer *lexer)
{
    BaaToken *whitespace_error = skip_whitespace(lexer);
    if (whitespace_error != NULL)
    {
        return whitespace_error;
    }

    lexer->start = lexer->current;

    if (lexer->current >= lexer->source_length) {
        lexer->start = lexer->current;
        return make_token(lexer, BAA_TOKEN_EOF);
    }

    wchar_t current_char_peeked = peek(lexer);

    if (current_char_peeked == L'\u062E') {
        if (lexer->current + 1 < lexer->source_length && lexer->source[lexer->current + 1] == L'"') {
            if (lexer->current + 3 < lexer->source_length &&
                lexer->source[lexer->current + 2] == L'"' &&
                lexer->source[lexer->current + 3] == L'"') {
                size_t start_line = lexer->line;
                size_t start_col = lexer->column;
                advance(lexer);
                advance(lexer);
                advance(lexer);
                advance(lexer);
                return scan_raw_string_literal(lexer, true, start_line, start_col);
            } else {
                size_t start_line = lexer->line;
                size_t start_col = lexer->column;
                advance(lexer);
                advance(lexer);
                return scan_raw_string_literal(lexer, false, start_line, start_col);
            }
        }
    }

    if (current_char_peeked == L'"') {
        if (lexer->current + 2 < lexer->source_length &&
            lexer->source[lexer->current + 1] == L'"' &&
            lexer->source[lexer->current + 2] == L'"') {
            size_t start_line = lexer->line;
            size_t start_col = lexer->column;
            advance(lexer);
            advance(lexer);
            advance(lexer);
            return scan_multiline_string_literal(lexer, start_line, start_col);
        } else {
            return scan_string(lexer);
        }
    }

    if (is_arabic_digit(current_char_peeked)) {
        return scan_number(lexer);
    }
    if (iswdigit(current_char_peeked)) {
        return scan_number(lexer);
    }
    if (iswalpha(current_char_peeked) || current_char_peeked == L'_' || is_arabic_letter(current_char_peeked)) {
        return scan_identifier(lexer);
    }

    wchar_t c = advance(lexer);

    if (c == L'\0' && is_at_end(lexer)) {
        lexer->start = lexer->current;
        return make_token(lexer, BAA_TOKEN_EOF);
    }

    switch (c)
    {
    case L'(': return make_token(lexer, BAA_TOKEN_LPAREN);
    case L')': return make_token(lexer, BAA_TOKEN_RPAREN);
    case L'{': return make_token(lexer, BAA_TOKEN_LBRACE);
    case L'}': return make_token(lexer, BAA_TOKEN_RBRACE);
    case L'[': return make_token(lexer, BAA_TOKEN_LBRACKET);
    case L']': return make_token(lexer, BAA_TOKEN_RBRACKET);
    case L',': return make_token(lexer, BAA_TOKEN_COMMA);
    case L'.': return make_token(lexer, BAA_TOKEN_DOT);
    case L':': return make_token(lexer, BAA_TOKEN_COLON);
    case L';': return make_token(lexer, BAA_TOKEN_SEMICOLON);
    case L'%': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_PERCENT_EQUAL) : make_token(lexer, BAA_TOKEN_PERCENT);
    case L'+': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_PLUS_EQUAL) : (match(lexer, L'+') ? make_token(lexer, BAA_TOKEN_INCREMENT) : make_token(lexer, BAA_TOKEN_PLUS));
    case L'-': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_MINUS_EQUAL) : (match(lexer, L'-') ? make_token(lexer, BAA_TOKEN_DECREMENT) : make_token(lexer, BAA_TOKEN_MINUS));
    case L'*': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_STAR_EQUAL) : make_token(lexer, BAA_TOKEN_STAR);
    case L'/': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_SLASH_EQUAL) : make_token(lexer, BAA_TOKEN_SLASH);
    case L'!': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_BANG_EQUAL) : make_token(lexer, BAA_TOKEN_BANG);
    case L'=': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_EQUAL_EQUAL) : make_token(lexer, BAA_TOKEN_EQUAL);
    case L'<': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_LESS_EQUAL) : make_token(lexer, BAA_TOKEN_LESS);
    case L'>': return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_GREATER_EQUAL) : make_token(lexer, BAA_TOKEN_GREATER);
    case L'\'': return scan_char_literal(lexer);
    case 0x060C: return make_token(lexer, BAA_TOKEN_COMMA);
    case 0x061B: return make_token(lexer, BAA_TOKEN_SEMICOLON);
    case 0x061F: return make_token(lexer, BAA_TOKEN_UNKNOWN);
    case 0x066D: return make_token(lexer, BAA_TOKEN_STAR);
    case L'&':
        if (!match(lexer, L'&'))
        {
            BaaToken *error_token = make_error_token(lexer, L"عامل غير صالح: علامة '&' مفردة (هل تقصد '&&'؟)");
            synchronize(lexer);
            return error_token;
        }
        return make_token(lexer, BAA_TOKEN_AND);
    case L'|':
        if (!match(lexer, L'|'))
        {
            BaaToken *error_token = make_error_token(lexer, L"عامل غير صالح: علامة '|' مفردة (هل تقصد '||'؟)");
            synchronize(lexer);
            return error_token;
        }
        return make_token(lexer, BAA_TOKEN_OR);
    }

    BaaToken *error_token = make_error_token(lexer, L"حرف غير متوقع: '%lc' (الكود: %u) في السطر %zu، العمود %zu",
                                             c, (unsigned int)c, lexer->line, lexer->column);
    synchronize(lexer);
    return error_token;
}

const wchar_t *baa_token_type_to_string(BaaTokenType type)
{
    switch (type)
    {
    case BAA_TOKEN_EOF: return L"BAA_TOKEN_EOF";
    case BAA_TOKEN_ERROR: return L"BAA_TOKEN_ERROR";
    case BAA_TOKEN_UNKNOWN: return L"BAA_TOKEN_UNKNOWN";
    case BAA_TOKEN_COMMENT: return L"BAA_TOKEN_COMMENT";
    case BAA_TOKEN_DOC_COMMENT: return L"BAA_TOKEN_DOC_COMMENT";
    case BAA_TOKEN_IDENTIFIER: return L"BAA_TOKEN_IDENTIFIER";
    case BAA_TOKEN_INT_LIT: return L"BAA_TOKEN_INT_LIT";
    case BAA_TOKEN_FLOAT_LIT: return L"BAA_TOKEN_FLOAT_LIT";
    case BAA_TOKEN_CHAR_LIT: return L"BAA_TOKEN_CHAR_LIT";
    case BAA_TOKEN_STRING_LIT: return L"BAA_TOKEN_STRING_LIT";
    case BAA_TOKEN_BOOL_LIT: return L"BAA_TOKEN_BOOL_LIT";
    case BAA_TOKEN_CONST: return L"BAA_TOKEN_CONST";
    case BAA_TOKEN_KEYWORD_INLINE: return L"BAA_TOKEN_KEYWORD_INLINE";
    case BAA_TOKEN_KEYWORD_RESTRICT: return L"BAA_TOKEN_KEYWORD_RESTRICT";
    case BAA_TOKEN_IF: return L"BAA_TOKEN_IF";
    case BAA_TOKEN_ELSE: return L"BAA_TOKEN_ELSE";
    case BAA_TOKEN_WHILE: return L"BAA_TOKEN_WHILE";
    case BAA_TOKEN_FOR: return L"BAA_TOKEN_FOR";
    case BAA_TOKEN_DO: return L"BAA_TOKEN_DO";
    case BAA_TOKEN_CASE: return L"BAA_TOKEN_CASE";
    case BAA_TOKEN_SWITCH: return L"BAA_TOKEN_SWITCH";
    case BAA_TOKEN_RETURN: return L"BAA_TOKEN_RETURN";
    case BAA_TOKEN_BREAK: return L"BAA_TOKEN_BREAK";
    case BAA_TOKEN_CONTINUE: return L"BAA_TOKEN_CONTINUE";
    case BAA_TOKEN_TYPE_INT: return L"BAA_TOKEN_TYPE_INT";
    case BAA_TOKEN_TYPE_FLOAT: return L"BAA_TOKEN_TYPE_FLOAT";
    case BAA_TOKEN_TYPE_CHAR: return L"BAA_TOKEN_TYPE_CHAR";
    case BAA_TOKEN_TYPE_VOID: return L"BAA_TOKEN_TYPE_VOID";
    case BAA_TOKEN_TYPE_BOOL: return L"BAA_TOKEN_TYPE_BOOL";
    case BAA_TOKEN_PLUS: return L"BAA_TOKEN_PLUS";
    case BAA_TOKEN_MINUS: return L"BAA_TOKEN_MINUS";
    case BAA_TOKEN_STAR: return L"BAA_TOKEN_STAR";
    case BAA_TOKEN_SLASH: return L"BAA_TOKEN_SLASH";
    case BAA_TOKEN_PERCENT: return L"BAA_TOKEN_PERCENT";
    case BAA_TOKEN_EQUAL: return L"BAA_TOKEN_EQUAL";
    case BAA_TOKEN_EQUAL_EQUAL: return L"BAA_TOKEN_EQUAL_EQUAL";
    case BAA_TOKEN_BANG: return L"BAA_TOKEN_BANG";
    case BAA_TOKEN_BANG_EQUAL: return L"BAA_TOKEN_BANG_EQUAL";
    case BAA_TOKEN_LESS: return L"BAA_TOKEN_LESS";
    case BAA_TOKEN_LESS_EQUAL: return L"BAA_TOKEN_LESS_EQUAL";
    case BAA_TOKEN_GREATER: return L"BAA_TOKEN_GREATER";
    case BAA_TOKEN_GREATER_EQUAL: return L"BAA_TOKEN_GREATER_EQUAL";
    case BAA_TOKEN_AND: return L"BAA_TOKEN_AND";
    case BAA_TOKEN_OR: return L"BAA_TOKEN_OR";
    case BAA_TOKEN_PLUS_EQUAL: return L"BAA_TOKEN_PLUS_EQUAL";
    case BAA_TOKEN_MINUS_EQUAL: return L"BAA_TOKEN_MINUS_EQUAL";
    case BAA_TOKEN_STAR_EQUAL: return L"BAA_TOKEN_STAR_EQUAL";
    case BAA_TOKEN_SLASH_EQUAL: return L"BAA_TOKEN_SLASH_EQUAL";
    case BAA_TOKEN_PERCENT_EQUAL: return L"BAA_TOKEN_PERCENT_EQUAL";
    case BAA_TOKEN_INCREMENT: return L"BAA_TOKEN_INCREMENT";
    case BAA_TOKEN_DECREMENT: return L"BAA_TOKEN_DECREMENT";
    case BAA_TOKEN_LPAREN: return L"BAA_TOKEN_LPAREN";
    case BAA_TOKEN_RPAREN: return L"BAA_TOKEN_RPAREN";
    case BAA_TOKEN_LBRACE: return L"BAA_TOKEN_LBRACE";
    case BAA_TOKEN_RBRACE: return L"BAA_TOKEN_RBRACE";
    case BAA_TOKEN_LBRACKET: return L"BAA_TOKEN_LBRACKET";
    case BAA_TOKEN_RBRACKET: return L"BAA_TOKEN_RBRACKET";
    case BAA_TOKEN_COMMA: return L"BAA_TOKEN_COMMA";
    case BAA_TOKEN_DOT: return L"BAA_TOKEN_DOT";
    case BAA_TOKEN_SEMICOLON: return L"BAA_TOKEN_SEMICOLON";
    case BAA_TOKEN_COLON: return L"BAA_TOKEN_COLON";
    default:
        return L"BAA_TOKEN_INVALID_TYPE_IN_TO_STRING";
    }
}

// --- Token Utility Implementations ---

bool baa_token_is_keyword(BaaTokenType type)
{
    // The lowest keyword token is now BAA_TOKEN_CONST.
    // The highest keyword token is BAA_TOKEN_CONTINUE.
    // This range includes BAA_TOKEN_KEYWORD_INLINE and BAA_TOKEN_KEYWORD_RESTRICT.
    return type >= BAA_TOKEN_CONST && type <= BAA_TOKEN_CONTINUE;
}

bool baa_token_is_type(BaaTokenType type)
{
    return type >= BAA_TOKEN_TYPE_INT && type <= BAA_TOKEN_TYPE_BOOL;
}

bool baa_token_is_operator(BaaTokenType type)
{
    return (type >= BAA_TOKEN_PLUS && type <= BAA_TOKEN_OR) ||
           (type >= BAA_TOKEN_PLUS_EQUAL && type <= BAA_TOKEN_PERCENT_EQUAL) ||
           (type >= BAA_TOKEN_INCREMENT && type <= BAA_TOKEN_DECREMENT);
}
