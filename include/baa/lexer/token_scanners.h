#ifndef BAA_TOKEN_SCANNERS_H
#define BAA_TOKEN_SCANNERS_H

// WARNING: This header declares functions intended for internal use within the lexer module.
// Including it outside the lexer implementation is not recommended.

#include "lexer.h" // For BaaLexer, BaaToken

// Forward declarations of static scanning functions originally in lexer.c
// These are internal to the lexer module but declared here as requested.

// Note: These functions return BaaToken* which must be freed by the caller (usually baa_lexer_next_token)
// or ownership transferred. Error tokens also need freeing.

BaaToken *scan_identifier(BaaLexer *lexer);
BaaToken *scan_number(BaaLexer *lexer);
BaaToken *scan_string(BaaLexer *lexer);
BaaToken *scan_char_literal(BaaLexer *lexer);
BaaToken *scan_multiline_string_literal(BaaLexer *lexer, size_t start_line, size_t start_col);
BaaToken *scan_raw_string_literal(BaaLexer *lexer, bool is_multiline, size_t start_line, size_t start_col);
BaaToken *scan_doc_comment(BaaLexer *lexer, size_t start_line, size_t start_col);
BaaToken *scan_whitespace_sequence(BaaLexer *lexer);
BaaToken *scan_single_line_comment(BaaLexer *lexer, size_t comment_delimiter_start_line, size_t comment_delimiter_start_col);
BaaToken *scan_multi_line_comment(BaaLexer *lexer, size_t comment_delimiter_start_line, size_t comment_delimiter_start_col);

#endif // BAA_TOKEN_SCANNERS_H
