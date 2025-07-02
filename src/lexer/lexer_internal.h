#ifndef BAA_LEXER_INTERNAL_H
#define BAA_LEXER_INTERNAL_H

#include "baa/lexer/lexer.h"      // For BaaLexer, BaaToken, BaaTokenType
#include "baa/lexer/lexer_char_utils.h" // For character classification

// --- Forward declarations of core lexer helper functions (from lexer.c) ---
// These are made available to other .c files within the lexer module.

// Navigation and peeking
bool is_at_end(BaaLexer *lexer);
wchar_t peek(BaaLexer *lexer);
wchar_t peek_next(BaaLexer *lexer);
wchar_t advance(BaaLexer *lexer);
bool match(BaaLexer *lexer, wchar_t expected);

// Token creation
BaaToken *make_token(BaaLexer *lexer, BaaTokenType type);


// Enhanced error token creation
BaaToken *make_specific_error_token(BaaLexer *lexer, BaaTokenType error_type,
                                   uint32_t error_code, const char *category,
                                   const wchar_t *suggestion,
                                   const wchar_t *format, ...);

// Error recovery
void synchronize(BaaLexer *lexer);
void enhanced_synchronize(BaaLexer *lexer, BaaTokenType error_type);

// Specific synchronization strategies
void synchronize_string_error(BaaLexer *lexer);      // Find next quote or newline
void synchronize_number_error(BaaLexer *lexer);      // Find next non-digit character
void synchronize_comment_error(BaaLexer *lexer);     // Find next */ or EOF
void synchronize_general_error(BaaLexer *lexer);     // Current basic strategy

// Source span utilities
BaaLexerSourceSpan calculate_source_span(BaaLexer *lexer, size_t start_offset);
void update_token_span(BaaToken *token, BaaLexer *lexer);

// String/Char literal helpers
void append_char_to_buffer(wchar_t **buffer, size_t *len, size_t *capacity, wchar_t c);
int scan_hex_escape(BaaLexer *lexer, int length);

// Keyword lookup
// Structure for keyword mapping (defined in lexer.c)
struct KeywordMapping {
    const wchar_t *keyword;
    BaaTokenType token;
};
extern struct KeywordMapping keywords[];
extern const size_t NUM_KEYWORDS; // Defined in lexer.c

#endif // BAA_LEXER_INTERNAL_H
