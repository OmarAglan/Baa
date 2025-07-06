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

// --- Control Flow Statement Node Creation ---

// --- If Statement Node Creation ---

BaaNode *baa_ast_new_if_stmt_node(BaaAstSourceSpan span, BaaNode *condition_expr, BaaNode *then_stmt, BaaNode *else_stmt)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_IF_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!condition_expr || !then_stmt)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Invalid required parameters
    }

    BaaIfStmtData *data = (BaaIfStmtData *)baa_malloc(sizeof(BaaIfStmtData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->condition_expr = condition_expr;
    data->then_stmt = then_stmt;
    data->else_stmt = else_stmt; // Can be NULL

    node->data = data;
    return node;
}

// --- While Statement Node Creation ---

BaaNode *baa_ast_new_while_stmt_node(BaaAstSourceSpan span, BaaNode *condition_expr, BaaNode *body_stmt)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_WHILE_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!condition_expr || !body_stmt)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Invalid required parameters
    }

    BaaWhileStmtData *data = (BaaWhileStmtData *)baa_malloc(sizeof(BaaWhileStmtData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->condition_expr = condition_expr;
    data->body_stmt = body_stmt;

    node->data = data;
    return node;
}

// --- For Statement Node Creation ---

BaaNode *baa_ast_new_for_stmt_node(BaaAstSourceSpan span, BaaNode *initializer_stmt, BaaNode *condition_expr, BaaNode *increment_expr, BaaNode *body_stmt)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_FOR_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    if (!body_stmt)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;             // Body statement is required
    }

    BaaForStmtData *data = (BaaForStmtData *)baa_malloc(sizeof(BaaForStmtData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->initializer_stmt = initializer_stmt; // Can be NULL
    data->condition_expr = condition_expr;     // Can be NULL
    data->increment_expr = increment_expr;     // Can be NULL
    data->body_stmt = body_stmt;

    node->data = data;
    return node;
}

// --- Return Statement Node Creation ---

BaaNode *baa_ast_new_return_stmt_node(BaaAstSourceSpan span, BaaNode *value_expr)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_RETURN_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    BaaReturnStmtData *data = (BaaReturnStmtData *)baa_malloc(sizeof(BaaReturnStmtData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    data->value_expr = value_expr; // Can be NULL for void returns

    node->data = data;
    return node;
}

// --- Break Statement Node Creation ---

BaaNode *baa_ast_new_break_stmt_node(BaaAstSourceSpan span)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_BREAK_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    // Break statements don't need additional data
    node->data = NULL;
    return node;
}

// --- Continue Statement Node Creation ---

BaaNode *baa_ast_new_continue_stmt_node(BaaAstSourceSpan span)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_CONTINUE_STMT, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    // Continue statements don't need additional data
    node->data = NULL;
    return node;
}

// --- Control Flow Statement Node Data Freeing ---

// --- If Statement Node Data Freeing ---

void baa_ast_free_if_stmt_data(BaaIfStmtData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the condition expression
    if (data->condition_expr)
    {
        baa_ast_free_node(data->condition_expr);
    }

    // Recursively free the then statement
    if (data->then_stmt)
    {
        baa_ast_free_node(data->then_stmt);
    }

    // Recursively free the else statement (if present)
    if (data->else_stmt)
    {
        baa_ast_free_node(data->else_stmt);
    }

    // Free the BaaIfStmtData struct itself
    baa_free(data);
}

// --- While Statement Node Data Freeing ---

void baa_ast_free_while_stmt_data(BaaWhileStmtData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the condition expression
    if (data->condition_expr)
    {
        baa_ast_free_node(data->condition_expr);
    }

    // Recursively free the body statement
    if (data->body_stmt)
    {
        baa_ast_free_node(data->body_stmt);
    }

    // Free the BaaWhileStmtData struct itself
    baa_free(data);
}

// --- For Statement Node Data Freeing ---

void baa_ast_free_for_stmt_data(BaaForStmtData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the initializer statement (if present)
    if (data->initializer_stmt)
    {
        baa_ast_free_node(data->initializer_stmt);
    }

    // Recursively free the condition expression (if present)
    if (data->condition_expr)
    {
        baa_ast_free_node(data->condition_expr);
    }

    // Recursively free the increment expression (if present)
    if (data->increment_expr)
    {
        baa_ast_free_node(data->increment_expr);
    }

    // Recursively free the body statement
    if (data->body_stmt)
    {
        baa_ast_free_node(data->body_stmt);
    }

    // Free the BaaForStmtData struct itself
    baa_free(data);
}

// --- Return Statement Node Data Freeing ---

void baa_ast_free_return_stmt_data(BaaReturnStmtData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free the value expression (if present)
    if (data->value_expr)
    {
        baa_ast_free_node(data->value_expr);
    }

    // Free the BaaReturnStmtData struct itself
    baa_free(data);
}
