#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

void test_primitive_type_node_creation(void)
{
    printf("Testing primitive type node creation...\n");

    // Create a source span for testing
    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 10}};

    // Test creating a primitive type node
    BaaNode *type_node = baa_ast_new_primitive_type_node(span, L"عدد_صحيح");
    assert(type_node != NULL);
    assert(type_node->kind == BAA_NODE_KIND_TYPE);
    assert(type_node->data != NULL);

    // Cast and verify the data
    BaaTypeAstData *type_data = (BaaTypeAstData *)type_node->data;
    assert(type_data->type_ast_kind == BAA_TYPE_AST_KIND_PRIMITIVE);
    assert(type_data->specifier.primitive.name != NULL);
    assert(wcscmp(type_data->specifier.primitive.name, L"عدد_صحيح") == 0);

    // Clean up
    baa_ast_free_node(type_node);

    printf("✓ Primitive type node creation test passed\n");
}

void test_array_type_node_creation(void)
{
    printf("Testing array type node creation...\n");

    // Create a source span for testing
    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 15}};

    // First create an element type node (primitive type)
    BaaNode *element_type = baa_ast_new_primitive_type_node(span, L"عدد_صحيح");
    assert(element_type != NULL);

    // Create an array type node without size expression (dynamic array)
    BaaNode *array_type = baa_ast_new_array_type_node(span, element_type, NULL);
    assert(array_type != NULL);
    assert(array_type->kind == BAA_NODE_KIND_TYPE);
    assert(array_type->data != NULL);

    // Cast and verify the data
    BaaTypeAstData *type_data = (BaaTypeAstData *)array_type->data;
    assert(type_data->type_ast_kind == BAA_TYPE_AST_KIND_ARRAY);
    assert(type_data->specifier.array.element_type_node == element_type);
    assert(type_data->specifier.array.size_expr == NULL);

    // Verify the element type is correct
    BaaTypeAstData *element_data = (BaaTypeAstData *)element_type->data;
    assert(element_data->type_ast_kind == BAA_TYPE_AST_KIND_PRIMITIVE);
    assert(wcscmp(element_data->specifier.primitive.name, L"عدد_صحيح") == 0);

    // Clean up (this should recursively free the element type as well)
    baa_ast_free_node(array_type);

    printf("✓ Array type node creation test passed\n");
}

void test_invalid_type_node_creation(void)
{
    printf("Testing invalid type node creation...\n");

    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 10}};

    // Test creating primitive type with NULL name
    BaaNode *invalid_primitive = baa_ast_new_primitive_type_node(span, NULL);
    assert(invalid_primitive == NULL);

    // Test creating array type with NULL element type
    BaaNode *invalid_array = baa_ast_new_array_type_node(span, NULL, NULL);
    assert(invalid_array == NULL);

    // Test creating array type with non-type element
    BaaNode *non_type_node = baa_ast_new_node(BAA_NODE_KIND_UNKNOWN, span);
    BaaNode *invalid_array2 = baa_ast_new_array_type_node(span, non_type_node, NULL);
    assert(invalid_array2 == NULL);

    // Clean up the non-type node
    baa_ast_free_node(non_type_node);

    printf("✓ Invalid type node creation test passed\n");
}

int main(void)
{
    printf("Running AST Type Representation Node tests...\n\n");

    test_primitive_type_node_creation();
    test_array_type_node_creation();
    test_invalid_type_node_creation();

    printf("\n✓ All AST Type Representation Node tests passed!\n");
    return 0;
}
