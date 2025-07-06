#ifndef BAA_AST_STATEMENTS_INTERNAL_H
#define BAA_AST_STATEMENTS_INTERNAL_H

#include "baa/ast/ast_types.h" // For BaaExprStmtData

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_EXPR_STMT.
 * Recursively frees the expression and then frees the BaaExprStmtData struct itself.
 *
 * @param data Pointer to the BaaExprStmtData to free. Must not be NULL.
 */
void baa_ast_free_expr_stmt_data(BaaExprStmtData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_BLOCK_STMT.
 * Recursively frees all statements and then frees the BaaBlockStmtData struct itself.
 *
 * @param data Pointer to the BaaBlockStmtData to free. Must not be NULL.
 */
void baa_ast_free_block_stmt_data(BaaBlockStmtData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_IF_STMT.
 * Recursively frees the condition, then statement, and optional else statement.
 *
 * @param data Pointer to the BaaIfStmtData to free. Must not be NULL.
 */
void baa_ast_free_if_stmt_data(BaaIfStmtData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_WHILE_STMT.
 * Recursively frees the condition and body statement.
 *
 * @param data Pointer to the BaaWhileStmtData to free. Must not be NULL.
 */
void baa_ast_free_while_stmt_data(BaaWhileStmtData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_FOR_STMT.
 * Recursively frees the initializer, condition, increment, and body.
 *
 * @param data Pointer to the BaaForStmtData to free. Must not be NULL.
 */
void baa_ast_free_for_stmt_data(BaaForStmtData *data);

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_RETURN_STMT.
 * Recursively frees the optional return value expression.
 *
 * @param data Pointer to the BaaReturnStmtData to free. Must not be NULL.
 */
void baa_ast_free_return_stmt_data(BaaReturnStmtData *data);

// Add other internal statement-related AST function declarations here in the future

#endif // BAA_AST_STATEMENTS_INTERNAL_H
