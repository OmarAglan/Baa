#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h"  // For baa_strdup and other utilities
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h> // For error messages
#include <string.h>
#include <wctype.h>
#include <stdarg.h> // Needed for va_list, etc.
#include "lexer_internal.h" // For internal helper declarations
#include "baa/lexer/token_scanners.h" // For scan_* function declarations (public as requested)

// Array of keywords and their corresponding token types
// struct KeywordMapping is defined in lexer_internal.h

// Define the actual keywords array (no longer static)
// The type 'struct KeywordMapping' is known via lexer_internal.h
struct KeywordMapping keywords[] = {
    {L"إرجع", BAA_TOKEN_RETURN},
    {L"إذا", BAA_TOKEN_IF},
    {L"وإلا", BAA_TOKEN_ELSE},
    {L"طالما", BAA_TOKEN_WHILE},
    {L"لكل", BAA_TOKEN_FOR},
    {L"افعل", BAA_TOKEN_DO},
    {L"اختر", BAA_TOKEN_SWITCH},
    {L"حالة", BAA_TOKEN_CASE},
    {L"توقف", BAA_TOKEN_BREAK},
    {L"استمر", BAA_TOKEN_CONTINUE},         //  "continue"
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
    wchar_t char_consumed = c; // For debug
    if (c == L'\n')
    {
        lexer->line++;
        lexer->column = 1; // Reset to 1 for new line
    }
    else
    {
        lexer->column++; // Increment for non-newline char
    }
    return char_consumed; // Return the character that was consumed
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
    lexer->column = 1;             // Columns should be 1-based for users
    lexer->start_token_column = 1; // Initialize start_token_column

    // Initialize enhanced error recovery fields
    lexer->error_count = 0;
    lexer->consecutive_errors = 0;
    lexer->error_limit_reached = false;
    baa_init_error_recovery_config(&lexer->recovery_config);
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
    wcsncpy_s((wchar_t *)token->lexeme, token->length + 1, &lexer->source[lexer->start], token->length);
    ((wchar_t *)token->lexeme)[token->length] = L'\0'; // Null-terminate
    token->line = lexer->line;
    token->column = lexer->start_token_column; // Use the recorded start column
    
    // Initialize span (enhanced source location)
    token->span.start_line = lexer->line;
    token->span.start_column = lexer->start_token_column;
    token->span.end_line = lexer->line;
    token->span.end_column = lexer->column;
    token->span.start_offset = lexer->start;
    token->span.end_offset = lexer->current;
    
    // Initialize error context to NULL for non-error tokens
    token->error = NULL;
    
    return token;
}



BaaToken *make_specific_error_token(BaaLexer *lexer, BaaTokenType error_type,
                                    uint32_t error_code, const char *category,
                                    const wchar_t *suggestion,
                                    const wchar_t *format, ...)
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
    va_copy(args_copy, args);
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
    int needed = _vsnwprintf_s(buffer, initial_size, initial_size - 1, format, args);
    if (needed < 0)
    {
        wcscpy_s(buffer, initial_size, L"خطأ غير معروف في تنسيق رسالة الخطأ الداخلية.");
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

    token->type = error_type;
    token->lexeme = buffer;
    token->length = wcslen(buffer);
    token->line = lexer->line;
    token->column = lexer->column;
    
    // Initialize span for error tokens
    token->span.start_line = lexer->line;
    token->span.start_column = lexer->column > 0 ? lexer->column - 1 : 1;
    token->span.end_line = lexer->line;
    token->span.end_column = lexer->column;
    token->span.start_offset = lexer->current > 0 ? lexer->current - 1 : 0;
    token->span.end_offset = lexer->current;

    // Step 4: Enhanced Error Context - Extract source context and generate smart suggestions
    wchar_t *before_context = NULL;
    wchar_t *after_context = NULL;
    size_t error_position = calculate_error_character_position(lexer);
    extract_error_context(lexer, error_position, &before_context, &after_context);

    // Generate enhanced suggestion if none provided or enhance existing one
    wchar_t *enhanced_suggestion = NULL;
    if (!suggestion)
    {
        enhanced_suggestion = generate_error_suggestion(error_type, before_context);
    }
    else
    {
        // Use provided suggestion but could enhance it in the future
        enhanced_suggestion = baa_strdup(suggestion);
    }

    token->error = baa_create_error_context(error_code, category,
                                           enhanced_suggestion ? enhanced_suggestion : suggestion,
                                           before_context, after_context);

    // Clean up temporary allocations
    if (enhanced_suggestion && enhanced_suggestion != suggestion)
    {
        free(enhanced_suggestion);
    }

    return token;
}

