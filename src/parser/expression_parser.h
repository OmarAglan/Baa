#ifndef BAA_EXPRESSION_PARSER_INTERNAL_H
#define BAA_EXPRESSION_PARSER_INTERNAL_H

#include "parser_internal.h" // For BaaParser
#include "baa/ast/ast.h"     // For BaaNode

/**
 * @brief Parses a primary expression (literals, identifiers, parenthesized expressions).
 * This is the highest precedence level in expression parsing.
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the primary expression, or NULL on error.
 */
BaaNode *parse_primary_expression(BaaParser *parser);

/**
 * @brief Parses any expression (entry point for expression parsing).
 * Currently delegates to parse_primary_expression, but will be expanded
 * to handle operator precedence levels.
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the expression, or NULL on error.
 */
BaaNode *parse_expression(BaaParser *parser);

// Add more expression parsing function declarations here as they are implemented
// e.g., parse_unary_expression, parse_binary_expression, etc.

#endif // BAA_EXPRESSION_PARSER_INTERNAL_H
