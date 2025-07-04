#ifndef BAA_AST_EXPRESSIONS_INTERNAL_H
#define BAA_AST_EXPRESSIONS_INTERNAL_H

#include "baa/ast/ast_types.h" // For BaaLiteralExprData

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_LITERAL_EXPR.
 * Specifically, it frees the duplicated string if the literal is a string.
 * It then frees the BaaLiteralExprData struct itself.
 *
 * @param data Pointer to the BaaLiteralExprData to free. Must not be NULL.
 */
void baa_ast_free_literal_expr_data(BaaLiteralExprData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_IDENTIFIER_EXPR.
 * Specifically, it frees the duplicated identifier name.
 * It then frees the BaaIdentifierExprData struct itself.
 *
 * @param data Pointer to the BaaIdentifierExprData to free. Must not be NULL.
 */
void baa_ast_free_identifier_expr_data(BaaIdentifierExprData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_BINARY_EXPR.
 * Recursively frees the left and right operands.
 * It then frees the BaaBinaryExprData struct itself.
 *
 * @param data Pointer to the BaaBinaryExprData to free. Must not be NULL.
 */
void baa_ast_free_binary_expr_data(BaaBinaryExprData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_UNARY_EXPR.
 * Recursively frees the operand.
 * It then frees the BaaUnaryExprData struct itself.
 *
 * @param data Pointer to the BaaUnaryExprData to free. Must not be NULL.
 */
void baa_ast_free_unary_expr_data(BaaUnaryExprData *data);

// Add other internal expression-related AST function declarations here in the future

#endif // BAA_AST_EXPRESSIONS_INTERNAL_H
