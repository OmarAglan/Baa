#ifndef BAA_AST_TYPES_INTERNAL_H
#define BAA_AST_TYPES_INTERNAL_H

#include "baa/ast/ast_types.h"

/**
 * @file ast_types.h
 * @brief Internal header for AST type representation node functions.
 * 
 * This header contains internal function declarations for managing
 * type representation AST nodes. These functions are used internally
 * by the AST system and are not part of the public API.
 */

// == Internal Type AST Data Management ==

/**
 * @brief Frees the data associated with a BaaTypeAstData structure.
 * 
 * This function recursively frees all memory associated with a BaaTypeAstData
 * structure, including any child nodes (for array types) and duplicated strings
 * (for primitive types).
 * 
 * @param data The BaaTypeAstData structure to free. Can be NULL.
 */
void baa_ast_free_type_ast_data(BaaTypeAstData *data);

#endif // BAA_AST_TYPES_INTERNAL_H
