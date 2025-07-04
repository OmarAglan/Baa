// src/ast/ast_program.c
#include "baa/ast/ast.h"       // For BaaNode, BaaAstSourceSpan, specific node creation prototypes
#include "baa/ast/ast_types.h" // For BaaNodeKind, BaaProgramData
#include "ast_program.h"       // For internal prototypes like baa_ast_free_program_data

#include "baa/utils/utils.h" // For baa_malloc, baa_free

#include <stdlib.h> // For NULL
#include <string.h> // For memset

// --- Program Node Creation ---

BaaNode *baa_ast_new_program_node(BaaAstSourceSpan span)
{
    BaaNode *node = baa_ast_new_node(BAA_NODE_KIND_PROGRAM, span);
    if (!node)
    {
        return NULL; // Allocation for BaaNode failed
    }

    BaaProgramData *data = (BaaProgramData *)baa_malloc(sizeof(BaaProgramData));
    if (!data)
    {
        baa_ast_free_node(node); // Clean up the partially created BaaNode
        return NULL;
    }

    // Initialize the dynamic array
    data->top_level_declarations = NULL;
    data->count = 0;
    data->capacity = 0;

    node->data = data;
    return node;
}

// --- Program Node Utility Functions ---

/**
 * @brief Adds a top-level declaration to a program node.
 * Handles dynamic array resizing as needed.
 *
 * @param program_node A BaaNode* of kind BAA_NODE_KIND_PROGRAM.
 * @param declaration_node A BaaNode* representing a top-level declaration.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_declaration_to_program(BaaNode *program_node, BaaNode *declaration_node)
{
    if (!program_node || program_node->kind != BAA_NODE_KIND_PROGRAM || !program_node->data)
    {
        return false; // Invalid program node
    }

    if (!declaration_node)
    {
        return false; // Invalid declaration node
    }

    BaaProgramData *data = (BaaProgramData *)program_node->data;

    // Check if we need to resize the array
    if (data->count >= data->capacity)
    {
        size_t new_capacity = data->capacity == 0 ? 4 : data->capacity * 2;
        BaaNode **new_declarations = (BaaNode **)baa_realloc(
            data->top_level_declarations,
            new_capacity * sizeof(BaaNode *)
        );
        if (!new_declarations)
        {
            return false; // Memory allocation failed
        }

        data->top_level_declarations = new_declarations;
        data->capacity = new_capacity;
    }

    // Add the declaration
    data->top_level_declarations[data->count] = declaration_node;
    data->count++;

    return true;
}

// --- Program Node Data Freeing ---

void baa_ast_free_program_data(BaaProgramData *data)
{
    if (!data)
    {
        return;
    }

    // Recursively free all top-level declarations
    if (data->top_level_declarations)
    {
        for (size_t i = 0; i < data->count; i++)
        {
            if (data->top_level_declarations[i])
            {
                baa_ast_free_node(data->top_level_declarations[i]);
            }
        }
        baa_free(data->top_level_declarations);
    }

    // Free the BaaProgramData struct itself
    baa_free(data);
}
