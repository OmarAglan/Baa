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

// --- Parameter Node Functions ---

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_PARAMETER.
 * Recursively frees the type_node if it exists.
 * Frees the duplicated parameter name.
 * Finally frees the BaaParameterData struct itself.
 *
 * @param data Pointer to the BaaParameterData to free. Must not be NULL.
 */
void baa_ast_free_parameter_data(BaaParameterData *data);

// --- Function Definition Node Functions ---

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_FUNCTION_DEF.
 * Recursively frees the return_type_node, all parameter nodes, and body if they exist.
 * Frees the duplicated function name and parameters array.
 * Finally frees the BaaFunctionDefData struct itself.
 *
 * @param data Pointer to the BaaFunctionDefData to free. Must not be NULL.
 */
void baa_ast_free_function_def_data(BaaFunctionDefData *data);

#endif // BAA_AST_DECLARATIONS_H
