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

/**
 * @brief Parses a unary expression (unary operators + primary expressions).
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the unary expression, or NULL on error.
 */
BaaNode *parse_unary_expression(BaaParser *parser);

/**
 * @brief Parses binary expressions using precedence climbing.
 * This handles all binary operators with proper precedence and associativity.
 *
 * @param parser Pointer to the parser state.
 * @param min_precedence Minimum precedence level to parse.
 * @param left_expr The left operand expression.
 * @return A BaaNode* representing the binary expression, or NULL on error.
 */
BaaNode *parse_binary_expression_rhs(BaaParser *parser, int min_precedence, BaaNode *left_expr);

/**
 * @brief Parses a postfix expression (function calls, array access, member access).
 * This handles expressions that come after a primary expression.
 *
 * @param parser Pointer to the parser state.
 * @param base_expr The base expression to apply postfix operations to.
 * @return A BaaNode* representing the postfix expression, or NULL on error.
 */
BaaNode *parse_postfix_expression(BaaParser *parser, BaaNode *base_expr);

/**
 * @brief Parses a function call expression with arguments.
 * Expected syntax: callee_expr '(' [argument (',' argument)*] ')'
 *
 * @param parser Pointer to the parser state.
 * @param callee_expr The callee expression (function to call).
 * @return A BaaNode* of kind BAA_NODE_KIND_CALL_EXPR representing the function call, or NULL on error.
 */
BaaNode *parse_call_expression(BaaParser *parser, BaaNode *callee_expr);

#endif // BAA_EXPRESSION_PARSER_INTERNAL_H
