// src/ast/ast_expressions.c
#include "baa/ast/ast.h"       // For BaaNode, BaaAstSourceSpan, specific node creation prototypes
#include "baa/ast/ast_types.h" // For BaaNodeKind, BaaLiteralExprData, BaaLiteralKind
#include "ast_expressions.h"   // For internal prototypes like baa_ast_free_literal_expr_data

#include "baa/utils/utils.h" // For baa_malloc, baa_free, baa_strdup
#include "baa/types/types.h" // For BaaType (used as non-owned pointer)

#include <stdlib.h> // For NULL
#include <string.h> // For string operations if needed (baa_strdup handles wchar)

// --- Literal Expression Node Creation ---

BaaNode *baa_ast_new_literal_int_node(BaaAstSourceSpan span, long long value, BaaType *type)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_LITERAL_EXPR, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    BaaLiteralExprData *data = (BaaLiteralExprData *)baa_malloc(sizeof(BaaLiteralExprData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->literal_kind = BAA_LITERAL_KIND_INT;
    data->value.int_value = value;
    data->determined_type = type; // Store the canonical type
    // data->original_lexeme = NULL; // If we add this field

    node->data = data;
    return node;
}

BaaNode *baa_ast_new_literal_string_node(BaaAstSourceSpan span, const wchar_t *value, BaaType *type)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_LITERAL_EXPR, span);
    if (!node)
    {
        return NULL;
    }

    BaaLiteralExprData *data = (BaaLiteralExprData *)baa_malloc(sizeof(BaaLiteralExprData));
    if (!data)
    {
        baa_ast_free_node(node);
        return NULL;
    }

    data->literal_kind = BAA_LITERAL_KIND_STRING;
    data->value.string_value = baa_strdup(value); // Duplicate the string
    if (!data->value.string_value && value != NULL)
    { // Check if baa_strdup failed for non-NULL input
        baa_free(data);
        baa_ast_free_node(node);
        return NULL;
    }
    data->determined_type = type;
    // data->original_lexeme = NULL;

    node->data = data;
    return node;
}

// Implement baa_ast_new_literal_float_node, _bool_node, _char_node, _null_node similarly later...

// --- Literal Expression Node Data Freeing ---

void baa_ast_free_literal_expr_data(BaaLiteralExprData *data)
{
    if (!data)
    {
        return;
    }

    if (data->literal_kind == BAA_LITERAL_KIND_STRING && data->value.string_value)
    {
        baa_free(data->value.string_value); // Free the duplicated string
    }
    // if (data->original_lexeme) { // If we add this field
    //     baa_free(data->original_lexeme);
    // }

    // Note: data->determined_type is a non-owned pointer to a canonical type, so we don't free it here.

    baa_free(data); // Free the BaaLiteralExprData struct itself
}
