#ifndef BAA_AST_H
#define BAA_AST_H

#include "baa/ast/ast_types.h" // Include the core type definitions
#include "baa/types/types.h"   // For BaaType
#include <stdbool.h>           // For bool

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
 * @param span The BaaAstSourceSpan indicating the node's location in the source code.
 * @return A pointer to the newly allocated BaaNode, or NULL on allocation failure.
 *         The caller is responsible for populating node->data if necessary.
 */
BaaNode *baa_ast_new_node(BaaNodeKind kind, BaaAstSourceSpan span);

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
void baa_ast_free_node(BaaNode *node);

// We will add more specific node creation function prototypes here later,
// e.g., BaaNode* baa_ast_new_literal_int_node(...);
// For now, just the generic ones.
// --- Specific AST Node Creation Functions ---

// == Literal Expressions ==

/**
 * @brief Creates a new AST node representing an integer literal.
 * The node's kind will be BAA_NODE_KIND_LITERAL_EXPR.
 * Its data will point to a BaaLiteralExprData struct.
 *
 * @param span The source span of the literal.
 * @param value The long long value of the integer literal.
 * @param type A pointer to the canonical BaaType (e.g., baa_type_int, baa_type_long) for this literal.
 *             The AST node does not take ownership of this type pointer.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_literal_int_node(BaaAstSourceSpan span, long long value, BaaType *type);

/**
 * @brief Creates a new AST node representing a string literal.
 * The node's kind will be BAA_NODE_KIND_LITERAL_EXPR.
 * Its data will point to a BaaLiteralExprData struct.
 * The provided string value will be duplicated.
 *
 * @param span The source span of the literal.
 * @param value The wide character string value of the literal. This function will duplicate it.
 * @param type A pointer to the canonical BaaType (e.g., baa_type_string or char_array_type) for this literal.
 *             The AST node does not take ownership of this type pointer.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_literal_string_node(BaaAstSourceSpan span, const wchar_t *value, BaaType *type);

// Add prototypes for other literal types (float, bool, char, null) as needed:
// BaaNode* baa_ast_new_literal_float_node(BaaSourceSpan span, double value, BaaType* type);
// BaaNode* baa_ast_new_literal_bool_node(BaaSourceSpan span, bool value, BaaType* type);
// BaaNode* baa_ast_new_literal_char_node(BaaSourceSpan span, wchar_t value, BaaType* type);
// BaaNode* baa_ast_new_literal_null_node(BaaSourceSpan span, BaaType* type);

// We will also need a specific free function for BaaLiteralExprData's contents.
// This will be declared internally (e.g., in ast_expressions.h if we create it)
// and called by baa_ast_free_node's dispatch.
// Example (internal declaration, not for this public header yet):
// void baa_ast_free_literal_expr_data(BaaLiteralExprData* data);

// == Program Nodes ==

/**
 * @brief Creates a new AST node representing a program (root of the AST).
 * The node's kind will be BAA_NODE_KIND_PROGRAM.
 * Its data will point to a BaaProgramData struct with an empty declarations array.
 *
 * @param span The source span of the program.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_program_node(BaaAstSourceSpan span);

/**
 * @brief Adds a top-level declaration to a program node.
 * Handles dynamic array resizing as needed.
 *
 * @param program_node A BaaNode* of kind BAA_NODE_KIND_PROGRAM.
 * @param declaration_node A BaaNode* representing a top-level declaration.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_declaration_to_program(BaaNode *program_node, BaaNode *declaration_node);

#endif // BAA_AST_H
