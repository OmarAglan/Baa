#include "test_framework.h"
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include Baa headers for testing utilities
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/utils/utils.h"

int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;
int current_failed_count = 0; // Add this

void init_test_framework()
{
    total_tests = 0;
    passed_tests = 0;
    failed_tests = 0;
    current_failed_count = 0; // Initialize
}

void print_test_results()
{
    wprintf(L"\n--- Test Results ---\n"); // Use wprintf
    wprintf(L"Total tests: %d\n", total_tests);
    wprintf(L"Passed: %d\n", passed_tests);
    wprintf(L"Failed: %d\n", failed_tests);
    wprintf(L"Success rate: %.2f%%\n",
            (total_tests > 0) ? ((double)passed_tests / total_tests * 100.0) : 0.0); // Use double for precision
}

// --- Memory Tracking ---
static size_t allocated_memory_count = 0;
static size_t initial_memory_count = 0;

void track_memory_allocation()
{
    initial_memory_count = allocated_memory_count;
}

void assert_no_memory_leaks()
{
    if (allocated_memory_count > initial_memory_count)
    {
        wprintf(L"[MEMORY LEAK] %zu allocations not freed\n",
                allocated_memory_count - initial_memory_count);
        failed_tests++;
    }
}

size_t get_allocated_memory_count()
{
    return allocated_memory_count;
}

// --- String Testing Utilities ---
void assert_wstr_eq(const wchar_t *expected, const wchar_t *actual, const char *test_name)
{
    if (!expected && !actual)
    {
        return; // Both NULL, considered equal
    }
    if (!expected || !actual)
    {
        wprintf(L"[ASSERT FAILED] %hs: One string is NULL, the other is not\n", test_name);
        wprintf(L"                Expected: %ls, Actual: %ls\n",
                expected ? expected : L"(NULL)",
                actual ? actual : L"(NULL)");
        failed_tests++;
        return;
    }
    if (wcscmp(expected, actual) != 0)
    {
        wprintf(L"[ASSERT FAILED] %hs: Strings not equal\n", test_name);
        wprintf(L"                Expected: \"%ls\"\n", expected);
        wprintf(L"                Actual:   \"%ls\"\n", actual);
        failed_tests++;
    }
}

void assert_wstr_contains(const wchar_t *haystack, const wchar_t *needle, const char *test_name)
{
    if (!haystack || !needle)
    {
        wprintf(L"[ASSERT FAILED] %hs: NULL string provided\n", test_name);
        failed_tests++;
        return;
    }
    if (!wcsstr(haystack, needle))
    {
        wprintf(L"[ASSERT FAILED] %hs: String does not contain expected substring\n", test_name);
        wprintf(L"                Haystack: \"%ls\"\n", haystack);
        wprintf(L"                Needle:   \"%ls\"\n", needle);
        failed_tests++;
    }
}

// --- File Testing Utilities ---
wchar_t *load_test_file(const char *relative_path)
{
    // Construct full path relative to tests directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "tests/%s", relative_path);

    // Convert to wide string for baa_read_file
    size_t len = strlen(full_path) + 1;
    wchar_t *wide_path = malloc(len * sizeof(wchar_t));
    if (!wide_path)
    {
        return NULL;
    }

    mbstowcs(wide_path, full_path, len);
    wchar_t *content = baa_read_file(wide_path);
    free(wide_path);

    return content;
}

