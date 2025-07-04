#ifndef BAA_AST_PROGRAM_INTERNAL_H
#define BAA_AST_PROGRAM_INTERNAL_H

#include "baa/ast/ast_types.h" // For BaaProgramData

/**
 * @brief Frees the data associated with a BAA_NODE_KIND_PROGRAM.
 * Recursively frees all top-level declarations and then frees the BaaProgramData struct itself.
 *
 * @param data Pointer to the BaaProgramData to free. Must not be NULL.
 */
void baa_ast_free_program_data(BaaProgramData *data);

// Add other internal program-related AST function declarations here in the future

#endif // BAA_AST_PROGRAM_INTERNAL_H
