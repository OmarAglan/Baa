#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"
#include "baa/ast/ast.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to run complete pipeline and return AST
BaaNode* parse_baa_code(const wchar_t* source_code, const char* filename)
{
    // Preprocessor
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = filename ? filename : "test_code",
        .data.source_string = source_code
    };
    
    wchar_t* error_message = NULL;
    wchar_t* preprocessed = baa_preprocess(&pp_source, NULL, &error_message);
    if (!preprocessed) {
        if (error_message) free(error_message);
        return NULL;
    }
    
    // Lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, preprocessed, L"test.baa");
    
    // Parser
    BaaParser* parser = baa_parser_create(&lexer, L"test.baa");
    if (!parser) {
        free(preprocessed);
        return NULL;
    }
    
    // AST
    BaaNode* ast = baa_parse_program(parser);
    
    // Cleanup
    baa_parser_free(parser);
    free(preprocessed);
    
    return ast;
}

void test_simple_program_with_macros(void)
{
    TEST_SETUP();
    wprintf(L"Testing simple program with macros...\n");
    
    const wchar_t* program = 
        L"#تعريف PI 3.14\n"
        L"#تعريف GREETING \"مرحبا بالعالم\"\n"
        L"PI.\n"
        L"GREETING.\n";
    
    BaaNode* ast = parse_baa_code(program, "simple_program.baa");
    ASSERT_NOT_NULL(ast, L"AST should be created for simple program");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_EQ(2, program_data->count); // Two statements
    
    // First statement: PI (should be expanded to 3.14)
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* first_expr = (BaaExprStmtData*)first_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, first_expr->expression->kind);
    
    BaaLiteralExprData* first_literal = (BaaLiteralExprData*)first_expr->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_FLOAT, first_literal->literal_kind);
    
    // Second statement: GREETING (should be expanded to Arabic string)
    BaaNode* second_stmt = program_data->top_level_declarations[1];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, second_stmt->kind);
    
    BaaExprStmtData* second_expr = (BaaExprStmtData*)second_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, second_expr->expression->kind);
    
    BaaLiteralExprData* second_literal = (BaaLiteralExprData*)second_expr->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, second_literal->literal_kind);
    
    baa_ast_free_node(ast);
    
    wprintf(L"  ✓ Simple program with macros processed successfully\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Simple program with macros test passed\n");
}

void test_conditional_compilation_program(void)
{
    TEST_SETUP();
    wprintf(L"Testing program with conditional compilation...\n");
    
    const wchar_t* program = 
        L"#تعريف DEBUG 1\n"
        L"#إذا DEBUG\n"
        L"\"Debug mode enabled\".\n"
        L"#إلا\n"
        L"\"Release mode\".\n"
        L"#نهاية_إذا\n"
        L"\"Program continues\".\n";
    
    BaaNode* ast = parse_baa_code(program, "conditional_program.baa");
    ASSERT_NOT_NULL(ast, L"AST should be created for conditional program");
    
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_EQ(2, program_data->count); // Should have debug message + continuation
    
    // First statement should be the debug message
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* first_expr = (BaaExprStmtData*)first_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, first_expr->expression->kind);
    
    BaaLiteralExprData* first_literal = (BaaLiteralExprData*)first_expr->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, first_literal->literal_kind);
    ASSERT_WSTR_CONTAINS(first_literal->value.string_value, L"Debug mode");
    
    // Second statement should be the continuation
    BaaNode* second_stmt = program_data->top_level_declarations[1];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, second_stmt->kind);
    
    BaaExprStmtData* second_expr = (BaaExprStmtData*)second_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, second_expr->expression->kind);
    
    BaaLiteralExprData* second_literal = (BaaLiteralExprData*)second_expr->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, second_literal->literal_kind);
    ASSERT_WSTR_CONTAINS(second_literal->value.string_value, L"Program continues");
    
    baa_ast_free_node(ast);
    
    wprintf(L"  ✓ Conditional compilation program processed successfully\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Conditional compilation program test passed\n");
}

