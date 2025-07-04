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

// Add other internal statement-related AST function declarations here in the future

#endif // BAA_AST_STATEMENTS_INTERNAL_H