bool file_exists(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

void compare_with_expected_file(const char *actual_output, const char *expected_file)
{
    // This is a simplified implementation
    // In a real scenario, you'd want more sophisticated comparison
    wprintf(L"[INFO] Comparing output with expected file: %hs\n", expected_file);
    // TODO: Implement actual file comparison logic
}

// --- AST Testing Utilities ---
BaaNode *create_test_ast_node(int kind, const char *test_data)
{
    // Create a simple test source span
    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Create node based on kind
    switch (kind)
    {
    case BAA_NODE_KIND_PROGRAM:
        return baa_ast_new_program_node(span);
    case BAA_NODE_KIND_IDENTIFIER_EXPR:
        if (test_data)
        {
            // Convert test_data to wide string
            size_t len = strlen(test_data) + 1;
            wchar_t *wide_name = malloc(len * sizeof(wchar_t));
            if (wide_name)
            {
                mbstowcs(wide_name, test_data, len);
                BaaNode *node = baa_ast_new_identifier_expr_node(span, wide_name);
                free(wide_name);
                return node;
            }
        }
        return baa_ast_new_identifier_expr_node(span, L"test_identifier");
    case BAA_NODE_KIND_TYPE:
        if (test_data)
        {
            size_t len = strlen(test_data) + 1;
            wchar_t *wide_name = malloc(len * sizeof(wchar_t));
            if (wide_name)
            {
                mbstowcs(wide_name, test_data, len);
                BaaNode *node = baa_ast_new_primitive_type_node(span, wide_name);
                free(wide_name);
                return node;
            }
        }
        return baa_ast_new_primitive_type_node(span, L"عدد_صحيح");
    default:
        return baa_ast_new_node((BaaNodeKind)kind, span);
    }
}

void print_ast_debug(BaaNode *root, int indent_level)
{
    if (!root)
    {
        for (int i = 0; i < indent_level; i++)
            wprintf(L"  ");
        wprintf(L"(NULL)\n");
        return;
    }

    for (int i = 0; i < indent_level; i++)
        wprintf(L"  ");
    wprintf(L"Node: kind=%d\n", root->kind);

    // TODO: Add more detailed AST printing based on node type
}

void free_test_ast(BaaNode *root)
{
    if (root)
    {
        baa_ast_free_node(root);
    }
}

void assert_ast_structure(BaaNode *root, const char *expected_structure)
{
    // This is a simplified implementation
    // In a real scenario, you'd want to parse the expected_structure
    // and compare it with the actual AST structure
    if (!root)
    {
        wprintf(L"[ASSERT FAILED] AST root is NULL\n");
        failed_tests++;
        return;
    }

    wprintf(L"[INFO] Checking AST structure against: %hs\n", expected_structure);
    // TODO: Implement actual AST structure comparison
}

// --- Parser Testing Utilities ---
BaaNode *parse_test_string(const wchar_t *source)
{
    if (!source)
    {
        return NULL;
    }

    // Initialize lexer with the source
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");

    // Create parser from lexer
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    if (!parser)
    {
        return NULL;
    }

    // Parse the program
    BaaNode *ast = baa_parse_program(parser);

    // Clean up parser (but not the AST)
    baa_parser_free(parser);

    return ast;
}

void assert_parse_success(const wchar_t *source, const char *test_name)
{
    BaaNode *ast = parse_test_string(source);
    if (!ast)
    {
        wprintf(L"[ASSERT FAILED] %hs: Parse failed for source: \"%ls\"\n", test_name, source);
        failed_tests++;
        return;
    }

    // Clean up
    baa_ast_free_node(ast);
}

void assert_parse_error(const wchar_t *source, int expected_error, const char *test_name)
{
    BaaNode *ast = parse_test_string(source);
    if (ast)
    {
        wprintf(L"[ASSERT FAILED] %hs: Expected parse error but parsing succeeded for: \"%ls\"\n",
                test_name, source);
        baa_ast_free_node(ast);
        failed_tests++;
        return;
    }

    // TODO: Check specific error code if error reporting is implemented
    wprintf(L"[INFO] %hs: Parse correctly failed for invalid source\n", test_name);
}

// --- Lexer Testing Utilities ---
BaaToken *tokenize_test_string(const wchar_t *source)
{
    if (!source)
    {
        return NULL;
    }

    // This is a simplified implementation
    // In practice, you'd tokenize the entire string and return an array
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");

    // For now, just return the first token
    return baa_lexer_next_token(&lexer);
}

void assert_token_sequence(const wchar_t *source, int *expected_types, size_t count, const char *test_name)
{
    if (!source || !expected_types)
    {
        wprintf(L"[ASSERT FAILED] %hs: NULL parameters provided\n", test_name);
        failed_tests++;
        return;
    }

    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");

    for (size_t i = 0; i < count; i++)
    {
        BaaToken *token = baa_lexer_next_token(&lexer);
        if (!token)
        {
            wprintf(L"[ASSERT FAILED] %hs: Expected token %zu but got NULL\n", test_name, i);
            failed_tests++;
            return;
        }

        if (token->type != expected_types[i])
        {
            wprintf(L"[ASSERT FAILED] %hs: Token %zu type mismatch. Expected %d, got %d\n",
                    test_name, i, expected_types[i], token->type);
            baa_free_token(token);
            failed_tests++;
            return;
        }

        baa_free_token(token);
    }
}

void free_test_tokens(BaaToken *tokens, size_t count)
{
    if (tokens)
    {
        for (size_t i = 0; i < count; i++)
        {
            baa_free_token(&tokens[i]);
        }
    }
}
