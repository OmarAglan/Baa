#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"
#include "baa/utils/utils.h"
#include "ast_types.h"
#include <wchar.h>

// == Type Representation Node Creation ==

BaaNode *baa_ast_new_primitive_type_node(BaaAstSourceSpan span, const wchar_t *type_name)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_TYPE, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!type_name)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Invalid type name
    }

    BaaTypeAstData *data = (BaaTypeAstData *)baa_malloc(sizeof(BaaTypeAstData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the data structure
    data->type_ast_kind = BAA_TYPE_AST_KIND_PRIMITIVE;

    // Duplicate the type name
    data->specifier.primitive.name = baa_strdup(type_name);
    if (!data->specifier.primitive.name)
    {
        baa_free(data);
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    node->data = data;
    return node;
}

BaaNode *baa_ast_new_array_type_node(BaaAstSourceSpan span, BaaNode *element_type_node, BaaNode *size_expr)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_TYPE, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!element_type_node)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Invalid element type node
    }

    // Validate that element_type_node is actually a type node
    if (element_type_node->kind != BAA_NODE_KIND_TYPE)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Element type node must be a type node
    }

    BaaTypeAstData *data = (BaaTypeAstData *)baa_malloc(sizeof(BaaTypeAstData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the data structure
    data->type_ast_kind = BAA_TYPE_AST_KIND_ARRAY;
    data->specifier.array.element_type_node = element_type_node;
    data->specifier.array.size_expr = size_expr; // Can be NULL for dynamic arrays

    node->data = data;
    return node;
}

// == Type Representation Node Cleanup ==

void baa_ast_free_type_ast_data(BaaTypeAstData *data)
{
    if (!data)
    {
        return;
    }

    switch (data->type_ast_kind)
    {
    case BAA_TYPE_AST_KIND_PRIMITIVE:
        if (data->specifier.primitive.name)
        {
            baa_free(data->specifier.primitive.name);
        }
        break;

    case BAA_TYPE_AST_KIND_ARRAY:
        // Free the element type node recursively
        if (data->specifier.array.element_type_node)
        {
            baa_ast_free_node(data->specifier.array.element_type_node);
        }
        // Free the size expression node recursively (if present)
        if (data->specifier.array.size_expr)
        {
            baa_ast_free_node(data->specifier.array.size_expr);
        }
        break;

    case BAA_TYPE_AST_KIND_POINTER:
    case BAA_TYPE_AST_KIND_USER_DEFINED:
        // Future implementation
        break;

    default:
        // Unknown type kind - nothing to free
        break;
    }

    baa_free(data);
}
