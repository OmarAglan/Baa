// src/ast/ast_statements.c
#include "baa/ast/ast.h"       // For BaaNode, BaaAstSourceSpan, specific node creation prototypes
#include "baa/ast/ast_types.h" // For BaaNodeKind, BaaExprStmtData
#include "ast_statements.h"    // For internal prototypes like baa_ast_free_expr_stmt_data

#include "baa/utils/utils.h" // For baa_malloc, baa_free

#include <stdlib.h> // For NULL

// --- Expression Statement Node Creation ---

BaaNode *baa_ast_new_expr_stmt_node(BaaAstSourceSpan span, BaaNode *expression_node)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_EXPR_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!expression_node)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL; // Invalid expression node
    }

    BaaExprStmtData *data = (BaaExprStmtData *)baa_malloc(sizeof(BaaExprStmtData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->expression = expression_node;

    node->data = data;
    return node;
}

// --- Expression Statement Node Data Freeing ---

void baa_ast_free_expr_stmt_data(BaaExprStmtData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the expression
    if (data->expression)
    {
        baa_ast_free_node(data->expression);
    }

    // Free the BaaExprStmtData struct itself
    baa_free(data);
}
