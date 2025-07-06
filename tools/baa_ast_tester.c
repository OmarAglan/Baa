#include "baa/ast/ast.h"
#include "baa/utils/utils.h"
#include "baa/types/types.h" // For BaaType and baa_get_int_type(), etc.
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <string.h> // For wcscmp

// Helper to print a node's basic info
void print_node_info(const BaaNode *node, const char *description)
{
    // ... (same as before) ...
    if (!node)
    {
        wprintf(L"%hs: Node is NULL\n", description);
        return;
    }
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

// Type-safe accessor macro example (would ideally be in an ast_utils.h)
// For testing, we can define it locally or wait until we formalize ast_utils.h
#define BaaNodeGetLiteralData(node) \
    (((node) && (node)->kind == BAA_NODE_KIND_LITERAL_EXPR) ? (BaaLiteralExprData *)((node)->data) : NULL)

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    setlocale(LC_ALL, "");
    baa_init_type_system(); // Initialize global types like baa_type_int

    wprintf(L"--- Baa AST Tester ---\n");

    BaaSourceLocation loc_start = {L"test.baa", 1, 1};
    BaaSourceLocation loc_end = {L"test.baa", 1, 10};
    BaaAstSourceSpan span = {loc_start, loc_end};

    // 1. Test basic node (unchanged)
    wprintf(L"\nTesting basic node creation and freeing:\n");
    BaaNode *node1 = baa_ast_new_node(BAA_NODE_KIND_UNKNOWN, span);
    print_node_info(node1, "Node 1 (Unknown)");
    baa_ast_free_node(node1);
    wprintf(L"Node 1 freed.\n");

    wprintf(L"\nTesting freeing a NULL node:\n");
    baa_ast_free_node(NULL);
    wprintf(L"Freeing NULL node completed.\n");

    // 2. Test Integer Literal Node
    wprintf(L"\nTesting Integer Literal Node:\n");
    BaaNode *int_literal_node = baa_ast_new_literal_int_node(span, 12345LL, baa_get_int_type());
    if (!int_literal_node)
    {
        wprintf(L"Failed to create integer literal node!\n");
        return 1;
    }
    print_node_info(int_literal_node, "Int Literal Node");
    BaaLiteralExprData *int_data = BaaNodeGetLiteralData(int_literal_node); // Using our example accessor
    if (int_data)
    {
        wprintf(L"  Literal Kind: %d (Expected BAA_LITERAL_KIND_INT=%d)\n", int_data->literal_kind, BAA_LITERAL_KIND_INT);
        wprintf(L"  Int Value: %lld (Expected 12345)\n", int_data->value.int_value);
        if (int_data->determined_type)
        {
            wprintf(L"  Determined Type: %ls (Expected %ls for int)\n",
                    int_data->determined_type->name, baa_get_int_type()->name);
        }
        else
        {
            wprintf(L"  Determined Type: NULL (Error!)\n");
        }
    }
    else
    {
        wprintf(L"  Failed to get literal data from int_literal_node!\n");
    }
    baa_ast_free_node(int_literal_node);
    wprintf(L"Int Literal Node freed.\n");

    // 3. Test String Literal Node
    wprintf(L"\nTesting String Literal Node:\n");
    const wchar_t *test_string = L"مرحباً بالعالم";
    BaaNode *string_literal_node = baa_ast_new_literal_string_node(span, test_string, baa_get_string_type()); // Assuming baa_type_string exists
    if (!string_literal_node)
    {
        wprintf(L"Failed to create string literal node!\n");
        return 1;
    }
    print_node_info(string_literal_node, "String Literal Node");
    BaaLiteralExprData *str_data = BaaNodeGetLiteralData(string_literal_node);
    if (str_data)
    {
        wprintf(L"  Literal Kind: %d (Expected BAA_LITERAL_KIND_STRING=%d)\n", str_data->literal_kind, BAA_LITERAL_KIND_STRING);
        if (str_data->value.string_value)
        {
            wprintf(L"  String Value: \"%ls\" (Expected \"%ls\")\n", str_data->value.string_value, test_string);
            // Test if it's a copy
            if (str_data->value.string_value == test_string)
            {
                wprintf(L"  ERROR: String value is not a copy!\n");
            }
            else if (wcscmp(str_data->value.string_value, test_string) == 0)
            {
                wprintf(L"  String value is a correct copy.\n");
            }
            else
            {
                wprintf(L"  ERROR: String value copy is incorrect!\n");
            }
        }
        else
        {
            wprintf(L"  String Value: NULL (Error!)\n");
        }
        if (str_data->determined_type)
        {
            wprintf(L"  Determined Type: %ls (Expected %ls for string)\n",
                    str_data->determined_type->name, baa_get_string_type()->name);
        }
        else
        {
            wprintf(L"  Determined Type: NULL (Error!)\n");
        }
    }
    else
    {
        wprintf(L"  Failed to get literal data from string_literal_node!\n");
    }
    baa_ast_free_node(string_literal_node);
    wprintf(L"String Literal Node freed.\n");

    wprintf(L"\n--- AST Tester Finished ---\n");
    return 0;
}
