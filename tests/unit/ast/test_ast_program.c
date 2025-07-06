#include "test_framework.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

void test_program_node_creation(void)
{
    TEST_SETUP();
    wprintf(L"Testing program node creation...\n");

    // Create a source span for testing
    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 10, .column = 1}};

    // Test creating a program node
    BaaNode *program_node = baa_ast_new_program_node(span);
    ASSERT_NOT_NULL(program_node, L"Program node should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, program_node->kind);
    ASSERT_NOT_NULL(program_node->data, L"Program node data should not be NULL");

    // Cast and verify the data
    BaaProgramData *program_data = (BaaProgramData *)program_node->data;
    ASSERT_EQ(0, program_data->count);
    ASSERT_EQ(0, program_data->capacity);
    ASSERT_NULL(program_data->top_level_declarations, L"Initial declarations array should be NULL");

    // Clean up
    baa_ast_free_node(program_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Program node creation test passed\n");
}

void test_program_node_add_declaration(void)
{
    TEST_SETUP();
    wprintf(L"Testing program node declaration addition...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 10, .column = 1}};

    // Create program node
    BaaNode *program_node = baa_ast_new_program_node(span);
    ASSERT_NOT_NULL(program_node, L"Program node should not be NULL");

    // Create a test declaration (using identifier expression as a simple declaration)
    BaaNode *declaration = baa_ast_new_identifier_expr_node(span, L"test_declaration");
    ASSERT_NOT_NULL(declaration, L"Declaration should not be NULL");

    // Add declaration to program
    bool result = baa_ast_add_declaration_to_program(program_node, declaration);
    ASSERT_TRUE(result, L"Adding declaration should succeed");

    // Verify the declaration was added
    BaaProgramData *program_data = (BaaProgramData *)program_node->data;
    ASSERT_EQ(1, program_data->count);
    ASSERT_NOT_NULL(program_data->top_level_declarations, L"Declarations array should not be NULL");
    ASSERT_PTR_EQ(declaration, program_data->top_level_declarations[0]);

    // Add another declaration
    BaaNode *declaration2 = baa_ast_new_identifier_expr_node(span, L"test_declaration2");
    result = baa_ast_add_declaration_to_program(program_node, declaration2);
    ASSERT_TRUE(result, L"Adding second declaration should succeed");
    ASSERT_EQ(2, program_data->count);
    ASSERT_PTR_EQ(declaration2, program_data->top_level_declarations[1]);

    // Clean up (should recursively free all declarations)
    baa_ast_free_node(program_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Program node declaration addition test passed\n");
}

void test_program_node_invalid_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing program node invalid operations...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 10, .column = 1}};

    // Test adding declaration to NULL program
    BaaNode *declaration = baa_ast_new_identifier_expr_node(span, L"test_declaration");
    bool result = baa_ast_add_declaration_to_program(NULL, declaration);
    ASSERT_TRUE(!result, L"Adding declaration to NULL program should fail");

    // Test adding NULL declaration to program
    BaaNode *program_node = baa_ast_new_program_node(span);
    result = baa_ast_add_declaration_to_program(program_node, NULL);
    ASSERT_TRUE(!result, L"Adding NULL declaration should fail");

    // Clean up
    baa_ast_free_node(declaration);
    baa_ast_free_node(program_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Program node invalid operations test passed\n");
}

void test_program_node_memory_management(void)
{
    TEST_SETUP();
    wprintf(L"Testing program node memory management...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 10, .column = 1}};

    // Create program with multiple declarations
    BaaNode *program_node = baa_ast_new_program_node(span);

    // Add multiple declarations to test dynamic array growth
    for (int i = 0; i < 10; i++)
    {
        wchar_t name[32];
        swprintf(name, 32, L"declaration_%d", i);
        BaaNode *declaration = baa_ast_new_identifier_expr_node(span, name);
        bool result = baa_ast_add_declaration_to_program(program_node, declaration);
        ASSERT_TRUE(result, L"Adding declaration should succeed");
    }

    // Verify all declarations were added
    BaaProgramData *program_data = (BaaProgramData *)program_node->data;
    ASSERT_EQ(10, program_data->count);
    ASSERT_NOT_NULL(program_data->top_level_declarations, L"Declarations array should not be NULL");

    // Verify capacity grew appropriately
    ASSERT_TRUE(program_data->capacity >= 10, L"Capacity should be at least 10");

    // Clean up (should free all declarations and the dynamic array)
    baa_ast_free_node(program_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Program node memory management test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running AST Program Node tests...\n\n");

TEST_CASE(test_program_node_creation);
TEST_CASE(test_program_node_add_declaration);
TEST_CASE(test_program_node_invalid_operations);
TEST_CASE(test_program_node_memory_management);

wprintf(L"\n✓ All AST Program Node tests completed!\n");

TEST_SUITE_END()
