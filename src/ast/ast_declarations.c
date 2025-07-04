#include "ast_declarations.h"
#include "baa/ast/ast.h"
#include "baa/utils/utils.h"

/**
 * @file ast_declarations.c
 * @brief Implementation of AST declaration node creation and management functions.
 *
 * This file implements functions for creating and managing AST nodes related to
 * declarations (variable declarations, function declarations, etc.).
 */

// --- Variable Declaration Node Creation ---

BaaNode *baa_ast_new_var_decl_node(BaaAstSourceSpan span, const wchar_t *name,
                                   BaaAstNodeModifiers modifiers, BaaNode *type_node,
                                   BaaNode *initializer_expr)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_VAR_DECL_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    BaaVarDeclData *data = (BaaVarDeclData *)baa_malloc(sizeof(BaaVarDeclData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the data structure
    data->name = baa_strdup(name); // Duplicate the variable name
    if (!data->name && name != NULL)
    { // Check if baa_strdup failed for non-NULL input
        baa_free(data);
        baa_ast_free_node(node);
        return NULL;
    }

    data->modifiers = modifiers;
    data->type_node = type_node;               // Take ownership of the type node
    data->initializer_expr = initializer_expr; // Take ownership of the initializer expression (can be NULL)

    node->data = data;
    return node;
}

// --- Variable Declaration Node Data Freeing ---

void baa_ast_free_var_decl_data(BaaVarDeclData *data)
{
    if (!data)
    {
        return;
    }

    // Free the duplicated variable name
    if (data->name)
    {
        baa_free(data->name);
    }

    // Recursively free the type node
    if (data->type_node)
    {
        baa_ast_free_node(data->type_node);
    }

    // Recursively free the initializer expression (if present)
    if (data->initializer_expr)
    {
        baa_ast_free_node(data->initializer_expr);
    }

    baa_free(data); // Free the BaaVarDeclData struct itself
}
