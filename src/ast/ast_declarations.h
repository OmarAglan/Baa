#ifndef BAA_AST_DECLARATIONS_H
#define BAA_AST_DECLARATIONS_H

#include "baa/ast/ast_types.h"

/**
 * @file ast_declarations.h
 * @brief Internal header for AST declaration node creation and management functions.
 * 
 * This file contains function declarations for creating and managing AST nodes
 * related to declarations (variable declarations, function declarations, etc.).
 * These functions are used internally by the AST implementation and parser.
 */

// --- Variable Declaration Node Functions ---

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_VAR_DECL_STMT.
 * Recursively frees the type_node and initializer_expr if they exist.
 * Frees the duplicated variable name.
 * Finally frees the BaaVarDeclData struct itself.
 *
 * @param data Pointer to the BaaVarDeclData to free. Must not be NULL.
 */
void baa_ast_free_var_decl_data(BaaVarDeclData *data);

#endif // BAA_AST_DECLARATIONS_H