void test_arabic_language_program(void)
{
    TEST_SETUP();
    wprintf(L"Testing full Arabic language program...\n");
    
    const wchar_t* arabic_program = 
        L"#تعريف العدد_الأقصى ١٠٠\n"
        L"#تعريف الرسالة \"مرحبا بالمطورين\"\n"
        L"العدد_الأقصى.\n"
        L"الرسالة.\n"
        L"متغير_عربي.\n"
        L"\"نص عربي آخر\".\n";
    
    BaaNode* ast = parse_baa_code(arabic_program, "arabic_program.baa");
    ASSERT_NOT_NULL(ast, L"AST should be created for Arabic program");
    
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_EQ(4, program_data->count); // Four statements
    
    // Verify each statement
    for (int i = 0; i < 4; i++) {
        BaaNode* stmt = program_data->top_level_declarations[i];
        ASSERT_NOT_NULL(stmt, L"Statement should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, stmt->kind);
        
        BaaExprStmtData* expr_data = (BaaExprStmtData*)stmt->data;
        ASSERT_NOT_NULL(expr_data->expression, L"Expression should not be NULL");
        
        // Should be either literal or identifier
        bool is_valid_expr = (expr_data->expression->kind == BAA_NODE_KIND_LITERAL_EXPR ||
                             expr_data->expression->kind == BAA_NODE_KIND_IDENTIFIER_EXPR);
        ASSERT_TRUE(is_valid_expr, L"Expression should be literal or identifier");
    }
    
    baa_ast_free_node(ast);
    
    wprintf(L"  ✓ Arabic language program processed successfully\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic language program test passed\n");
}

void test_mixed_content_program(void)
{
    TEST_SETUP();
    wprintf(L"Testing program with mixed Arabic and English content...\n");
    
    const wchar_t* mixed_program = 
        L"#تعريف VERSION \"1.0\"\n"
        L"#تعريف المؤلف \"Developer Name\"\n"
        L"VERSION.\n"
        L"المؤلف.\n"
        L"\"Mixed content: العربية and English\".\n"
        L"variable_name.\n"
        L"اسم_المتغير.\n";
    
    BaaNode* ast = parse_baa_code(mixed_program, "mixed_program.baa");
    ASSERT_NOT_NULL(ast, L"AST should be created for mixed content program");
    
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_EQ(5, program_data->count); // Five statements
    
    // Verify the mixed content string
    BaaNode* mixed_stmt = program_data->top_level_declarations[2];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, mixed_stmt->kind);
    
    BaaExprStmtData* mixed_expr = (BaaExprStmtData*)mixed_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, mixed_expr->expression->kind);
    
    BaaLiteralExprData* mixed_literal = (BaaLiteralExprData*)mixed_expr->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, mixed_literal->literal_kind);
    ASSERT_WSTR_CONTAINS(mixed_literal->value.string_value, L"العربية");
    ASSERT_WSTR_CONTAINS(mixed_literal->value.string_value, L"English");
    
    baa_ast_free_node(ast);
    
    wprintf(L"  ✓ Mixed content program processed successfully\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Mixed content program test passed\n");
}

