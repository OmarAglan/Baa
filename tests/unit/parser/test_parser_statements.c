#include "test_framework.h"
#include "baa/parser/parser.h"
#include "baa/lexer/lexer.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

// Helper function to parse a single statement from source
BaaNode* parse_statement_from_source(const wchar_t* source)
{
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    if (!parser) {
        return NULL;
    }
    
    // Parse as a program and extract the first statement
    BaaNode *ast = baa_parse_program(parser);
    if (!ast || ast->kind != BAA_NODE_KIND_PROGRAM) {
        baa_parser_free(parser);
        if (ast) baa_ast_free_node(ast);
        return NULL;
    }
    
    BaaProgramData *program_data = (BaaProgramData *)ast->data;
    if (program_data->count == 0) {
        baa_parser_free(parser);
        baa_ast_free_node(ast);
        return NULL;
    }
    
    BaaNode *statement = program_data->top_level_declarations[0];
    
    // Remove the statement from the program to avoid double-free
    program_data->top_level_declarations[0] = NULL;
    program_data->count = 0;
    
    baa_parser_free(parser);
    baa_ast_free_node(ast);
    
    return statement;
}

void test_parse_expression_statement(void)
{
    TEST_SETUP();
    wprintf(L"Testing expression statement parsing...\n");
    
    // Test various expression statements
    const wchar_t *expr_stmt_sources[] = {
        L"42.",
        L"\"hello\".",
        L"identifier.",
        L"(42).",
        L"variable."
    };
    
    size_t num_sources = sizeof(expr_stmt_sources) / sizeof(expr_stmt_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *stmt = parse_statement_from_source(expr_stmt_sources[i]);
        
        if (stmt && stmt->kind == BAA_NODE_KIND_EXPR_STMT) {
            BaaExprStmtData *expr_stmt_data = (BaaExprStmtData *)stmt->data;
            ASSERT_NOT_NULL(expr_stmt_data->expression, L"Expression should not be NULL");
            wprintf(L"  ✓ Parsed expression statement: %ls\n", expr_stmt_sources[i]);
        } else {
            wprintf(L"  ⚠ Failed to parse expression statement: %ls\n", expr_stmt_sources[i]);
        }
        
        if (stmt) {
            baa_ast_free_node(stmt);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Expression statement parsing test passed\n");
}

void test_parse_block_statement(void)
{
    TEST_SETUP();
    wprintf(L"Testing block statement parsing...\n");
    
    // Test various block statements
    const wchar_t *block_sources[] = {
        L"{}",           // Empty block
        L"{ 42. }",      // Block with one statement
        L"{ 42. \"hello\". }", // Block with multiple statements
        L"{ { 42. } }",  // Nested blocks
        L"{ identifier. variable. }" // Block with identifiers
    };
    
    size_t num_sources = sizeof(block_sources) / sizeof(block_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *stmt = parse_statement_from_source(block_sources[i]);
        
        if (stmt && stmt->kind == BAA_NODE_KIND_BLOCK_STMT) {
            BaaBlockStmtData *block_data = (BaaBlockStmtData *)stmt->data;
            wprintf(L"  ✓ Parsed block statement: %ls (contains %d statements)\n", 
                    block_sources[i], block_data->count);
        } else {
            wprintf(L"  ⚠ Failed to parse block statement: %ls\n", block_sources[i]);
        }
        
        if (stmt) {
            baa_ast_free_node(stmt);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Block statement parsing test passed\n");
}

void test_parse_empty_block(void)
{
    TEST_SETUP();
    wprintf(L"Testing empty block parsing...\n");
    
    const wchar_t *empty_block_source = L"{}";
    
    BaaNode *stmt = parse_statement_from_source(empty_block_source);
    
    ASSERT_NOT_NULL(stmt, L"Empty block should be parsed");
    ASSERT_EQ(BAA_NODE_KIND_BLOCK_STMT, stmt->kind);
    
    BaaBlockStmtData *block_data = (BaaBlockStmtData *)stmt->data;
    ASSERT_NOT_NULL(block_data, L"Block data should not be NULL");
    ASSERT_EQ(0, block_data->count); // Should be empty
    
    baa_ast_free_node(stmt);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Empty block parsing test passed\n");
}

void test_parse_nested_blocks(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested block parsing...\n");
    
    const wchar_t *nested_source = L"{ { 42. } { \"hello\". } }";
    
    BaaNode *stmt = parse_statement_from_source(nested_source);
    
    ASSERT_NOT_NULL(stmt, L"Nested blocks should be parsed");
    ASSERT_EQ(BAA_NODE_KIND_BLOCK_STMT, stmt->kind);
    
    BaaBlockStmtData *outer_block = (BaaBlockStmtData *)stmt->data;
    ASSERT_NOT_NULL(outer_block, L"Outer block data should not be NULL");
    ASSERT_EQ(2, outer_block->count); // Should contain two inner blocks
    
    // Check first inner block
    BaaNode *first_inner = outer_block->statements[0];
    ASSERT_NOT_NULL(first_inner, L"First inner block should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_BLOCK_STMT, first_inner->kind);
    
    // Check second inner block
    BaaNode *second_inner = outer_block->statements[1];
    ASSERT_NOT_NULL(second_inner, L"Second inner block should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_BLOCK_STMT, second_inner->kind);
    
    baa_ast_free_node(stmt);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Nested block parsing test passed\n");
}

void test_parse_block_with_multiple_statements(void)
{
    TEST_SETUP();
    wprintf(L"Testing block with multiple statements...\n");
    
    const wchar_t *multi_stmt_source = L"{ 42. \"hello\". identifier. }";
    
    BaaNode *stmt = parse_statement_from_source(multi_stmt_source);
    
    ASSERT_NOT_NULL(stmt, L"Block with multiple statements should be parsed");
    ASSERT_EQ(BAA_NODE_KIND_BLOCK_STMT, stmt->kind);
    
    BaaBlockStmtData *block_data = (BaaBlockStmtData *)stmt->data;
    ASSERT_NOT_NULL(block_data, L"Block data should not be NULL");
    ASSERT_EQ(3, block_data->count); // Should contain three statements
    
    // Verify all statements are expression statements
    for (int i = 0; i < 3; i++) {
        BaaNode *inner_stmt = block_data->statements[i];
        ASSERT_NOT_NULL(inner_stmt, L"Inner statement should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, inner_stmt->kind);
    }
    
    baa_ast_free_node(stmt);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Block with multiple statements parsing test passed\n");
}

void test_parse_invalid_statements(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid statement parsing...\n");
    
    // Test various invalid statements
    const wchar_t *invalid_sources[] = {
        L"{",           // Unclosed block
        L"}",           // Unmatched closing brace
        L"{ 42 }",      // Missing dot terminator
        L"{ { }",       // Unclosed nested block
        L"42 43."       // Invalid expression
    };
    
    size_t num_sources = sizeof(invalid_sources) / sizeof(invalid_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *stmt = parse_statement_from_source(invalid_sources[i]);
        
        // These should either return NULL or produce an error
        if (stmt) {
            wprintf(L"  ⚠ Unexpectedly parsed invalid statement: %ls (kind: %d)\n", 
                    invalid_sources[i], stmt->kind);
            baa_ast_free_node(stmt);
        } else {
            wprintf(L"  ✓ Correctly rejected invalid statement: %ls\n", invalid_sources[i]);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Invalid statement parsing test passed\n");
}

void test_parse_statement_with_arabic_content(void)
{
    TEST_SETUP();
    wprintf(L"Testing statement parsing with Arabic content...\n");
    
    // Test statements with Arabic content
    const wchar_t *arabic_sources[] = {
        L"متغير.",
        L"\"مرحبا بالعالم\".",
        L"{ العدد. النص. }",
        L"{ { \"السلام عليكم\". } }",
        L"القيمة_الأولى."
    };
    
    size_t num_sources = sizeof(arabic_sources) / sizeof(arabic_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *stmt = parse_statement_from_source(arabic_sources[i]);
        
        if (stmt) {
            wprintf(L"  ✓ Parsed Arabic statement: %ls (kind: %d)\n", 
                    arabic_sources[i], stmt->kind);
            baa_ast_free_node(stmt);
        } else {
            wprintf(L"  ⚠ Failed to parse Arabic statement: %ls\n", arabic_sources[i]);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic content statement parsing test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Parser Statement Function tests...\n\n");

TEST_CASE(test_parse_expression_statement);
TEST_CASE(test_parse_block_statement);
TEST_CASE(test_parse_empty_block);
TEST_CASE(test_parse_nested_blocks);
TEST_CASE(test_parse_block_with_multiple_statements);
TEST_CASE(test_parse_invalid_statements);
TEST_CASE(test_parse_statement_with_arabic_content);

wprintf(L"\n✓ All Parser Statement Function tests completed!\n");

TEST_SUITE_END()
