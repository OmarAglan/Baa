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

// --- Parameter Node Creation ---

BaaNode *baa_ast_new_parameter_node(BaaAstSourceSpan span, const wchar_t *name, BaaNode *type_node)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_PARAMETER, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!type_node)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Type node is required
    }

    // Validate that type_node is actually a type node
    if (type_node->kind != BAA_NODE_KIND_TYPE)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Type node must be a type node
    }

    BaaParameterData *data = (BaaParameterData *)baa_malloc(sizeof(BaaParameterData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the data structure
    data->name = baa_strdup(name); // Duplicate the parameter name
    if (!data->name && name != NULL)
    { // Check if baa_strdup failed for non-NULL input
        baa_free(data);
        baa_ast_free_node(node);
        return NULL;
    }

    data->type_node = type_node; // Take ownership of the type node

    node->data = data;
    return node;
}

// --- Function Definition Node Creation ---

BaaNode *baa_ast_new_function_def_node(BaaAstSourceSpan span, const wchar_t *name,
                                       BaaAstNodeModifiers modifiers, BaaNode *return_type_node,
                                       BaaNode *body, bool is_variadic)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_FUNCTION_DEF, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!return_type_node || !body)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Return type and body are required
    }

    // Validate that return_type_node is actually a type node
    if (return_type_node->kind != BAA_NODE_KIND_TYPE)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Return type node must be a type node
    }

    // Validate that body is actually a block statement
    if (body->kind != BAA_NODE_KIND_BLOCK_STMT)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Body must be a block statement
    }

    BaaFunctionDefData *data = (BaaFunctionDefData *)baa_malloc(sizeof(BaaFunctionDefData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the data structure
    data->name = baa_strdup(name); // Duplicate the function name
    if (!data->name && name != NULL)
    { // Check if baa_strdup failed for non-NULL input
        baa_free(data);
        baa_ast_free_node(node);
        return NULL;
    }

    data->modifiers = modifiers;
    data->return_type_node = return_type_node; // Take ownership of the return type node
    data->parameters = NULL;                   // Initialize empty parameters array
    data->parameter_count = 0;
    data->parameter_capacity = 0;
    data->body = body;                         // Take ownership of the body
    data->is_variadic = is_variadic;

    node->data = data;
    return node;
}

// --- Function Definition Node Utility Functions ---

bool baa_ast_add_function_parameter(BaaNode *function_def_node, BaaNode *parameter_node)
{
    if (!function_def_node || !parameter_node)
    {
        return false; // Invalid input
    }

    // Validate node types
    if (function_def_node->kind != BAA_NODE_KIND_FUNCTION_DEF)
    {
        return false; // Not a function definition node
    }

    if (parameter_node->kind != BAA_NODE_KIND_PARAMETER)
    {
        return false; // Not a parameter node
    }

    BaaFunctionDefData *data = (BaaFunctionDefData *)function_def_node->data;
    if (!data)
    {
        return false; // Invalid function definition data
    }

    // Check if we need to resize the parameters array
    if (data->parameter_count >= data->parameter_capacity)
    {
        size_t new_capacity = (data->parameter_capacity == 0) ? 4 : data->parameter_capacity * 2;
        BaaNode **new_parameters = (BaaNode **)baa_realloc(data->parameters, new_capacity * sizeof(BaaNode *));
        if (!new_parameters)
        {
            return false; // Memory allocation failed
        }
        data->parameters = new_parameters;
        data->parameter_capacity = new_capacity;
    }

    // Add the parameter to the array
    data->parameters[data->parameter_count] = parameter_node;
    data->parameter_count++;

    return true;
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

// --- Parameter Node Data Freeing ---

void baa_ast_free_parameter_data(BaaParameterData *data)
{
    if (!data)
    {
        return;
    }

    // Free the duplicated parameter name
    if (data->name)
    {
        baa_free(data->name);
    }

    // Recursively free the type node
    if (data->type_node)
    {
        baa_ast_free_node(data->type_node);
    }

    // Free the data structure itself
    baa_free(data);
}

// --- Function Definition Node Data Freeing ---

void baa_ast_free_function_def_data(BaaFunctionDefData *data)
{
    if (!data)
    {
        return;
    }

    // Free the duplicated function name
    if (data->name)
    {
        baa_free(data->name);
    }

    // Recursively free the return type node
    if (data->return_type_node)
    {
        baa_ast_free_node(data->return_type_node);
    }

    // Recursively free all parameter nodes
    if (data->parameters)
    {
        for (size_t i = 0; i < data->parameter_count; i++)
        {
            if (data->parameters[i])
            {
                baa_ast_free_node(data->parameters[i]);
            }
        }
        baa_free(data->parameters); // Free the parameters array itself
    }

    // Recursively free the function body
    if (data->body)
    {
        baa_ast_free_node(data->body);
    }

    // Free the data structure itself
    baa_free(data);
}