void test_complex_macro_program(void)
{
    TEST_SETUP();
    wprintf(L"Testing program with complex macro usage...\n");
    
    const wchar_t* complex_program = 
        L"#تعريف MAX(a, b) ((a) > (b) ? (a) : (b))\n"
        L"#تعريف SQUARE(x) ((x) * (x))\n"
        L"#تعريف MESSAGE(name) \"Hello, \" name\n"
        L"MAX.\n"  // Note: Function-like macros without arguments might not expand
        L"SQUARE.\n"
        L"MESSAGE.\n";
    
    BaaNode* ast = parse_baa_code(complex_program, "complex_macro_program.baa");
    ASSERT_NOT_NULL(ast, L"AST should be created for complex macro program");
    
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_TRUE(program_data->count >= 1, L"Should have at least one statement");
    
    // Verify that the program was parsed (exact behavior depends on macro implementation)
    for (int i = 0; i < program_data->count; i++) {
        BaaNode* stmt = program_data->top_level_declarations[i];
        ASSERT_NOT_NULL(stmt, L"Statement should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, stmt->kind);
    }
    
    baa_ast_free_node(ast);
    
    wprintf(L"  ✓ Complex macro program processed successfully\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Complex macro program test passed\n");
}

void test_large_program_performance(void)
{
    TEST_SETUP();
    wprintf(L"Testing large program performance...\n");
    
    // Generate a larger program with many statements
    wchar_t* large_program = malloc(10000 * sizeof(wchar_t));
    ASSERT_NOT_NULL(large_program, L"Should allocate memory for large program");
    
    wcscpy(large_program, L"#تعريف VALUE 42\n");
    
    // Add many statements
    for (int i = 0; i < 50; i++) {
        wchar_t statement[100];
        swprintf(statement, 100, L"VALUE. \"Statement %d\". identifier_%d.\n", i, i);
        wcscat(large_program, statement);
    }
    
    BaaNode* ast = parse_baa_code(large_program, "large_program.baa");
    ASSERT_NOT_NULL(ast, L"AST should be created for large program");
    
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_EQ(150, program_data->count); // 50 * 3 statements per iteration
    
    // Verify a few random statements
    for (int i = 0; i < 10; i++) {
        int idx = i * 15; // Sample every 15th statement
        if (idx < program_data->count) {
            BaaNode* stmt = program_data->top_level_declarations[idx];
            ASSERT_NOT_NULL(stmt, L"Statement should not be NULL");
            ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, stmt->kind);
        }
    }
    
    baa_ast_free_node(ast);
    free(large_program);
    
    wprintf(L"  ✓ Large program (150 statements) processed successfully\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Large program performance test passed\n");
}

void test_edge_case_programs(void)
{
    TEST_SETUP();
    wprintf(L"Testing edge case programs...\n");
    
    // Test 1: Empty program
    const wchar_t* empty_program = L"";
    BaaNode* empty_ast = parse_baa_code(empty_program, "empty.baa");
    ASSERT_NOT_NULL(empty_ast, L"AST should be created for empty program");
    
    BaaProgramData* empty_data = (BaaProgramData*)empty_ast->data;
    ASSERT_EQ(0, empty_data->count); // No statements
    
    baa_ast_free_node(empty_ast);
    wprintf(L"  ✓ Empty program handled correctly\n");
    
    // Test 2: Only comments and whitespace
    const wchar_t* comment_program = L"// Comment only\n/* Multi-line comment */\n";
    BaaNode* comment_ast = parse_baa_code(comment_program, "comments.baa");
    ASSERT_NOT_NULL(comment_ast, L"AST should be created for comment-only program");
    
    BaaProgramData* comment_data = (BaaProgramData*)comment_ast->data;
    ASSERT_EQ(0, comment_data->count); // No statements (comments are ignored)
    
    baa_ast_free_node(comment_ast);
    wprintf(L"  ✓ Comment-only program handled correctly\n");
    
    // Test 3: Only preprocessor directives
    const wchar_t* directive_program = L"#تعريف UNUSED 42\n";
    BaaNode* directive_ast = parse_baa_code(directive_program, "directives.baa");
    ASSERT_NOT_NULL(directive_ast, L"AST should be created for directive-only program");
    
    BaaProgramData* directive_data = (BaaProgramData*)directive_ast->data;
    ASSERT_EQ(0, directive_data->count); // No statements (only directives)
    
    baa_ast_free_node(directive_ast);
    wprintf(L"  ✓ Directive-only program handled correctly\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Edge case programs test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Real-World Scenario tests...\n\n");

TEST_CASE(test_simple_program_with_macros);
TEST_CASE(test_conditional_compilation_program);
TEST_CASE(test_arabic_language_program);
TEST_CASE(test_mixed_content_program);
TEST_CASE(test_complex_macro_program);
TEST_CASE(test_large_program_performance);
TEST_CASE(test_edge_case_programs);

wprintf(L"\n✓ All Real-World Scenario tests completed!\n");

TEST_SUITE_END()