BaaLexer *baa_create_lexer(const wchar_t *source)
{
    BaaLexer *lexer = malloc(sizeof(BaaLexer));
    if (!lexer)
        return NULL;
    lexer->source = source;
    lexer->source_length = wcslen(source);
    lexer->start = 0;
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 0;

    // Initialize enhanced error recovery fields
    lexer->error_count = 0;
    lexer->consecutive_errors = 0;
    lexer->error_limit_reached = false;
    baa_init_error_recovery_config(&lexer->recovery_config);

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
        if (token->error)
        {
            baa_free_error_context(token->error);
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

// Enhanced error recovery functions
void baa_init_error_recovery_config(BaaErrorRecoveryConfig *config)
{
    if (!config) return;

    config->max_errors = 50;
    config->max_consecutive_errors = 10;
    config->stop_on_unterminated_string = false;
    config->stop_on_invalid_number = false;
    config->continue_after_comment_errors = true;
    config->sync_search_limit = 1000;
}

bool baa_should_continue_lexing(const BaaLexer *lexer)
{
    if (!lexer) return false;

    if (lexer->error_limit_reached) return false;
    if (lexer->error_count >= lexer->recovery_config.max_errors) return false;
    if (lexer->consecutive_errors >= lexer->recovery_config.max_consecutive_errors) return false;

    return true;
}

void baa_increment_error_count(BaaLexer *lexer, BaaTokenType error_type)
{
    if (!lexer) return;

    lexer->error_count++;
    lexer->consecutive_errors++;

    // Check specific stopping conditions
    switch (error_type)
    {
        case BAA_TOKEN_ERROR_UNTERMINATED_STRING:
            if (lexer->recovery_config.stop_on_unterminated_string)
            {
                lexer->error_limit_reached = true;
                return;
            }
            break;

        case BAA_TOKEN_ERROR_INVALID_NUMBER:
            if (lexer->recovery_config.stop_on_invalid_number)
            {
                lexer->error_limit_reached = true;
                return;
            }
            break;

        case BAA_TOKEN_ERROR_UNTERMINATED_COMMENT:
            if (!lexer->recovery_config.continue_after_comment_errors)
            {
                lexer->error_limit_reached = true;
                return;
            }
            break;

        default:
            break;
    }

    // Check general limits
    if (lexer->error_count >= lexer->recovery_config.max_errors ||
        lexer->consecutive_errors >= lexer->recovery_config.max_consecutive_errors)
    {
        lexer->error_limit_reached = true;
    }
}

void baa_reset_consecutive_errors(BaaLexer *lexer)
{
    if (lexer)
    {
        lexer->consecutive_errors = 0;
    }
}

// Enhanced synchronization functions
void enhanced_synchronize(BaaLexer *lexer, BaaTokenType error_type)
{
    if (!lexer) return;

    switch (error_type)
    {
        case BAA_TOKEN_ERROR_UNTERMINATED_STRING:
        case BAA_TOKEN_ERROR_INVALID_ESCAPE:
            synchronize_string_error(lexer);
            break;

        case BAA_TOKEN_ERROR_INVALID_NUMBER:
        case BAA_TOKEN_ERROR_NUMBER_OVERFLOW:
            synchronize_number_error(lexer);
            break;

        case BAA_TOKEN_ERROR_UNTERMINATED_COMMENT:
            synchronize_comment_error(lexer);
            break;

        default:
            synchronize_general_error(lexer);
            break;
    }
}

void synchronize_string_error(BaaLexer *lexer)
{
    if (!lexer) return;

    size_t chars_searched = 0;
    const size_t max_search = lexer->recovery_config.sync_search_limit;

    while (!is_at_end(lexer) && chars_searched < max_search)
    {
        wchar_t c = peek(lexer);

        // Look for string terminators or safe synchronization points
        if (c == L'"' || c == L'\'' || c == L'\n' || c == L';' || c == L'.')
        {
            if (c == L'"' || c == L'\'')
            {
                advance(lexer); // Consume the quote
            }
            return;
        }

        // Look for Arabic keywords that start statements
        if (c == L'د' || c == L'إ' || c == L'ط' || c == L'ل' || c == L'ا' ||
            c == L'ح' || c == L'ت' || c == L'س' || c == L'م' || c == L'ث')
        {
            return;
        }

        advance(lexer);
        chars_searched++;
    }
}

void synchronize_number_error(BaaLexer *lexer)
{
    if (!lexer) return;

    size_t chars_searched = 0;
    const size_t max_search = lexer->recovery_config.sync_search_limit;

    while (!is_at_end(lexer) && chars_searched < max_search)
    {
        wchar_t c = peek(lexer);

        // Skip until we find a non-digit, non-letter character
        if (!iswdigit(c) && !iswalpha(c) && c != L'.' && c != L'x' && c != L'X' &&
            c != L'b' && c != L'B' && c != L'_' && c != 0x066B) // Arabic decimal separator
        {
            // Found a safe synchronization point
            if (c == L';' || c == L',' || c == L')' || c == L'}' || c == L']' ||
                c == L'\n' || c == L' ' || c == L'\t')
            {
                return;
            }

            // Check for Arabic keywords
            if (c == L'د' || c == L'إ' || c == L'ط' || c == L'ل' || c == L'ا' ||
                c == L'ح' || c == L'ت' || c == L'س' || c == L'م' || c == L'ث')
            {
                return;
            }
        }

        advance(lexer);
        chars_searched++;
    }
}

void synchronize_comment_error(BaaLexer *lexer)
{
    if (!lexer) return;

    size_t chars_searched = 0;
    const size_t max_search = lexer->recovery_config.sync_search_limit;

    while (!is_at_end(lexer) && chars_searched < max_search)
    {
        wchar_t c = peek(lexer);

        // Look for comment terminator */
        if (c == L'*' && peek_next(lexer) == L'/')
        {
            advance(lexer); // Consume *
            advance(lexer); // Consume /
            return;
        }

        // Also synchronize on newlines for single-line recovery
        if (c == L'\n')
        {
            advance(lexer);
            return;
        }

        advance(lexer);
        chars_searched++;
    }
}

void synchronize_general_error(BaaLexer *lexer)
{
    // Use the existing synchronize function as fallback
    synchronize(lexer);
}

// Helper function to create successful tokens and reset consecutive errors
BaaToken *make_successful_token(BaaLexer *lexer, BaaTokenType type)
{
    BaaToken *token = make_token(lexer, type);
    if (token && !baa_token_is_error(type))
    {
        baa_reset_consecutive_errors(lexer);
    }
    return token;
}

// Step 4: Error Context Enhancement - Source Context Extraction Functions

/**
 * Extract source context around error location
 * @param lexer The lexer instance
 * @param error_position Character position of the error in source
 * @param before_context Output parameter for context before error (caller must free)
 * @param after_context Output parameter for context after error (caller must free)
 */
void extract_error_context(BaaLexer *lexer, size_t error_position,
                          wchar_t **before_context, wchar_t **after_context)
{
    if (!lexer || !lexer->source || !before_context || !after_context)
    {
        if (before_context) *before_context = NULL;
        if (after_context) *after_context = NULL;
        return;
    }

    const size_t context_length = 30; // Characters to show before/after error
    size_t start_pos = 0;
    size_t end_pos = lexer->source_length;

    // Calculate before context start position
    if (error_position > context_length)
    {
        start_pos = error_position - context_length;
    }

    // Calculate after context end position
    if (error_position + context_length < lexer->source_length)
    {
        end_pos = error_position + context_length;
    }

    // Extract before context
    if (error_position > start_pos)
    {
        size_t before_len = error_position - start_pos;
        *before_context = malloc((before_len + 1) * sizeof(wchar_t));
        if (*before_context)
        {
            wcsncpy_s(*before_context, before_len + 1, lexer->source + start_pos, before_len);
            (*before_context)[before_len] = L'\0';
        }
    }
    else
    {
        *before_context = NULL;
    }

    // Extract after context
    if (error_position < end_pos)
    {
        size_t after_len = end_pos - error_position;
        *after_context = malloc((after_len + 1) * sizeof(wchar_t));
        if (*after_context)
        {
            wcsncpy_s(*after_context, after_len + 1, lexer->source + error_position, after_len);
            (*after_context)[after_len] = L'\0';
        }
    }
    else
    {
        *after_context = NULL;
    }
}

/**
 * Calculate error position in source
 * @param lexer The lexer instance
 * @return Character position of current error location
 */
size_t calculate_error_character_position(BaaLexer *lexer)
{
    if (!lexer)
        return 0;

    // Use current position as error position
    return lexer->current;
}

/**
 * Get line content for error reporting
 * @param lexer The lexer instance
 * @param line_number Line number to extract (1-based)
 * @return Allocated string containing the line content (caller must free), or NULL on error
 */
wchar_t* get_current_line_content(BaaLexer *lexer, size_t line_number)
{
    if (!lexer || !lexer->source || line_number == 0)
        return NULL;

    const wchar_t *source = lexer->source;
    size_t current_line = 1;
    size_t line_start = 0;
    size_t line_end = 0;
    size_t pos = 0;

    // Find the start of the requested line
    while (pos < lexer->source_length && current_line < line_number)
    {
        if (source[pos] == L'\n')
        {
            current_line++;
            line_start = pos + 1;
        }
        pos++;
    }

    // If we didn't find the line, return NULL
    if (current_line != line_number)
        return NULL;

    // Find the end of the line
    line_end = line_start;
    while (line_end < lexer->source_length && source[line_end] != L'\n')
    {
        line_end++;
    }

    // Extract the line content
    size_t line_length = line_end - line_start;
    wchar_t *line_content = malloc((line_length + 1) * sizeof(wchar_t));
    if (!line_content)
        return NULL;

    wcsncpy_s(line_content, line_length + 1, source + line_start, line_length);
    line_content[line_length] = L'\0';

    return line_content;
}

// Step 4: Smart Suggestions System

/**
 * Generate contextual suggestions based on error type and content
 * @param error_type The type of error encountered
 * @param error_context The context around the error (may be NULL)
 * @return Allocated suggestion string in Arabic (caller must free), or NULL if no suggestion
 */
wchar_t* generate_error_suggestion(BaaTokenType error_type, const wchar_t* error_context)
{
    switch (error_type)
    {
        case BAA_TOKEN_ERROR_UNTERMINATED_STRING:
            return suggest_string_termination_fix(error_context);

        case BAA_TOKEN_ERROR_INVALID_ESCAPE:
            if (error_context && wcslen(error_context) > 0)
                return suggest_escape_sequence_fix(error_context[0]);
            return baa_strdup(L"تحقق من تسلسل الهروب واستخدم التسلسلات الصحيحة: \\س، \\م، \\ر، \\ص، \\يXXXX، \\هـHH");

        case BAA_TOKEN_ERROR_INVALID_NUMBER:
        case BAA_TOKEN_ERROR_NUMBER_OVERFLOW:
            return suggest_number_format_fix(error_context);

        case BAA_TOKEN_ERROR_INVALID_CHARACTER:
            return baa_strdup(L"تأكد من استخدام الأحرف المسموحة في لغة باء (أحرف عربية، إنجليزية، أرقام، شرطة سفلية)");

        case BAA_TOKEN_ERROR_INVALID_SUFFIX:
            return baa_strdup(L"استخدم اللواحق الصحيحة: غ (غير مُوقع)، ط (طويل)، طط (طويل جداً)، ح (حقيقي)");

        case BAA_TOKEN_ERROR_UNTERMINATED_CHAR:
            return baa_strdup(L"أضف علامة اقتباس مفردة ' لإنهاء الحرف");

        default:
            return NULL;
    }
}

/**
 * Suggest escape sequence fix based on invalid escape character
 * @param invalid_escape_char The invalid escape character
 * @return Allocated suggestion string in Arabic (caller must free)
 */
wchar_t* suggest_escape_sequence_fix(wchar_t invalid_escape_char)
{
    wchar_t *suggestion = malloc(300 * sizeof(wchar_t));
    if (!suggestion)
        return NULL;

    switch (invalid_escape_char)
    {
        case L'س':
            wcscpy_s(suggestion, 300, L"استخدم \\س للسطر الجديد (SEEN)");
            break;
        case L'م':
            wcscpy_s(suggestion, 300, L"استخدم \\م للتبويب (MEEM)");
            break;
        case L'ر':
            wcscpy_s(suggestion, 300, L"استخدم \\ر للإرجاع (REH)");
            break;
        case L'ص':
            wcscpy_s(suggestion, 300, L"استخدم \\ص للحرف الفارغ (SAD)");
            break;
        case L'\\':
            wcscpy_s(suggestion, 300, L"استخدم \\\\ للشرطة المائلة العكسية");
            break;
        case L'"':
            wcscpy_s(suggestion, 300, L"استخدم \\\" لعلامة الاقتباس المزدوجة");
            break;
        case L'\'':
            wcscpy_s(suggestion, 300, L"استخدم \\' لعلامة الاقتباس المفردة");
            break;
        case L'ي':
            wcscpy_s(suggestion, 300, L"استخدم \\يXXXX للهروب اليونيكود (مثل \\ي0623 للحرف 'أ') - YEH مع 4 أرقام سداسية عشرية");
            break;
        case L'ه':
            wcscpy_s(suggestion, 300, L"استخدم \\هـHH للهروب السداسي عشري (مثل \\هـ41 للحرف A) - HEH مع تطويل ثم رقمان سداسيان");
            break;
        // Common mistakes - suggest correct Baa equivalents
        case L'n':
            wcscpy_s(suggestion, 300, L"استخدم \\س بدلاً من \\n للسطر الجديد - باء تستخدم الأحرف العربية للهروب");
            break;
        case L't':
            wcscpy_s(suggestion, 300, L"استخدم \\م بدلاً من \\t للتبويب - باء تستخدم الأحرف العربية للهروب");
            break;
        case L'r':
            wcscpy_s(suggestion, 300, L"استخدم \\ر بدلاً من \\r للإرجاع - باء تستخدم الأحرف العربية للهروب");
            break;
        case L'0':
            wcscpy_s(suggestion, 300, L"استخدم \\ص بدلاً من \\0 للحرف الفارغ - باء تستخدم الأحرف العربية للهروب");
            break;
        case L'u':
            wcscpy_s(suggestion, 300, L"استخدم \\يXXXX بدلاً من \\uXXXX للهروب اليونيكود - باء تستخدم \\ي مع 4 أرقام سداسية");
            break;
        case L'x':
            wcscpy_s(suggestion, 300, L"استخدم \\هـHH بدلاً من \\xHH للهروب السداسي عشري - باء تستخدم \\هـ مع رقمين سداسيين");
            break;
        default:
            swprintf(suggestion, 300, L"تسلسل هروب غير صالح '\\%lc' - استخدم: \\س (سطر جديد)، \\م (تبويب)، \\ر (إرجاع)، \\ص (فارغ)، \\\\، \\\"، \\'، \\يXXXX (يونيكود)، \\هـHH (سداسي عشري)", invalid_escape_char);
            break;
    }

    return suggestion;
}

/**
 * Suggest number format fix based on invalid number
 * @param invalid_number The invalid number string (may be NULL)
 * @return Allocated suggestion string in Arabic (caller must free)
 */
wchar_t* suggest_number_format_fix(const wchar_t* invalid_number)
{
    wchar_t *suggestion = malloc(400 * sizeof(wchar_t));
    if (!suggestion)
        return NULL;

    if (!invalid_number)
    {
        wcscpy(suggestion, L"تأكد من صيغة الرقم الصحيحة: ١٢٣ أو 123 للأعداد الصحيحة، ٣٫١٤ أو 3.14 للأعداد العشرية");
        return suggestion;
    }

    // Check for common number format issues
    if (wcschr(invalid_number, L'.') && wcschr(invalid_number, L'٫'))
    {
        wcscpy(suggestion, L"لا تخلط بين النقطة العشرية الإنجليزية (.) والعربية (٫) - استخدم واحدة فقط");
    }
    else if (wcsstr(invalid_number, L"..") || wcsstr(invalid_number, L"٫٫"))
    {
        wcscpy(suggestion, L"لا تستخدم فاصلتين عشريتين متتاليتين - استخدم فاصلة واحدة فقط (. أو ٫)");
    }
    else if (wcschr(invalid_number, L'e') || wcschr(invalid_number, L'E'))
    {
        wcscpy(suggestion, L"استخدم 'أ' بدلاً من 'e' أو 'E' للترميز العلمي (مثل: ١٫٥أ٣ أو 1.5أ3)");
    }
    else if (wcsstr(invalid_number, L"__"))
    {
        wcscpy(suggestion, L"لا تستخدم شرطتين سفليتين متتاليتين - استخدم شرطة واحدة للفصل (مثل: ١_٠٠٠_٠٠٠)");
    }
    else if (wcsstr(invalid_number, L"0x") && !wcschr(invalid_number, L'أ'))
    {
        wcscpy(suggestion, L"للأرقام السداسية العشرية، تأكد من الصيغة الصحيحة: 0x1A2B أو استخدم اللواحق العربية");
    }
    else if (wcsstr(invalid_number, L"ll") || wcsstr(invalid_number, L"LL"))
    {
        wcscpy(suggestion, L"استخدم اللاحقة العربية 'طط' بدلاً من 'll' أو 'LL' للأعداد الطويلة جداً");
    }
    else if (wcschr(invalid_number, L'u') || wcschr(invalid_number, L'U'))
    {
        wcscpy(suggestion, L"استخدم اللاحقة العربية 'غ' بدلاً من 'u' أو 'U' للأعداد غير المُوقعة");
    }
    else if (wcschr(invalid_number, L'f') || wcschr(invalid_number, L'F'))
    {
        wcscpy(suggestion, L"استخدم اللاحقة العربية 'ح' بدلاً من 'f' أو 'F' للأعداد الحقيقية");
    }
    else
    {
        wcscpy(suggestion, L"تحقق من صيغة الرقم: الأرقام (٠-٩ أو 0-9)، الفاصلة العشرية (٫ أو .)، الترميز العلمي (أ)، اللواحق (غ، ط، طط، ح)");
    }

    return suggestion;
}

/**
 * Suggest string termination fix based on partial string
 * @param partial_string The partial string content (may be NULL)
 * @return Allocated suggestion string in Arabic (caller must free)
 */
wchar_t* suggest_string_termination_fix(const wchar_t* partial_string)
{
    wchar_t *suggestion = malloc(250 * sizeof(wchar_t));
    if (!suggestion)
        return NULL;

    if (!partial_string)
    {
        wcscpy(suggestion, L"أضف علامة اقتباس مزدوجة \" لإنهاء السلسلة النصية");
        return suggestion;
    }

    // Check if it looks like a multiline string attempt
    if (wcschr(partial_string, L'\n'))
    {
        wcscpy(suggestion, L"للسلاسل متعددة الأسطر، استخدم \"\"\" في البداية والنهاية");
    }
    // Check if it looks like a raw string attempt
    else if (wcslen(partial_string) > 0 && partial_string[0] == L'خ')
    {
        wcscpy(suggestion, L"للسلاسل الخام، استخدم خ\" في البداية و \" في النهاية");
    }
    else
    {
        wcscpy(suggestion, L"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة النصية");
    }

    return suggestion;
}

BaaToken *baa_lexer_next_token(BaaLexer *lexer)
{
    // Check if we should continue lexing based on error limits
    if (!baa_should_continue_lexing(lexer))
    {
        lexer->start = lexer->current;
        return make_token(lexer, BAA_TOKEN_EOF);
    }

    lexer->start = lexer->current;
    lexer->start_token_column = lexer->column; // Record column at start of token

    if (lexer->current >= lexer->source_length)
    {
        lexer->start = lexer->current;
        return make_token(lexer, BAA_TOKEN_EOF);
    }

    wchar_t c = peek(lexer);

    // 1. Handle Newlines first
    if (c == L'\n')
    {
        advance(lexer); // Consumes \n, advance updates line/col
                        // make_token will use the updated lexer->start which was set before this check,
        // and lexer->current which is now after the \n. Lexeme will be L"\n".
        return make_token(lexer, BAA_TOKEN_NEWLINE);
    }
    if (c == L'\r')
    {
        advance(lexer); // Consumes \r
        if (peek(lexer) == L'\n')
        {
            advance(lexer); // Consumes \n for \r\n
        }
        // make_token will capture the correct lexeme ("\r" or "\r\n")
        // lexer->start was at the beginning of '\r'.
        return make_token(lexer, BAA_TOKEN_NEWLINE);
    }

    // 2. Handle other Whitespace (spaces, tabs)
    if (c == L' ' || c == L'\t')
    {
        // scan_whitespace_sequence will consume all subsequent ' ' and '\t'
        // and call make_token for BAA_TOKEN_WHITESPACE.
        // lexer->start was already set for the beginning of this sequence.
        return scan_whitespace_sequence(lexer); // scan_whitespace_sequence is a new function
    }

    // 3. Handle Comments
    if (c == L'/')
    {
        if (peek_next(lexer) == L'/')
        { // Single-line comment: //
            size_t comment_start_line = lexer->line;
            size_t comment_start_col = lexer->column; // Column of the first '/'
            advance(lexer);                           // Consume first /
            advance(lexer);                           // Consume second /
            // scan_single_line_comment will consume content until newline.
            // Newline itself will be tokenized in the next call to baa_lexer_next_token.
            return scan_single_line_comment(lexer, comment_start_line, comment_start_col);
        }
        else if (peek_next(lexer) == L'*')
        { // Multi-line: /* or Doc: /**
            size_t comment_start_line = lexer->line;
            size_t comment_start_col = lexer->column; // Column of the first '/'

            advance(lexer); // Consume /
            advance(lexer); // Consume *

            if (peek(lexer) == L'*' && peek_next(lexer) != L'/')
            {                   // Doc comment: /** (and not /**/)
                advance(lexer); // Consume the second '*' of /**
                // scan_doc_comment handles from here, consumes content and '*/'
                return scan_doc_comment(lexer, comment_start_line, comment_start_col);
            }
            else
            { // Regular /* or empty /**/
                // scan_multi_line_comment handles from here, consumes content and '*/'
                return scan_multi_line_comment(lexer, comment_start_line, comment_start_col);
            }
        }
        // If just '/', it's not a comment starter here, fall through to operator handling below.
    }

    // 4. Dispatch to other token types
    //    (Raw strings, strings, chars, numbers, identifiers, operators)
    if (c == L'\u062E')
    {
        if (peek_next(lexer) == L'"') // Simplified using peek_next
        {
            if (lexer->current + 3 < lexer->source_length &&
                lexer->source[lexer->current + 2] == L'"' &&
                lexer->source[lexer->current + 3] == L'"')
            {
                // Raw multiline string: خ"""..."""
                size_t start_line = lexer->line;
                size_t start_col = lexer->column;
                // FIX: Consume the opening 'خ"""' delimiter before calling scanner
                advance(lexer); // 'خ'
                advance(lexer); // First '"'
                advance(lexer); // Second '"'
                advance(lexer); // Third '"'
                return scan_raw_string_literal(lexer, true, start_line, start_col);
            }
            else
            {
                // Raw single-line string: خ"..."
                size_t start_line = lexer->line;
                size_t start_col = lexer->column;
                // FIX: Consume the opening 'خ"' delimiter before calling scanner
                advance(lexer); // 'خ'
                advance(lexer); // '"'
                return scan_raw_string_literal(lexer, false, start_line, start_col);
            }
        }
    }

    if (c == L'"')
    {
        if (lexer->current + 2 < lexer->source_length &&
            lexer->source[lexer->current + 1] == L'"' &&
            lexer->source[lexer->current + 2] == L'"')
        {
            size_t start_line = lexer->line;
            size_t start_col = lexer->column;
            // FIX: Consume the opening '"""' delimiter before calling scanner
            advance(lexer); // First '"'
            advance(lexer); // Second '"'
            advance(lexer); // Third '"'
            return scan_multiline_string_literal(lexer, start_line, start_col);
        }
        else
        {
            return scan_string(lexer);
        }
    }

    if (is_baa_digit(c) ||
        (c == L'.' && is_baa_digit(peek_next(lexer))) ||
        (c == L'\u066B' && is_baa_digit(peek_next(lexer))))
    {
        return scan_number(lexer);
    }
    bool check_iswalpha = iswalpha(c);
    bool check_is_underscore = (c == L'_');
    bool check_is_arabic_letter = is_arabic_letter(c);

    if (check_iswalpha || check_is_underscore || check_is_arabic_letter)
    {
        return scan_identifier(lexer);
    }

    // It was 'c' from peek, now consume it if it's an operator/delimiter
    wchar_t first_char_consumed = advance(lexer);

    if (first_char_consumed == L'\0' && is_at_end(lexer))
    { // Should have been caught by is_at_end earlier
        lexer->start = lexer->current;
        return make_token(lexer, BAA_TOKEN_EOF);
    }

    switch (first_char_consumed)
    {
    case L'(':
        return make_token(lexer, BAA_TOKEN_LPAREN);
    case L')':
        return make_token(lexer, BAA_TOKEN_RPAREN);
    case L'{':
        return make_token(lexer, BAA_TOKEN_LBRACE);
    case L'}':
        return make_token(lexer, BAA_TOKEN_RBRACE);
    case L'[':
        return make_token(lexer, BAA_TOKEN_LBRACKET);
    case L']':
        return make_token(lexer, BAA_TOKEN_RBRACKET);
    case L',':
        return make_token(lexer, BAA_TOKEN_COMMA);
    case L'.':
        return make_token(lexer, BAA_TOKEN_DOT);
    case L':':
        return make_token(lexer, BAA_TOKEN_COLON);
    case L';':
        return make_token(lexer, BAA_TOKEN_SEMICOLON);
    case L'%':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_PERCENT_EQUAL) : make_token(lexer, BAA_TOKEN_PERCENT);
    case L'+':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_PLUS_EQUAL) : (match(lexer, L'+') ? make_token(lexer, BAA_TOKEN_INCREMENT) : make_token(lexer, BAA_TOKEN_PLUS));
    case L'-':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_MINUS_EQUAL) : (match(lexer, L'-') ? make_token(lexer, BAA_TOKEN_DECREMENT) : make_token(lexer, BAA_TOKEN_MINUS));
    case L'*':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_STAR_EQUAL) : make_token(lexer, BAA_TOKEN_STAR);
    case L'/':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_SLASH_EQUAL) : make_token(lexer, BAA_TOKEN_SLASH);
    case L'!':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_BANG_EQUAL) : make_token(lexer, BAA_TOKEN_BANG);
    case L'=':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_EQUAL_EQUAL) : make_token(lexer, BAA_TOKEN_EQUAL);
    case L'<':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_LESS_EQUAL) : make_token(lexer, BAA_TOKEN_LESS);
    case L'>':
        return match(lexer, L'=') ? make_token(lexer, BAA_TOKEN_GREATER_EQUAL) : make_token(lexer, BAA_TOKEN_GREATER);
    case L'\'':
        return scan_char_literal(lexer);
    case 0x060C:
        return make_token(lexer, BAA_TOKEN_COMMA);
    case 0x061B:
        return make_token(lexer, BAA_TOKEN_SEMICOLON);
    case 0x061F:
        return make_token(lexer, BAA_TOKEN_UNKNOWN);
    case 0x066D:
        return make_token(lexer, BAA_TOKEN_STAR);
    case L'&':
        if (!match(lexer, L'&'))
        {
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_CHARACTER,
                1008, "operator",
                L"استخدم && للعامل المنطقي AND",
                L"عامل غير صالح: علامة '&' مفردة (هل تقصد '&&'؟)");
            baa_increment_error_count(lexer, BAA_TOKEN_ERROR_INVALID_CHARACTER);
            enhanced_synchronize(lexer, BAA_TOKEN_ERROR_INVALID_CHARACTER);
            return error_token;
        }
        return make_token(lexer, BAA_TOKEN_AND);
    case L'|':
        if (!match(lexer, L'|'))
        {
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_CHARACTER,
                1008, "operator",
                L"استخدم || للعامل المنطقي OR",
                L"عامل غير صالح: علامة '|' مفردة (هل تقصد '||'؟)");
            baa_increment_error_count(lexer, BAA_TOKEN_ERROR_INVALID_CHARACTER);
            enhanced_synchronize(lexer, BAA_TOKEN_ERROR_INVALID_CHARACTER);
            return error_token;
        }
        return make_token(lexer, BAA_TOKEN_OR);
    }

    BaaToken *error_token = make_specific_error_token(lexer,
        BAA_TOKEN_ERROR_INVALID_CHARACTER,
        1009, "character",
        L"تحقق من صحة الحرف أو احذفه",
        L"حرف غير متوقع: '%lc' (الكود: %u) في السطر %zu، العمود %zu",
        c, (unsigned int)c, lexer->line, lexer->column);
    baa_increment_error_count(lexer, BAA_TOKEN_ERROR_INVALID_CHARACTER);
    enhanced_synchronize(lexer, BAA_TOKEN_ERROR_INVALID_CHARACTER);
    return error_token;
}

