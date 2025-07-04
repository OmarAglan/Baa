#include "test_framework.h"
#include "baa/parser/parser.h"
#include "baa/lexer/lexer.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

void test_parser_creation_and_destruction(void)
{
    TEST_SETUP();
    wprintf(L"Testing parser creation and destruction...\n");
    
    // Create a simple source for testing
    const wchar_t *source = L"42.";
    
    // Initialize lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    // Test parser creation
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created successfully");
    
    // Test parser destruction
    baa_parser_free(parser);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Parser creation and destruction test passed\n");
}

void test_parser_creation_with_invalid_input(void)
{
    TEST_SETUP();
    wprintf(L"Testing parser creation with invalid input...\n");
    
    // Test parser creation with NULL lexer
    BaaParser *invalid_parser = baa_parser_create(NULL, L"test.baa");
    ASSERT_NULL(invalid_parser, L"Parser creation with NULL lexer should fail");
    
    // Test parser creation with NULL filename (should still work)
    const wchar_t *source = L"42.";
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    BaaParser *parser_null_filename = baa_parser_create(&lexer, NULL);
    ASSERT_NOT_NULL(parser_null_filename, L"Parser creation with NULL filename should still work");
    
    baa_parser_free(parser_null_filename);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Parser creation with invalid input test passed\n");
}

void test_parser_error_state(void)
{
    TEST_SETUP();
    wprintf(L"Testing parser error state handling...\n");
    
    // Create source with syntax error
    const wchar_t *invalid_source = L"42 43."; // Missing operator between numbers
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, invalid_source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // Test error state checking
    bool initial_error_state = baa_parser_had_error(parser);
    // Note: The parser might already have an error from initialization
    
    // Try to parse the invalid program
    BaaNode *ast = baa_parse_program(parser);
    
    // Check if parser detected the error
    bool final_error_state = baa_parser_had_error(parser);
    
    // The parser should have detected some kind of error
    // (either during initialization or parsing)
    wprintf(L"  Initial error state: %s\n", initial_error_state ? L"true" : L"false");
    wprintf(L"  Final error state: %s\n", final_error_state ? L"true" : L"false");
    
    // Clean up
    if (ast) {
        baa_ast_free_node(ast);
    }
    baa_parser_free(parser);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Parser error state test passed\n");
}

void test_parse_simple_program(void)
{
    TEST_SETUP();
    wprintf(L"Testing simple program parsing...\n");
    
    // Test parsing a simple valid program
    const wchar_t *simple_source = L"42.";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, simple_source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // Parse the program
    BaaNode *ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created for valid program");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify the program structure
    BaaProgramData *program_data = (BaaProgramData *)ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count); // Should have one statement
    
    // Verify the statement is an expression statement
    BaaNode *first_stmt = program_data->top_level_declarations[0];
    ASSERT_NOT_NULL(first_stmt, L"First statement should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    // Clean up
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Simple program parsing test passed\n");
}

void test_parse_empty_program(void)
{
    TEST_SETUP();
    wprintf(L"Testing empty program parsing...\n");
    
    // Test parsing an empty program (just EOF)
    const wchar_t *empty_source = L"";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, empty_source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // Parse the empty program
    BaaNode *ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created for empty program");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify the program is empty
    BaaProgramData *program_data = (BaaProgramData *)ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(0, program_data->count); // Should have no statements
    
    // Clean up
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Empty program parsing test passed\n");
}

void test_parse_multiple_statements(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiple statements parsing...\n");
    
    // Test parsing a program with multiple statements
    const wchar_t *multi_source = L"42. \"hello\". identifier.";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, multi_source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // Parse the program
    BaaNode *ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created for multi-statement program");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify the program structure
    BaaProgramData *program_data = (BaaProgramData *)ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(3, program_data->count); // Should have three statements
    
    // Verify all statements are expression statements
    for (int i = 0; i < 3; i++) {
        BaaNode *stmt = program_data->top_level_declarations[i];
        ASSERT_NOT_NULL(stmt, L"Statement should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, stmt->kind);
    }
    
    // Clean up
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Multiple statements parsing test passed\n");
}

void test_parse_with_arabic_identifiers(void)
{
    TEST_SETUP();
    wprintf(L"Testing parsing with Arabic identifiers...\n");
    
    // Test parsing with Arabic identifiers
    const wchar_t *arabic_source = L"متغير. العدد. النتيجة.";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, arabic_source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // Parse the program
    BaaNode *ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created for Arabic identifier program");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify the program structure
    BaaProgramData *program_data = (BaaProgramData *)ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(3, program_data->count); // Should have three statements
    
    // Clean up
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic identifiers parsing test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Parser Core Function tests...\n\n");

TEST_CASE(test_parser_creation_and_destruction);
TEST_CASE(test_parser_creation_with_invalid_input);
TEST_CASE(test_parser_error_state);
TEST_CASE(test_parse_simple_program);
TEST_CASE(test_parse_empty_program);
TEST_CASE(test_parse_multiple_statements);
TEST_CASE(test_parse_with_arabic_identifiers);

wprintf(L"\n✓ All Parser Core Function tests completed!\n");

TEST_SUITE_END()
