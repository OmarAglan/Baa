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

// Add other internal expression-related AST function declarations here in the future

#endif // BAA_AST_EXPRESSIONS_INTERNAL_H