const wchar_t *baa_token_type_to_string(BaaTokenType type)
{
    switch (type)
    {
    case BAA_TOKEN_EOF:
        return L"BAA_TOKEN_EOF";
    case BAA_TOKEN_ERROR:
        return L"BAA_TOKEN_ERROR";
    case BAA_TOKEN_UNKNOWN:
        return L"BAA_TOKEN_UNKNOWN";
    case BAA_TOKEN_WHITESPACE:
        return L"BAA_TOKEN_WHITESPACE";
    case BAA_TOKEN_NEWLINE:
        return L"BAA_TOKEN_NEWLINE";
    case BAA_TOKEN_SINGLE_LINE_COMMENT:
        return L"BAA_TOKEN_SINGLE_LINE_COMMENT";
    case BAA_TOKEN_MULTI_LINE_COMMENT:
        return L"BAA_TOKEN_MULTI_LINE_COMMENT";
    case BAA_TOKEN_DOC_COMMENT:
        return L"BAA_TOKEN_DOC_COMMENT";
    case BAA_TOKEN_IDENTIFIER:
        return L"BAA_TOKEN_IDENTIFIER";
    case BAA_TOKEN_INT_LIT:
        return L"BAA_TOKEN_INT_LIT";
    case BAA_TOKEN_FLOAT_LIT:
        return L"BAA_TOKEN_FLOAT_LIT";
    case BAA_TOKEN_CHAR_LIT:
        return L"BAA_TOKEN_CHAR_LIT";
    case BAA_TOKEN_STRING_LIT:
        return L"BAA_TOKEN_STRING_LIT";
    case BAA_TOKEN_BOOL_LIT:
        return L"BAA_TOKEN_BOOL_LIT";
    case BAA_TOKEN_CONST:
        return L"BAA_TOKEN_CONST";
    case BAA_TOKEN_KEYWORD_INLINE:
        return L"BAA_TOKEN_KEYWORD_INLINE";
    case BAA_TOKEN_KEYWORD_RESTRICT:
        return L"BAA_TOKEN_KEYWORD_RESTRICT";
    case BAA_TOKEN_IF:
        return L"BAA_TOKEN_IF";
    case BAA_TOKEN_ELSE:
        return L"BAA_TOKEN_ELSE";
    case BAA_TOKEN_WHILE:
        return L"BAA_TOKEN_WHILE";
    case BAA_TOKEN_FOR:
        return L"BAA_TOKEN_FOR";
    case BAA_TOKEN_DO:
        return L"BAA_TOKEN_DO";
    case BAA_TOKEN_CASE:
        return L"BAA_TOKEN_CASE";
    case BAA_TOKEN_SWITCH:
        return L"BAA_TOKEN_SWITCH";
    case BAA_TOKEN_RETURN:
        return L"BAA_TOKEN_RETURN";
    case BAA_TOKEN_BREAK:
        return L"BAA_TOKEN_BREAK";
    case BAA_TOKEN_CONTINUE:
        return L"BAA_TOKEN_CONTINUE";
    case BAA_TOKEN_TYPE_INT:
        return L"BAA_TOKEN_TYPE_INT";
    case BAA_TOKEN_TYPE_FLOAT:
        return L"BAA_TOKEN_TYPE_FLOAT";
    case BAA_TOKEN_TYPE_CHAR:
        return L"BAA_TOKEN_TYPE_CHAR";
    case BAA_TOKEN_TYPE_VOID:
        return L"BAA_TOKEN_TYPE_VOID";
    case BAA_TOKEN_TYPE_BOOL:
        return L"BAA_TOKEN_TYPE_BOOL";
    case BAA_TOKEN_PLUS:
        return L"BAA_TOKEN_PLUS";
    case BAA_TOKEN_MINUS:
        return L"BAA_TOKEN_MINUS";
    case BAA_TOKEN_STAR:
        return L"BAA_TOKEN_STAR";
    case BAA_TOKEN_SLASH:
        return L"BAA_TOKEN_SLASH";
    case BAA_TOKEN_PERCENT:
        return L"BAA_TOKEN_PERCENT";
    case BAA_TOKEN_EQUAL:
        return L"BAA_TOKEN_EQUAL";
    case BAA_TOKEN_EQUAL_EQUAL:
        return L"BAA_TOKEN_EQUAL_EQUAL";
    case BAA_TOKEN_BANG:
        return L"BAA_TOKEN_BANG";
    case BAA_TOKEN_BANG_EQUAL:
        return L"BAA_TOKEN_BANG_EQUAL";
    case BAA_TOKEN_LESS:
        return L"BAA_TOKEN_LESS";
    case BAA_TOKEN_LESS_EQUAL:
        return L"BAA_TOKEN_LESS_EQUAL";
    case BAA_TOKEN_GREATER:
        return L"BAA_TOKEN_GREATER";
    case BAA_TOKEN_GREATER_EQUAL:
        return L"BAA_TOKEN_GREATER_EQUAL";
    case BAA_TOKEN_AND:
        return L"BAA_TOKEN_AND";
    case BAA_TOKEN_OR:
        return L"BAA_TOKEN_OR";
    case BAA_TOKEN_PLUS_EQUAL:
        return L"BAA_TOKEN_PLUS_EQUAL";
    case BAA_TOKEN_MINUS_EQUAL:
        return L"BAA_TOKEN_MINUS_EQUAL";
    case BAA_TOKEN_STAR_EQUAL:
        return L"BAA_TOKEN_STAR_EQUAL";
    case BAA_TOKEN_SLASH_EQUAL:
        return L"BAA_TOKEN_SLASH_EQUAL";
    case BAA_TOKEN_PERCENT_EQUAL:
        return L"BAA_TOKEN_PERCENT_EQUAL";
    case BAA_TOKEN_INCREMENT:
        return L"BAA_TOKEN_INCREMENT";
    case BAA_TOKEN_DECREMENT:
        return L"BAA_TOKEN_DECREMENT";
    case BAA_TOKEN_LPAREN:
        return L"BAA_TOKEN_LPAREN";
    case BAA_TOKEN_RPAREN:
        return L"BAA_TOKEN_RPAREN";
    case BAA_TOKEN_LBRACE:
        return L"BAA_TOKEN_LBRACE";
    case BAA_TOKEN_RBRACE:
        return L"BAA_TOKEN_RBRACE";
    case BAA_TOKEN_LBRACKET:
        return L"BAA_TOKEN_LBRACKET";
    case BAA_TOKEN_RBRACKET:
        return L"BAA_TOKEN_RBRACKET";
    case BAA_TOKEN_COMMA:
        return L"BAA_TOKEN_COMMA";
    case BAA_TOKEN_DOT:
        return L"BAA_TOKEN_DOT";
    case BAA_TOKEN_SEMICOLON:
        return L"BAA_TOKEN_SEMICOLON";
    case BAA_TOKEN_COLON:
        return L"BAA_TOKEN_COLON";
    
    // Specific error token types
    case BAA_TOKEN_ERROR_UNTERMINATED_STRING:
        return L"BAA_TOKEN_ERROR_UNTERMINATED_STRING";
    case BAA_TOKEN_ERROR_UNTERMINATED_CHAR:
        return L"BAA_TOKEN_ERROR_UNTERMINATED_CHAR";
    case BAA_TOKEN_ERROR_UNTERMINATED_COMMENT:
        return L"BAA_TOKEN_ERROR_UNTERMINATED_COMMENT";
    case BAA_TOKEN_ERROR_INVALID_ESCAPE:
        return L"BAA_TOKEN_ERROR_INVALID_ESCAPE";
    case BAA_TOKEN_ERROR_INVALID_NUMBER:
        return L"BAA_TOKEN_ERROR_INVALID_NUMBER";
    case BAA_TOKEN_ERROR_INVALID_CHARACTER:
        return L"BAA_TOKEN_ERROR_INVALID_CHARACTER";
    case BAA_TOKEN_ERROR_NUMBER_OVERFLOW:
        return L"BAA_TOKEN_ERROR_NUMBER_OVERFLOW";
    case BAA_TOKEN_ERROR_INVALID_SUFFIX:
        return L"BAA_TOKEN_ERROR_INVALID_SUFFIX";
    
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

bool baa_token_is_error(BaaTokenType type)
{
    return type == BAA_TOKEN_ERROR ||
           (type >= BAA_TOKEN_ERROR_UNTERMINATED_STRING && type <= BAA_TOKEN_ERROR_INVALID_SUFFIX);
}

// Error context utilities
BaaErrorContext *baa_create_error_context(uint32_t error_code, const char *category,
                                          const wchar_t *suggestion,
                                          const wchar_t *context_before,
                                          const wchar_t *context_after)
{
    BaaErrorContext *context = malloc(sizeof(BaaErrorContext));
    if (!context)
    {
        fprintf(stderr, "FATAL: Failed to allocate memory for error context.\n");
        return NULL;
    }

    context->error_code = error_code;
    context->category = category; // Assuming category is a string literal
    context->suggestion = suggestion ? baa_strdup(suggestion) : NULL;
    context->context_before = context_before ? baa_strdup(context_before) : NULL;
    context->context_after = context_after ? baa_strdup(context_after) : NULL;

    return context;
}

void baa_free_error_context(BaaErrorContext *context)
{
    if (context)
    {
        free(context->suggestion);
        free(context->context_before);
        free(context->context_after);
        free(context);
    }
}

const wchar_t *baa_get_error_category_description(const char *category)
{
    if (!category) return L"غير محدد";
    
    if (strcmp(category, "string") == 0) return L"سلسلة نصية";
    if (strcmp(category, "character") == 0) return L"محرف";
    if (strcmp(category, "number") == 0) return L"رقم";
    if (strcmp(category, "comment") == 0) return L"تعليق";
    if (strcmp(category, "escape") == 0) return L"تسلسل هروب";
    if (strcmp(category, "general") == 0) return L"عام";
    
    return L"غير معروف";
}

const wchar_t *baa_get_error_type_description(BaaTokenType error_type)
{
    switch (error_type)
    {
    case BAA_TOKEN_ERROR:
        return L"خطأ عام";
    case BAA_TOKEN_ERROR_UNTERMINATED_STRING:
        return L"سلسلة نصية غير منتهية";
    case BAA_TOKEN_ERROR_UNTERMINATED_CHAR:
        return L"محرف غير منته";
    case BAA_TOKEN_ERROR_UNTERMINATED_COMMENT:
        return L"تعليق غير منته";
    case BAA_TOKEN_ERROR_INVALID_ESCAPE:
        return L"تسلسل هروب غير صالح";
    case BAA_TOKEN_ERROR_INVALID_NUMBER:
        return L"تنسيق رقم غير صالح";
    case BAA_TOKEN_ERROR_INVALID_CHARACTER:
        return L"محرف غير صالح";
    case BAA_TOKEN_ERROR_NUMBER_OVERFLOW:
        return L"فيض في الرقم";
    case BAA_TOKEN_ERROR_INVALID_SUFFIX:
        return L"لاحقة غير صالحة";
    default:
        return L"نوع خطأ غير معروف";
    }
}
