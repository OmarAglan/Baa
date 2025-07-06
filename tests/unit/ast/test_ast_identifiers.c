#include "test_framework.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

void test_identifier_node_creation(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier node creation...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Test creating an identifier node
    const wchar_t *test_name = L"متغير_اختبار";
    BaaNode *identifier_node = baa_ast_new_identifier_expr_node(span, test_name);
    ASSERT_NOT_NULL(identifier_node, L"Identifier node should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, identifier_node->kind);
    ASSERT_NOT_NULL(identifier_node->data, L"Identifier node data should not be NULL");

    // Cast and verify the data
    BaaIdentifierExprData *identifier_data = (BaaIdentifierExprData *)identifier_node->data;
    ASSERT_NOT_NULL(identifier_data->name, L"Identifier name should not be NULL");
    ASSERT_WSTR_EQ(test_name, identifier_data->name);

    // Clean up
    baa_ast_free_node(identifier_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Identifier node creation test passed\n");
}

void test_identifier_node_arabic_names(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier node with Arabic names...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 15}};

    // Test various Arabic identifier names
    const wchar_t *arabic_names[] = {
        L"العدد",
        L"النص",
        L"المتغير_الأول",
        L"دالة_الحساب",
        L"قيمة_صحيحة",
        L"نتيجة_العملية"};

    size_t num_names = sizeof(arabic_names) / sizeof(arabic_names[0]);

    for (size_t i = 0; i < num_names; i++)
    {
        BaaNode *identifier_node = baa_ast_new_identifier_expr_node(span, arabic_names[i]);
        ASSERT_NOT_NULL(identifier_node, L"Arabic identifier node should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, identifier_node->kind);

        BaaIdentifierExprData *data = (BaaIdentifierExprData *)identifier_node->data;
        ASSERT_WSTR_EQ(arabic_names[i], data->name);

        baa_ast_free_node(identifier_node);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Identifier node Arabic names test passed\n");
}

void test_identifier_node_mixed_names(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier node with mixed character names...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 20}};

    // Test mixed Arabic and Latin identifiers (if supported)
    const wchar_t *mixed_names[] = {
        L"متغير_1",
        L"value_عربي",
        L"test_اختبار_123",
        L"العدد_الأول_v1",
        L"result_نتيجة"};

    size_t num_names = sizeof(mixed_names) / sizeof(mixed_names[0]);

    for (size_t i = 0; i < num_names; i++)
    {
        BaaNode *identifier_node = baa_ast_new_identifier_expr_node(span, mixed_names[i]);
        ASSERT_NOT_NULL(identifier_node, L"Mixed identifier node should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, identifier_node->kind);

        BaaIdentifierExprData *data = (BaaIdentifierExprData *)identifier_node->data;
        ASSERT_WSTR_EQ(mixed_names[i], data->name);

        baa_ast_free_node(identifier_node);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Identifier node mixed names test passed\n");
}

void test_identifier_node_invalid_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier node invalid operations...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Test creating identifier with NULL name (should be allowed)
    BaaNode *null_identifier = baa_ast_new_identifier_expr_node(span, NULL);
    ASSERT_NOT_NULL(null_identifier, L"Identifier with NULL name should be created");

    BaaIdentifierExprData *null_data = (BaaIdentifierExprData *)null_identifier->data;
    ASSERT_NULL(null_data->name, L"Name should be NULL as specified");

    baa_ast_free_node(null_identifier);

    // Test creating identifier with empty name
    BaaNode *empty_identifier = baa_ast_new_identifier_expr_node(span, L"");
    ASSERT_NOT_NULL(empty_identifier, L"Identifier with empty name should be created");

    BaaIdentifierExprData *data = (BaaIdentifierExprData *)empty_identifier->data;
    ASSERT_WSTR_EQ(L"", data->name);

    // Clean up
    baa_ast_free_node(empty_identifier);

    TEST_TEARDOWN();
    wprintf(L"✓ Identifier node invalid operations test passed\n");
}

void test_identifier_node_edge_cases(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier node edge cases...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 100}};

    // Test with very long identifier name
    wchar_t long_name[1000];
    for (int i = 0; i < 999; i++)
    {
        long_name[i] = (i % 2 == 0) ? L'م' : L'ت';
    }
    long_name[999] = L'\0';

    BaaNode *long_identifier = baa_ast_new_identifier_expr_node(span, long_name);
    ASSERT_NOT_NULL(long_identifier, L"Long identifier should be created");

    BaaIdentifierExprData *long_data = (BaaIdentifierExprData *)long_identifier->data;
    ASSERT_WSTR_EQ(long_name, long_data->name);

    // Test with single character identifier
    BaaNode *single_char = baa_ast_new_identifier_expr_node(span, L"أ");
    ASSERT_NOT_NULL(single_char, L"Single character identifier should be created");

    BaaIdentifierExprData *single_data = (BaaIdentifierExprData *)single_char->data;
    ASSERT_WSTR_EQ(L"أ", single_data->name);

    // Test with identifier containing special Arabic characters
    BaaNode *special_chars = baa_ast_new_identifier_expr_node(span, L"متغير_بـالتشكيل");
    ASSERT_NOT_NULL(special_chars, L"Identifier with special characters should be created");

    BaaIdentifierExprData *special_data = (BaaIdentifierExprData *)special_chars->data;
    ASSERT_WSTR_EQ(L"متغير_بـالتشكيل", special_data->name);

    // Clean up
    baa_ast_free_node(long_identifier);
    baa_ast_free_node(single_char);
    baa_ast_free_node(special_chars);

    TEST_TEARDOWN();
    wprintf(L"✓ Identifier node edge cases test passed\n");
}

void test_identifier_node_memory_management(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier node memory management...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 15}};

    // Create multiple identifier nodes to test memory management
    const int num_identifiers = 50;
    BaaNode *identifiers[num_identifiers];

    for (int i = 0; i < num_identifiers; i++)
    {
        wchar_t name[32];
        swprintf(name, 32, L"متغير_%d", i);
        identifiers[i] = baa_ast_new_identifier_expr_node(span, name);
        ASSERT_NOT_NULL(identifiers[i], L"Identifier should be created successfully");
    }

    // Verify all identifiers were created correctly
    for (int i = 0; i < num_identifiers; i++)
    {
        ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, identifiers[i]->kind);
        BaaIdentifierExprData *data = (BaaIdentifierExprData *)identifiers[i]->data;
        ASSERT_NOT_NULL(data->name, L"Identifier name should not be NULL");

        wchar_t expected_name[32];
        swprintf(expected_name, 32, L"متغير_%d", i);
        ASSERT_WSTR_EQ(expected_name, data->name);
    }

    // Clean up all identifiers
    for (int i = 0; i < num_identifiers; i++)
    {
        baa_ast_free_node(identifiers[i]);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Identifier node memory management test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running AST Identifier Expression Node tests...\n\n");

TEST_CASE(test_identifier_node_creation);
TEST_CASE(test_identifier_node_arabic_names);
TEST_CASE(test_identifier_node_mixed_names);
TEST_CASE(test_identifier_node_invalid_operations);
TEST_CASE(test_identifier_node_edge_cases);
TEST_CASE(test_identifier_node_memory_management);

wprintf(L"\n✓ All AST Identifier Expression Node tests completed!\n");

TEST_SUITE_END()
