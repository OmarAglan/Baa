#include "test_framework.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"
#include "baa/types/types.h"

void test_binary_expr_arithmetic_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing binary expression arithmetic operations...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Create operand nodes
    BaaNode *left = baa_ast_new_literal_int_node(span, 10, baa_type_int);
    BaaNode *right = baa_ast_new_literal_int_node(span, 5, baa_type_int);
    ASSERT_NOT_NULL(left, L"Left operand should not be NULL");
    ASSERT_NOT_NULL(right, L"Right operand should not be NULL");

    // Test arithmetic operators
    BaaBinaryOperatorKind arithmetic_ops[] = {
        BAA_BINARY_OP_ADD,
        BAA_BINARY_OP_SUBTRACT,
        BAA_BINARY_OP_MULTIPLY,
        BAA_BINARY_OP_DIVIDE,
        BAA_BINARY_OP_MODULO};

    const wchar_t *op_names[] = {
        L"addition",
        L"subtraction",
        L"multiplication",
        L"division",
        L"modulo"};

    size_t num_ops = sizeof(arithmetic_ops) / sizeof(arithmetic_ops[0]);

    for (size_t i = 0; i < num_ops; i++)
    {
        // Create new operands for each operation (since they'll be owned by the binary expr)
        BaaNode *left_copy = baa_ast_new_literal_int_node(span, 10, baa_type_int);
        BaaNode *right_copy = baa_ast_new_literal_int_node(span, 5, baa_type_int);

        BaaNode *binary_expr = baa_ast_new_binary_expr_node(span, left_copy, right_copy, arithmetic_ops[i]);
        ASSERT_NOT_NULL(binary_expr, L"Binary expression should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_BINARY_EXPR, binary_expr->kind);

        BaaBinaryExprData *data = (BaaBinaryExprData *)binary_expr->data;
        ASSERT_PTR_EQ(left_copy, data->left_operand);
        ASSERT_PTR_EQ(right_copy, data->right_operand);
        ASSERT_EQ(arithmetic_ops[i], data->operator_kind);

        wprintf(L"  ✓ %ls operation test passed\n", op_names[i]);

        // Clean up (will recursively free operands)
        baa_ast_free_node(binary_expr);
    }

    // Clean up the original operands (not used in expressions)
    baa_ast_free_node(left);
    baa_ast_free_node(right);

    TEST_TEARDOWN();
    wprintf(L"✓ Binary expression arithmetic operations test passed\n");
}

