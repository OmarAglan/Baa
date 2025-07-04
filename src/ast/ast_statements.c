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
        return NULL;             // Invalid expression node
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

// --- Block Statement Node Creation ---

BaaNode *baa_ast_new_block_stmt_node(BaaAstSourceSpan span)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_BLOCK_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    BaaBlockStmtData *data = (BaaBlockStmtData *)baa_malloc(sizeof(BaaBlockStmtData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the dynamic array
    data->statements = NULL;
    data->count = 0;
    data->capacity = 0;

    node->data = data;
    return node;
}

// --- Block Statement Node Utility Functions ---

/**
 * @brief Adds a statement to a block statement node.
 * Handles dynamic array resizing as needed.
 *
 * @param block_node A BaaNode* of kind BAA_NODE_KIND_BLOCK_STMT.
 * @param statement_node A BaaNode* representing a statement.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_stmt_to_block(BaaNode *block_node, BaaNode *statement_node)
{
    if (!block_node || block_node->kind != BAA_NODE_KIND_BLOCK_STMT || !block_node->data)
    {
        return false; // Invalid block node
    }

    if (!statement_node)
    {
        return false; // Invalid statement node
    }

    BaaBlockStmtData *data = (BaaBlockStmtData *)block_node->data;

    // Check if we need to resize the array
    if (data->count >= data->capacity)
    {
        size_t new_capacity = data->capacity == 0 ? 4 : data->capacity * 2;
        BaaNode **new_statements = (BaaNode **)baa_realloc(
            data->statements,
            new_capacity * sizeof(BaaNode *));
        if (!new_statements)
        {
            return false; // Memory allocation failed
        }

        data->statements = new_statements;
        data->capacity = new_capacity;
    }

    // Add the statement
    data->statements[data->count] = statement_node;
    data->count++;

    return true;
}

// --- Statement Node Data Freeing ---

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

// --- Block Statement Node Data Freeing ---

void baa_ast_free_block_stmt_data(BaaBlockStmtData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free all statements
    if (data->statements)
    {
        for (size_t i = 0; i < data->count; i++)
        {
            if (data->statements[i])
            {
                baa_ast_free_node(data->statements[i]);
            }
        }
        baa_free(data->statements);
    }

    // Free the BaaBlockStmtData struct itself
    baa_free(data);
}
