#ifndef BAA_PARSER_UTILS_INTERNAL_H
#define BAA_PARSER_UTILS_INTERNAL_H

#include "parser_internal.h"
#include "baa/lexer/lexer.h"
#include <stdarg.h>

/**
 * @brief Consumes the current token and fetches the next one from the lexer.
 * Skips over lexical error tokens, reporting them and continuing to the next
 * valid token or EOF. Manages lexeme ownership for current_token and previous_token.
 *
 * @param parser Pointer to the parser state.
 */
void advance(BaaParser *parser);

/**
 * @brief Checks if the current token's type matches the given type.
 *
 * @param parser Pointer to the parser state.
 * @param type The token type to check against.
 * @return True if the current token's type matches, false otherwise.
 */
bool check_token(BaaParser *parser, BaaTokenType type);

/**
 * @brief Checks if the current token matches the given type and consumes it if so.
 *
 * @param parser Pointer to the parser state.
 * @param type The token type to match.
 * @return True if the token was matched and consumed, false otherwise.
 */
bool match_token(BaaParser *parser, BaaTokenType type);

/**
 * @brief Consumes a token of the expected type or reports an error.
 *
 * @param parser Pointer to the parser state.
 * @param expected_type The expected token type.
 * @param error_message_format Format string for the error message.
 * @param ... Additional arguments for the error message format string.
 */
void consume_token(BaaParser *parser, BaaTokenType expected_type, const wchar_t *error_message_format, ...);

/**
 * @brief Reports a parser error at the given token location.
 *
 * @param parser Pointer to the parser state.
 * @param token Pointer to the token where the error occurred.
 * @param message_format Format string for the error message.
 * @param ... Additional arguments for the format string.
 */
void parser_error_at_token(BaaParser *parser, const BaaToken *token, const wchar_t *message_format, ...);

/**
 * @brief Attempts to recover from a syntax error by discarding tokens.
 * Advances tokens until a synchronization point is found or EOF is reached.
 *
 * @param parser Pointer to the parser state.
 */
void synchronize(BaaParser *parser);

#endif // BAA_PARSER_UTILS_INTERNAL_H