void test_binary_expr_comparison_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing binary expression comparison operations...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Test comparison operators
    BaaBinaryOperatorKind comparison_ops[] = {
        BAA_BINARY_OP_EQUAL,
        BAA_BINARY_OP_NOT_EQUAL,
        BAA_BINARY_OP_LESS_THAN,
        BAA_BINARY_OP_LESS_EQUAL,
        BAA_BINARY_OP_GREATER_THAN,
        BAA_BINARY_OP_GREATER_EQUAL};

    const wchar_t *op_names[] = {
        L"equal",
        L"not equal",
        L"less than",
        L"less or equal",
        L"greater than",
        L"greater or equal"};

    size_t num_ops = sizeof(comparison_ops) / sizeof(comparison_ops[0]);

    for (size_t i = 0; i < num_ops; i++)
    {
        BaaNode *left = baa_ast_new_literal_int_node(span, 10, baa_type_int);
        BaaNode *right = baa_ast_new_literal_int_node(span, 5, baa_type_int);

        BaaNode *binary_expr = baa_ast_new_binary_expr_node(span, left, right, comparison_ops[i]);
        ASSERT_NOT_NULL(binary_expr, L"Comparison expression should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_BINARY_EXPR, binary_expr->kind);

        BaaBinaryExprData *data = (BaaBinaryExprData *)binary_expr->data;
        ASSERT_EQ(comparison_ops[i], data->operator_kind);

        wprintf(L"  ✓ %ls operation test passed\n", op_names[i]);

        baa_ast_free_node(binary_expr);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Binary expression comparison operations test passed\n");
}

void test_binary_expr_logical_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing binary expression logical operations...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    // Test logical operators
    BaaBinaryOperatorKind logical_ops[] = {
        BAA_BINARY_OP_LOGICAL_AND,
        BAA_BINARY_OP_LOGICAL_OR};

    const wchar_t *op_names[] = {
        L"logical AND",
        L"logical OR"};

    size_t num_ops = sizeof(logical_ops) / sizeof(logical_ops[0]);

    for (size_t i = 0; i < num_ops; i++)
    {
        // Use identifier expressions as boolean-like operands
        BaaNode *left = baa_ast_new_identifier_expr_node(span, L"condition1");
        BaaNode *right = baa_ast_new_identifier_expr_node(span, L"condition2");

        BaaNode *binary_expr = baa_ast_new_binary_expr_node(span, left, right, logical_ops[i]);
        ASSERT_NOT_NULL(binary_expr, L"Logical expression should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_BINARY_EXPR, binary_expr->kind);

        BaaBinaryExprData *data = (BaaBinaryExprData *)binary_expr->data;
        ASSERT_EQ(logical_ops[i], data->operator_kind);

        wprintf(L"  ✓ %ls operation test passed\n", op_names[i]);

        baa_ast_free_node(binary_expr);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Binary expression logical operations test passed\n");
}

void test_binary_expr_nested_expressions(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested binary expressions...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 20}};

    // Create nested expression: (10 + 5) * (20 - 15)
    BaaNode *left_left = baa_ast_new_literal_int_node(span, 10, baa_type_int);
    BaaNode *left_right = baa_ast_new_literal_int_node(span, 5, baa_type_int);
    BaaNode *left_expr = baa_ast_new_binary_expr_node(span, left_left, left_right, BAA_BINARY_OP_ADD);

    BaaNode *right_left = baa_ast_new_literal_int_node(span, 20, baa_type_int);
    BaaNode *right_right = baa_ast_new_literal_int_node(span, 15, baa_type_int);
    BaaNode *right_expr = baa_ast_new_binary_expr_node(span, right_left, right_right, BAA_BINARY_OP_SUBTRACT);

    BaaNode *root_expr = baa_ast_new_binary_expr_node(span, left_expr, right_expr, BAA_BINARY_OP_MULTIPLY);

    ASSERT_NOT_NULL(root_expr, L"Root expression should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_BINARY_EXPR, root_expr->kind);

    // Verify the structure
    BaaBinaryExprData *root_data = (BaaBinaryExprData *)root_expr->data;
    ASSERT_EQ(BAA_BINARY_OP_MULTIPLY, root_data->operator_kind);
    ASSERT_PTR_EQ(left_expr, root_data->left_operand);
    ASSERT_PTR_EQ(right_expr, root_data->right_operand);

    // Verify left sub-expression
    BaaBinaryExprData *left_data = (BaaBinaryExprData *)left_expr->data;
    ASSERT_EQ(BAA_BINARY_OP_ADD, left_data->operator_kind);

    // Verify right sub-expression
    BaaBinaryExprData *right_data = (BaaBinaryExprData *)right_expr->data;
    ASSERT_EQ(BAA_BINARY_OP_SUBTRACT, right_data->operator_kind);

    // Clean up (should recursively free all sub-expressions)
    baa_ast_free_node(root_expr);

    TEST_TEARDOWN();
    wprintf(L"✓ Nested binary expressions test passed\n");
}

void test_binary_expr_invalid_operations(void)
{
    TEST_SETUP();
    wprintf(L"Testing binary expression invalid operations...\n");

    baa_init_type_system();

    BaaAstSourceSpan span = {
        .start = {.filename = L"test.baa", .line = 1, .column = 1},
        .end = {.filename = L"test.baa", .line = 1, .column = 10}};

    BaaNode *valid_operand = baa_ast_new_literal_int_node(span, 42, baa_type_int);

    // Test with NULL left operand
    BaaNode *invalid_left = baa_ast_new_binary_expr_node(span, NULL, valid_operand, BAA_BINARY_OP_ADD);
    ASSERT_NULL(invalid_left, L"Binary expression with NULL left operand should return NULL");

    // Test with NULL right operand
    BaaNode *invalid_right = baa_ast_new_binary_expr_node(span, valid_operand, NULL, BAA_BINARY_OP_ADD);
    ASSERT_NULL(invalid_right, L"Binary expression with NULL right operand should return NULL");

    // Test with both NULL operands
    BaaNode *invalid_both = baa_ast_new_binary_expr_node(span, NULL, NULL, BAA_BINARY_OP_ADD);
    ASSERT_NULL(invalid_both, L"Binary expression with both NULL operands should return NULL");

    // Clean up
    baa_ast_free_node(valid_operand);

    TEST_TEARDOWN();
    wprintf(L"✓ Binary expression invalid operations test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running AST Binary Expression Node tests...\n\n");

TEST_CASE(test_binary_expr_arithmetic_operations);
TEST_CASE(test_binary_expr_comparison_operations);
TEST_CASE(test_binary_expr_logical_operations);
TEST_CASE(test_binary_expr_nested_expressions);
TEST_CASE(test_binary_expr_invalid_operations);

wprintf(L"\n✓ All AST Binary Expression Node tests completed!\n");

TEST_SUITE_END()
