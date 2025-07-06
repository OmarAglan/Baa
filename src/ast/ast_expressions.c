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

// --- Identifier Expression Node Creation ---

BaaNode *baa_ast_new_identifier_expr_node(BaaAstSourceSpan span, const wchar_t *name)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_IDENTIFIER_EXPR, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    BaaIdentifierExprData *data = (BaaIdentifierExprData *)baa_malloc(sizeof(BaaIdentifierExprData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->name = baa_strdup(name); // Duplicate the identifier name
    if (!data->name && name != NULL)
    { // Check if baa_strdup failed for non-NULL input
        baa_free(data);
        baa_ast_free_node(node);
        return NULL;
    }

    node->data = data;
    return node;
}

// --- Binary Expression Node Creation ---

BaaNode *baa_ast_new_binary_expr_node(BaaAstSourceSpan span, BaaNode *left_operand, BaaNode *right_operand, BaaBinaryOperatorKind operator_kind)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_BINARY_EXPR, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!left_operand || !right_operand)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Invalid operands
    }

    BaaBinaryExprData *data = (BaaBinaryExprData *)baa_malloc(sizeof(BaaBinaryExprData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->left_operand = left_operand;
    data->right_operand = right_operand;
    data->operator_kind = operator_kind;

    node->data = data;
    return node;
}

// --- Unary Expression Node Creation ---

BaaNode *baa_ast_new_unary_expr_node(BaaAstSourceSpan span, BaaNode *operand, BaaUnaryOperatorKind operator_kind)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_UNARY_EXPR, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!operand)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Invalid operand
    }

    BaaUnaryExprData *data = (BaaUnaryExprData *)baa_malloc(sizeof(BaaUnaryExprData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->operand = operand;
    data->operator_kind = operator_kind;

    node->data = data;
    return node;
}

// --- Call Expression Node Creation ---

BaaNode *baa_ast_new_call_expr_node(BaaAstSourceSpan span, BaaNode *callee_expr)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_CALL_EXPR, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!callee_expr)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Callee expression is required
    }

    BaaCallExprData *data = (BaaCallExprData *)baa_malloc(sizeof(BaaCallExprData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the data structure
    data->callee_expr = callee_expr; // Take ownership of the callee expression
    data->arguments = NULL;          // Initialize empty arguments array
    data->argument_count = 0;
    data->argument_capacity = 0;

    node->data = data;
    return node;
}

// --- Call Expression Node Utility Functions ---

bool baa_ast_add_call_argument(BaaNode *call_expr_node, BaaNode *argument_node)
{
    if (!call_expr_node || !argument_node)
    {
        return false; // Invalid input
    }

    // Validate node type
    if (call_expr_node->kind != BAA_NODE_KIND_CALL_EXPR)
    {
        return false; // Not a call expression node
    }

    BaaCallExprData *data = (BaaCallExprData *)call_expr_node->data;
    if (!data)
    {
        return false; // Invalid call expression data
    }

    // Check if we need to resize the arguments array
    if (data->argument_count >= data->argument_capacity)
    {
        size_t new_capacity = (data->argument_capacity == 0) ? 4 : data->argument_capacity * 2;
        BaaNode **new_arguments = (BaaNode **)baa_realloc(data->arguments, new_capacity * sizeof(BaaNode *));
        if (!new_arguments)
        {
            return false; // Memory allocation failed
        }
        data->arguments = new_arguments;
        data->argument_capacity = new_capacity;
    }

    // Add the argument to the array
    data->arguments[data->argument_count] = argument_node;
    data->argument_count++;

    return true;
}

// --- Expression Node Data Freeing ---

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

// --- Identifier Expression Node Data Freeing ---

void baa_ast_free_identifier_expr_data(BaaIdentifierExprData *data)
{
    if (!data)
    {
        return;
    }

    if (data->name)
    {
        baa_free(data->name); // Free the duplicated identifier name
    }

    baa_free(data); // Free the BaaIdentifierExprData struct itself
}

// --- Binary Expression Node Data Freeing ---

void baa_ast_free_binary_expr_data(BaaBinaryExprData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the operands
    if (data->left_operand)
    {
        baa_ast_free_node(data->left_operand);
    }

    if (data->right_operand)
    {
        baa_ast_free_node(data->right_operand);
    }

    baa_free(data); // Free the BaaBinaryExprData struct itself
}

// --- Unary Expression Node Data Freeing ---

void baa_ast_free_unary_expr_data(BaaUnaryExprData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the operand
    if (data->operand)
    {
        baa_ast_free_node(data->operand);
    }

    baa_free(data); // Free the BaaUnaryExprData struct itself
}

// --- Call Expression Node Data Freeing ---

void baa_ast_free_call_expr_data(BaaCallExprData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the callee expression
    if (data->callee_expr)
    {
        baa_ast_free_node(data->callee_expr);
    }

    // Recursively free all argument expressions
    if (data->arguments)
    {
        for (size_t i = 0; i < data->argument_count; i++)
        {
            if (data->arguments[i])
            {
                baa_ast_free_node(data->arguments[i]);
            }
        }
        baa_free(data->arguments); // Free the arguments array itself
    }

    // Free the data structure itself
    baa_free(data);
}
