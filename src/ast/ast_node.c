#include "baa/ast/ast.h"     // For BaaNode, BaaNodeKind, BaaAstSourceSpan, and function prototypes
#include "ast_expressions.h" // For baa_ast_free_literal_expr_data (internal header)
#include "ast_program.h"     // For baa_ast_free_program_data (internal header)
#include "ast_statements.h"  // For baa_ast_free_expr_stmt_data (internal header)
#include "ast_types.h"       // For baa_ast_free_type_ast_data (internal header)
#include "baa/utils/utils.h" // For baa_malloc, baa_free
#include <stdlib.h>          // For NULL
#include <string.h>          // For memset (optional, for zeroing memory)

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
BaaNode *baa_ast_new_node(BaaNodeKind kind, BaaAstSourceSpan span)
{
    BaaNode *node = (BaaNode *)baa_malloc(sizeof(BaaNode));
    if (!node)
    {
        // Optional: Log an error or set a global error flag if baa_malloc fails
        // For now, returning NULL is the primary indication of failure.
        return NULL;
    }

    // Initialize fields
    node->kind = kind;
    node->span = span; // Struct copy
    node->data = NULL; // Explicitly set data to NULL

    // Optional: Initialize future fields to default/zero values if added
    // node->resolved_type = NULL;
    // node->parent_node = NULL;
    // node->unique_id = 0; // Or some unique ID generation logic

    return node;
}

/**
 * @brief Frees a BaaNode and its associated data recursively.
 *
 * This function is the primary way to deallocate AST nodes. It will:
 * 1. Handle NULL node pointer gracefully.
 * 2. Check the node's kind.
 * 3. Call a kind-specific helper function (to be implemented progressively) to free
 *    the contents of node->data (which includes freeing any duplicated strings
 *    and recursively freeing child BaaNodes).
 * 4. Free the node->data pointer itself (if not NULL and not already freed by helper).
 * 5. Free the BaaNode structure.
 *
 * @param node The BaaNode to be freed.
 */
void baa_ast_free_node(BaaNode *node)
{
    if (!node)
    {
        return; // Safe to call with NULL
    }

    // Dispatch to kind-specific data freeing functions
    // This switch will be expanded as we add more node kinds and their data structs.
    switch (node->kind)
    {
    // --- Utility/Placeholder Kinds ---
    case BAA_NODE_KIND_UNKNOWN:
        // No specific data to free for BAA_NODE_KIND_UNKNOWN if node->data is NULL
        // If it could have data, a helper would be called here.
        break;

    // --- Program Structure Kinds ---
    case BAA_NODE_KIND_PROGRAM:
        if (node->data)
        {
            baa_ast_free_program_data((BaaProgramData *)node->data);
        }
        break;

    // --- Expression Kinds ---
    case BAA_NODE_KIND_LITERAL_EXPR:
        if (node->data)
        {
            baa_ast_free_literal_expr_data((BaaLiteralExprData *)node->data);
        }
        break;

    case BAA_NODE_KIND_IDENTIFIER_EXPR:
        if (node->data)
        {
            baa_ast_free_identifier_expr_data((BaaIdentifierExprData *)node->data);
        }
        break;

    case BAA_NODE_KIND_BINARY_EXPR:
        if (node->data)
        {
            baa_ast_free_binary_expr_data((BaaBinaryExprData *)node->data);
        }
        break;

    case BAA_NODE_KIND_UNARY_EXPR:
        if (node->data)
        {
            baa_ast_free_unary_expr_data((BaaUnaryExprData *)node->data);
        }
        break;

    // --- Statement Kinds ---
    case BAA_NODE_KIND_EXPR_STMT:
        if (node->data)
        {
            baa_ast_free_expr_stmt_data((BaaExprStmtData *)node->data);
        }
        break;

    case BAA_NODE_KIND_BLOCK_STMT:
        if (node->data)
        {
            baa_ast_free_block_stmt_data((BaaBlockStmtData *)node->data);
        }
        break;

    // --- Type Representation Kinds ---
    case BAA_NODE_KIND_TYPE:
        if (node->data)
        {
            baa_ast_free_type_ast_data((BaaTypeAstData *)node->data);
        }
        break;

        // Add cases for BAA_NODE_KIND_FUNCTION_DEF, BAA_NODE_KIND_PARAMETER,
        // BAA_NODE_KIND_VAR_DECL_STMT, etc., as they are implemented.
        // Each case will call a specific `baa_ast_free_..._data()` helper.

    default:
        // If a new kind is added and its data freeing logic is missed here,
        // node->data might leak if it's not NULL.
        // For robust handling, we might log a warning in debug builds
        // if node->data is non-NULL for an unhandled kind.
        // fprintf(stderr, "Warning: baa_ast_free_node - Unhandled kind %d with data %p\n", node->kind, node->data);
        break;
    }

    // Free the specific data structure itself, if it exists and wasn't freed by a helper
    // (Convention will be that helpers free their *contents* but not the data struct itself,
    //  allowing this generic part to free the data pointer).
    // However, it's often cleaner if the specific free_xxx_data also frees the data struct.
    // Let's adopt the convention that free_xxx_data frees the data struct it's passed.
    // So, the call to free(node->data) here might be redundant IF specific free functions handle it.
    // For now, we will assume specific free functions will free the data*.
    // This part needs careful coordination.
    // For this initial step, if kind-specific free functions are not yet implemented,
    // we should free `node->data` if it's not NULL and not handled by a specific case.
    // This is a temporary measure until all specific free functions are in place.
    // With the convention that specific free_xxx_data functions free the data struct,
    // node->data should be NULL here if it was handled by a specific case.
    if (node->data && node->kind == BAA_NODE_KIND_UNKNOWN)
    { // Only for truly unhandled data in UNKNOWN
        // If we reach here and node->data is not NULL, it means no specific
        // free function was called (or the specific function didn't free the data struct itself).
        // This is a potential leak or a design choice to be clarified.
        // For this initial step, let's just free it if it's not handled above.
        // This will change as we add specific free_xxx_data functions.
        baa_free(node->data); // General free for BAA_NODE_KIND_UNKNOWN's data if it exists
        node->data = NULL;    // Avoid double free if logic changes
    }

    // Finally, free the BaaNode structure itself
    baa_free(node);
}
