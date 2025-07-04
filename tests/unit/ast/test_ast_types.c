#include "test_framework.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

void test_primitive_type_node_creation(void)
{
    TEST_SETUP();

    wprintf(L"Testing primitive type node creation...\n");

    // Create a source span for testing
    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 10}};

    // Test creating a primitive type node
    BaaNode *type_node = baa_ast_new_primitive_type_node(span, L"عدد_صحيح");
    ASSERT_NOT_NULL(type_node, L"Type node should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_TYPE, type_node->kind);
    ASSERT_NOT_NULL(type_node->data, L"Type node data should not be NULL");

    // Cast and verify the data
    BaaTypeAstData *type_data = (BaaTypeAstData *)type_node->data;
    ASSERT_EQ(BAA_TYPE_AST_KIND_PRIMITIVE, type_data->type_ast_kind);
    ASSERT_NOT_NULL(type_data->specifier.primitive.name, L"Type name should not be NULL");
    ASSERT_WSTR_EQ(L"عدد_صحيح", type_data->specifier.primitive.name);

    // Clean up
    baa_ast_free_node(type_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Primitive type node creation test passed\n");
}

void test_array_type_node_creation(void)
{
    TEST_SETUP();
    wprintf(L"Testing array type node creation...\n");

    // Create a source span for testing
    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 15}};

    // First create an element type node (primitive type)
    BaaNode *element_type = baa_ast_new_primitive_type_node(span, L"عدد_صحيح");
    ASSERT_NOT_NULL(element_type, L"Element type should not be NULL");

    // Create an array type node without size expression (dynamic array)
    BaaNode *array_type = baa_ast_new_array_type_node(span, element_type, NULL);
    ASSERT_NOT_NULL(array_type, L"Array type should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_TYPE, array_type->kind);
    ASSERT_NOT_NULL(array_type->data, L"Array type data should not be NULL");

    // Cast and verify the data
    BaaTypeAstData *type_data = (BaaTypeAstData *)array_type->data;
    ASSERT_EQ(BAA_TYPE_AST_KIND_ARRAY, type_data->type_ast_kind);
    ASSERT_EQ(element_type, type_data->specifier.array.element_type_node);
    ASSERT_NULL(type_data->specifier.array.size_expr, L"Size expression should be NULL for dynamic array");

    // Verify the element type is correct
    BaaTypeAstData *element_data = (BaaTypeAstData *)element_type->data;
    ASSERT_EQ(BAA_TYPE_AST_KIND_PRIMITIVE, element_data->type_ast_kind);
    ASSERT_WSTR_EQ(L"عدد_صحيح", element_data->specifier.primitive.name);

    // Clean up (this should recursively free the element type as well)
    baa_ast_free_node(array_type);

    TEST_TEARDOWN();
    wprintf(L"✓ Array type node creation test passed\n");
}

void test_invalid_type_node_creation(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid type node creation...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 10}};

    // Test creating primitive type with NULL name
    BaaNode *invalid_primitive = baa_ast_new_primitive_type_node(span, NULL);
    ASSERT_NULL(invalid_primitive, L"Primitive type with NULL name should return NULL");

    // Test creating array type with NULL element type
    BaaNode *invalid_array = baa_ast_new_array_type_node(span, NULL, NULL);
    ASSERT_NULL(invalid_array, L"Array type with NULL element type should return NULL");

    // Test creating array type with non-type element
    BaaNode *non_type_node = baa_ast_new_node(BAA_NODE_KIND_UNKNOWN, span);
    BaaNode *invalid_array2 = baa_ast_new_array_type_node(span, non_type_node, NULL);
    ASSERT_NULL(invalid_array2, L"Array type with non-type element should return NULL");

    // Clean up the non-type node
    baa_ast_free_node(non_type_node);

    TEST_TEARDOWN();
    wprintf(L"✓ Invalid type node creation test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running AST Type Representation Node tests...\n\n");

TEST_CASE(test_primitive_type_node_creation);
TEST_CASE(test_array_type_node_creation);
TEST_CASE(test_invalid_type_node_creation);

wprintf(L"\n✓ All AST Type Representation Node tests completed!\n");

TEST_SUITE_END()
