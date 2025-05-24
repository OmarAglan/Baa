#include "baa/ast/ast.h"     // For BaaNode, BaaNodeKind, BaaSourceSpan, etc.
#include "baa/utils/utils.h" // For baa_malloc, baa_free (if AST uses them directly, though it should use its own for node management)
#include <stdio.h>
#include <wchar.h>
#include <locale.h>

// Helper to print a node's basic info (will be very simple for now)
void print_node_info(const BaaNode *node, const char *description)
{
    if (!node)
    {
        wprintf(L"%hs: Node is NULL\n", description);
        return;
    }
    // In the future, we can have a BaaNodeKind_to_string function
    wprintf(L"%hs: Node Kind: %d, Span: (%s L%zu C%zu - %s L%zu C%zu), Data: %p\n",
            description,
            node->kind,
            node->span.start.filename ? node->span.start.filename : "N/A",
            node->span.start.line,
            node->span.start.column,
            node->span.end.filename ? node->span.end.filename : "N/A",
            node->span.end.line,
            node->span.end.column,
            node->data);
}

int main(int argc, char *argv[])
{
    (void)argc; // Unused for now
    (void)argv; // Unused for now

    setlocale(LC_ALL, ""); // For wprintf

    wprintf(L"--- Baa AST Tester ---\n");

    // 1. Test baa_ast_new_node and baa_ast_free_node
    wprintf(L"\nTesting basic node creation and freeing:\n");
    BaaSourceLocation loc_start = {
        "test.baa",
        1,
        1,
    };
    BaaSourceLocation loc_end = {"test.baa", 1, 10};
    BaaSourceSpan span = {loc_start, loc_end};

    BaaNode *node1 = baa_ast_new_node(BAA_NODE_KIND_UNKNOWN, span);
    if (!node1)
    {
        wprintf(L"Failed to create node1!\n");
        return 1;
    }
    print_node_info(node1, "Node 1 (Unknown)");
    baa_ast_free_node(node1);
    wprintf(L"Node 1 freed.\n");

    // Test freeing a NULL node (should be safe)
    wprintf(L"\nTesting freeing a NULL node:\n");
    baa_ast_free_node(NULL);
    wprintf(L"Freeing NULL node completed (no crash is good).\n");

    // Add more tests here as we implement more node types
    // For example, once literal nodes are implemented:
    /*
    wprintf(L"\nTesting literal node creation (placeholder - requires literal AST implementation):\n");
    BaaType dummy_type_int = { .name = L"عدد_صحيح" }; // Placeholder
    BaaNode* int_literal_node = baa_ast_new_literal_int_node(span, 123, &dummy_type_int);
    if(int_literal_node) {
        print_node_info(int_literal_node, "Int Literal Node");
        // Here, we would also check int_literal_node->data if accessors existed
        BaaLiteralExprData* literal_data = BaaNodeGetLiteralData(int_literal_node); // Assuming accessor
        if (literal_data) {
            wprintf(L"  Literal Data: Kind=%d, Value=%lld\n", literal_data->literal_kind, literal_data->value.int_value);
        }
        baa_ast_free_node(int_literal_node);
        wprintf(L"Int Literal Node freed.\n");
    } else {
        wprintf(L"Failed to create int literal node (or not yet implemented).\n");
    }
    */

    wprintf(L"\n--- AST Tester Finished ---\n");
    return 0;
}
