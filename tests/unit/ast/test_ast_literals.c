#include "test_framework.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"
#include "baa/types/types.h"
#include <limits.h>

void test_literal_int_node_creation(void)
{
    TEST_SETUP();
    wprintf(L"Testing literal integer node creation...\n");

    // Initialize type system for testing
    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 5}};

    // Test creating an integer literal node
    long long test_value = 42;
    BaaNode *literal_node = baa_ast_new_literal_int_node(span, test_value, baa_type_int);
    ASSERT_NOT_NULL(literal_node, L"Literal node should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, literal_node->kind);
    ASSERT_NOT_NULL(literal_node->data, L"Literal node data should not be NULL");

    // Cast and verify the data
    BaaLiteralExprData *literal_data = (BaaLiteralExprData *)literal_node->data;
    ASSERT_EQ(BAA_LITERAL_KIND_INT, literal_data->literal_kind);
    ASSERT_EQ(test_value, literal_data->value.int_value);
    ASSERT_PTR_EQ(baa_type_int, literal_data->determined_type);

    // Clean up
    baa_ast_free_node(literal_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Literal integer node creation test passed\n");
}

void test_literal_string_node_creation(void)
{
    TEST_SETUP();
    wprintf(L"Testing literal string node creation...\n");

    // Initialize type system for testing
    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Test creating a string literal node
    const wchar_t *test_string = L"مرحبا";
    BaaNode *literal_node = baa_ast_new_literal_string_node(span, test_string, baa_type_string);
    ASSERT_NOT_NULL(literal_node, L"Literal node should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, literal_node->kind);
    ASSERT_NOT_NULL(literal_node->data, L"Literal node data should not be NULL");

    // Cast and verify the data
    BaaLiteralExprData *literal_data = (BaaLiteralExprData *)literal_node->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, literal_data->literal_kind);
    ASSERT_NOT_NULL(literal_data->value.string_value, L"String value should not be NULL");
    ASSERT_WSTR_EQ(test_string, literal_data->value.string_value);
    ASSERT_PTR_EQ(baa_type_string, literal_data->determined_type);

    // Clean up
    baa_ast_free_node(literal_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Literal string node creation test passed\n");
}

void test_literal_node_invalid_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing literal node invalid operations...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 5}};

    // Test creating string literal with NULL string (should be allowed)
    BaaNode *null_string = baa_ast_new_literal_string_node(span, NULL, baa_type_string);
    ASSERT_NOT_NULL(null_string, L"String literal with NULL string should be created");

    BaaLiteralExprData *null_data = (BaaLiteralExprData *)null_string->data;
    ASSERT_NULL(null_data->value.string_value, L"String value should be NULL as specified");

    baa_ast_free_node(null_string);

    // Test creating literals with NULL type (should still work)
    BaaNode *int_with_null_type = baa_ast_new_literal_int_node(span, 42, NULL);
    ASSERT_NOT_NULL(int_with_null_type, L"Integer literal with NULL type should still work");

    BaaLiteralExprData *data = (BaaLiteralExprData *)int_with_null_type->data;
    ASSERT_NULL(data->determined_type, L"Type should be NULL as specified");

    // Clean up
    baa_ast_free_node(int_with_null_type);

    TEST_TEARDOWN();
    wprintf(L"✓ Literal node invalid operations test passed\n");
}

void test_literal_node_edge_cases(void)
{
    TEST_SETUP();
    wprintf(L"Testing literal node edge cases...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Test with extreme integer values
    BaaNode *max_int = baa_ast_new_literal_int_node(span, 9223372036854775807LL, baa_type_int);
    ASSERT_NOT_NULL(max_int, L"Max integer literal should be created");

    BaaLiteralExprData *max_data = (BaaLiteralExprData *)max_int->data;
    ASSERT_EQ(9223372036854775807LL, max_data->value.int_value);

    BaaNode *min_int = baa_ast_new_literal_int_node(span, -9223372036854775807LL - 1, baa_type_int);
    ASSERT_NOT_NULL(min_int, L"Min integer literal should be created");

    BaaLiteralExprData *min_data = (BaaLiteralExprData *)min_int->data;
    ASSERT_EQ(-9223372036854775807LL - 1, min_data->value.int_value);

    // Test with empty string
    BaaNode *empty_string = baa_ast_new_literal_string_node(span, L"", baa_type_string);
    ASSERT_NOT_NULL(empty_string, L"Empty string literal should be created");

    BaaLiteralExprData *empty_data = (BaaLiteralExprData *)empty_string->data;
    ASSERT_WSTR_EQ(L"", empty_data->value.string_value);

    // Test with very long string
    wchar_t long_string[1000];
    for (int i = 0; i < 999; i++)
    {
        long_string[i] = L'أ';
    }
    long_string[999] = L'\0';

    BaaNode *long_str_node = baa_ast_new_literal_string_node(span, long_string, baa_type_string);
    ASSERT_NOT_NULL(long_str_node, L"Long string literal should be created");

    BaaLiteralExprData *long_data = (BaaLiteralExprData *)long_str_node->data;
    ASSERT_WSTR_EQ(long_string, long_data->value.string_value);

    // Clean up
    baa_ast_free_node(max_int);
    baa_ast_free_node(min_int);
    baa_ast_free_node(empty_string);
    baa_ast_free_node(long_str_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Literal node edge cases test passed\n");
}

void test_literal_node_memory_management(void)
{
    TEST_SETUP();
    wprintf(L"Testing literal node memory management...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Create multiple literal nodes to test memory management
    const int num_literals = 100;
    BaaNode *literals[num_literals];

    for (int i = 0; i < num_literals; i++)
    {
        if (i % 2 == 0)
        {
            // Create integer literals
            literals[i] = baa_ast_new_literal_int_node(span, i, baa_type_int);
        }
        else
        {
            // Create string literals
            wchar_t str[32];
            swprintf(str, 32, L"string_%d", i);
            literals[i] = baa_ast_new_literal_string_node(span, str, baa_type_string);
        }
        ASSERT_NOT_NULL(literals[i], L"Literal should be created successfully");
    }

    // Verify all literals were created correctly
    for (int i = 0; i < num_literals; i++)
    {
        ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, literals[i]->kind);
        BaaLiteralExprData *data = (BaaLiteralExprData *)literals[i]->data;

        if (i % 2 == 0)
        {
            ASSERT_EQ(BAA_LITERAL_KIND_INT, data->literal_kind);
            ASSERT_EQ(i, data->value.int_value);
        }
        else
        {
            ASSERT_EQ(BAA_LITERAL_KIND_STRING, data->literal_kind);
            ASSERT_NOT_NULL(data->value.string_value, L"String value should not be NULL");
        }
    }

    // Clean up all literals
    for (int i = 0; i < num_literals; i++)
    {
        baa_ast_free_node(literals[i]);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Literal node memory management test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running AST Literal Expression Node tests...\n\n");

TEST_CASE(test_literal_int_node_creation);
TEST_CASE(test_literal_string_node_creation);
TEST_CASE(test_literal_node_invalid_operations);
TEST_CASE(test_literal_node_edge_cases);
TEST_CASE(test_literal_node_memory_management);

wprintf(L"\n✓ All AST Literal Expression Node tests completed!\n");

TEST_SUITE_END()
