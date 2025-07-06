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

/**
 * @brief Parses an if statement (إذا condition statement [وإلا statement]).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the if statement, or NULL on error.
 */
BaaNode *parse_if_statement(BaaParser *parser);

/**
 * @brief Parses a while statement (طالما condition statement).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the while statement, or NULL on error.
 */
BaaNode *parse_while_statement(BaaParser *parser);

/**
 * @brief Parses a for statement (لكل (init; condition; increment) statement).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the for statement, or NULL on error.
 */
BaaNode *parse_for_statement(BaaParser *parser);

/**
 * @brief Parses a return statement (إرجع [expression].).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the return statement, or NULL on error.
 */
BaaNode *parse_return_statement(BaaParser *parser);

/**
 * @brief Parses a break statement (توقف.).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the break statement, or NULL on error.
 */
BaaNode *parse_break_statement(BaaParser *parser);

/**
 * @brief Parses a continue statement (استمر.).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the continue statement, or NULL on error.
 */
BaaNode *parse_continue_statement(BaaParser *parser);

// Add more statement parsing function declarations here as they are implemented

#endif // BAA_STATEMENT_PARSER_INTERNAL_H
