#ifndef BAA_STATEMENT_PARSER_INTERNAL_H
#define BAA_STATEMENT_PARSER_INTERNAL_H

#include "parser_internal.h" // For BaaParser
#include "baa/ast/ast.h"     // For BaaNode

/**
 * @brief Parses an expression statement (expression followed by dot terminator).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the expression statement, or NULL on error.
 */
BaaNode *parse_expression_statement(BaaParser *parser);

/**
 * @brief Parses a statement (dispatcher function).
 * Determines the type of statement based on the current token and calls
 * the appropriate parsing function.
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the statement, or NULL on error.
 */
BaaNode *parse_statement(BaaParser *parser);

/**
 * @brief Parses a block statement ({ statement* }).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the block statement, or NULL on error.
 */
BaaNode *parse_block_statement(BaaParser *parser);

// Add more statement parsing function declarations here as they are implemented
// e.g., parse_if_statement, parse_while_statement, etc.

#endif // BAA_STATEMENT_PARSER_INTERNAL_H
