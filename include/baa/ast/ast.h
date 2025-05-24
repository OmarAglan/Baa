#ifndef BAA_AST_H
#define BAA_AST_H

#include "baa/ast/ast_types.h" // Include the core type definitions

// --- Core AST Node Lifecycle Functions ---

/**
 * @brief Creates a new generic BaaNode.
 *
 * Allocates memory for a BaaNode and initializes its kind and source span.
 * The 'data' field of the newly created node will be initialized to NULL.
 * Specific node creation functions (e.g., baa_ast_new_literal_expr_node)
 * will call this and then allocate and assign the specific data structure.
 *
 * @param kind The BaaNodeKind for the new node.
 * @param span The BaaSourceSpan indicating the node's location in the source code.
 * @return A pointer to the newly allocated BaaNode, or NULL on allocation failure.
 *         The caller is responsible for populating node->data if necessary.
 */
BaaNode* baa_ast_new_node(BaaNodeKind kind, BaaSourceSpan span);

/**
 * @brief Frees a BaaNode and its associated data recursively.
 *
 * This function is the primary way to deallocate AST nodes. It will:
 * 1. Check the node's kind.
 * 2. Call a kind-specific helper function to free the contents of node->data
 *    (which includes freeing any duplicated strings and recursively freeing child BaaNodes).
 * 3. Free the node->data pointer itself (if not NULL).
 * 4. Free the BaaNode structure.
 *
 * It is safe to call this function with a NULL node pointer.
 *
 * @param node The BaaNode to be freed.
 */
void baa_ast_free_node(BaaNode* node);

// We will add more specific node creation function prototypes here later,
// e.g., BaaNode* baa_ast_new_literal_int_node(...);
// For now, just the generic ones.

#endif // BAA_AST_H
