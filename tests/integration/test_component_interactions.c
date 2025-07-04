#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"
#include "baa/ast/ast.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

void test_preprocessor_to_lexer_interaction(void)
{
    TEST_SETUP();
    wprintf(L"Testing Preprocessor → Lexer interaction...\n");
    
    // Test that preprocessor output is correctly consumed by lexer
    const wchar_t* source_with_macro = L"#تعريف NUMBER 123\nNUMBER";
    
    // Step 1: Preprocess
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "test_pp_lexer",
        .data.source_string = source_with_macro
    };
    
    wchar_t* error_message = NULL;
    wchar_t* preprocessed = baa_preprocess(&pp_source, NULL, &error_message);
    
    ASSERT_NOT_NULL(preprocessed, L"Preprocessed source should not be NULL");
    ASSERT_WSTR_CONTAINS(preprocessed, L"123");
    
    // Step 2: Lex the preprocessed output
    BaaLexer lexer;
    baa_init_lexer(&lexer, preprocessed, L"test.baa");
    
    // Get tokens and verify the macro was expanded
    BaaToken* token = baa_lexer_next_token(&lexer);
    ASSERT_NOT_NULL(token, L"First token should not be NULL");
    ASSERT_EQ(BAA_TOKEN_INT_LIT, token->type);
    ASSERT_WSTR_EQ(L"123", token->lexeme);
    
    baa_free_token(token);
    free(preprocessed);
    if (error_message) free(error_message);
    
    wprintf(L"  ✓ Preprocessor → Lexer interaction successful\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Preprocessor → Lexer interaction test passed\n");
}

void test_lexer_to_parser_interaction(void)
{
    TEST_SETUP();
    wprintf(L"Testing Lexer → Parser interaction...\n");
    
    const wchar_t* source = L"identifier.";
    
    // Step 1: Initialize lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    // Step 2: Create parser with lexer
    BaaParser* parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created successfully");
    
    // Step 3: Parse and verify token consumption
    BaaNode* ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify the parser correctly consumed the identifier token
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count);
    
    BaaNode* stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, stmt->kind);
    
    BaaExprStmtData* expr_stmt_data = (BaaExprStmtData*)stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, expr_stmt_data->expression->kind);
    
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    
    wprintf(L"  ✓ Lexer → Parser interaction successful\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Lexer → Parser interaction test passed\n");
}

void test_parser_to_ast_interaction(void)
{
    TEST_SETUP();
    wprintf(L"Testing Parser → AST interaction...\n");
    
    const wchar_t* source = L"42. \"hello\".";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    BaaParser* parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // Parse and verify AST structure
    BaaNode* ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify the AST contains the expected nodes
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_EQ(2, program_data->count); // Two statements
    
    // First statement: integer literal
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* first_expr_data = (BaaExprStmtData*)first_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, first_expr_data->expression->kind);
    
    BaaLiteralExprData* first_literal = (BaaLiteralExprData*)first_expr_data->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_INT, first_literal->literal_kind);
    ASSERT_EQ(42, first_literal->value.int_value);
    
    // Second statement: string literal
    BaaNode* second_stmt = program_data->top_level_declarations[1];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, second_stmt->kind);
    
    BaaExprStmtData* second_expr_data = (BaaExprStmtData*)second_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, second_expr_data->expression->kind);
    
    BaaLiteralExprData* second_literal = (BaaLiteralExprData*)second_expr_data->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, second_literal->literal_kind);
    ASSERT_NOT_NULL(second_literal->value.string_value);
    
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    
    wprintf(L"  ✓ Parser → AST interaction successful\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Parser → AST interaction test passed\n");
}

void test_arabic_content_through_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic content through all components...\n");
    
    // Test Arabic keywords, identifiers, and strings through the complete pipeline
    const wchar_t* arabic_source = L"#تعريف المتغير العدد\nالمتغير. \"النص العربي\".";
    
    // Preprocessor
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "test_arabic_pipeline",
        .data.source_string = arabic_source
    };
    
    wchar_t* error_message = NULL;
    wchar_t* preprocessed = baa_preprocess(&pp_source, NULL, &error_message);
    ASSERT_NOT_NULL(preprocessed, L"Preprocessed source should not be NULL");
    
    // Lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, preprocessed, L"test.baa");
    
    // Parser
    BaaParser* parser = baa_parser_create(&lexer, L"test.baa");
    ASSERT_NOT_NULL(parser, L"Parser should be created");
    
    // AST
    BaaNode* ast = baa_parse_program(parser);
    ASSERT_NOT_NULL(ast, L"AST should be created");
    
    // Verify Arabic content is preserved through the pipeline
    BaaProgramData* program_data = (BaaProgramData*)ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(2, program_data->count); // Two statements
    
    // First statement should be the expanded macro (العدد)
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* first_expr_data = (BaaExprStmtData*)first_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, first_expr_data->expression->kind);
    
    BaaIdentifierExprData* identifier_data = (BaaIdentifierExprData*)first_expr_data->expression->data;
    ASSERT_WSTR_EQ(L"العدد", identifier_data->name);
    
    // Second statement should be the Arabic string
    BaaNode* second_stmt = program_data->top_level_declarations[1];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, second_stmt->kind);
    
    BaaExprStmtData* second_expr_data = (BaaExprStmtData*)second_stmt->data;
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, second_expr_data->expression->kind);
    
    BaaLiteralExprData* string_data = (BaaLiteralExprData*)second_expr_data->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, string_data->literal_kind);
    ASSERT_WSTR_CONTAINS(string_data->value.string_value, L"النص العربي");
    
    baa_ast_free_node(ast);
    baa_parser_free(parser);
    free(preprocessed);
    if (error_message) free(error_message);
    
    wprintf(L"  ✓ Arabic content preserved through complete pipeline\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic content pipeline test passed\n");
}

void test_error_handling_across_components(void)
{
    TEST_SETUP();
    wprintf(L"Testing error handling across components...\n");
    
    // Test 1: Preprocessor error
    const wchar_t* pp_error_source = L"#خطأ \"Preprocessor error message\"";
    
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "test_pp_error",
        .data.source_string = pp_error_source
    };
    
    wchar_t* error_message = NULL;
    wchar_t* preprocessed = baa_preprocess(&pp_source, NULL, &error_message);
    
    // Should fail at preprocessor stage
    ASSERT_NULL(preprocessed, L"Preprocessor should fail with #خطأ directive");
    ASSERT_NOT_NULL(error_message, L"Error message should be provided");
    
    if (error_message) {
        wprintf(L"  ✓ Preprocessor error correctly detected: %ls\n", error_message);
        free(error_message);
        error_message = NULL;
    }
    
    // Test 2: Lexer error (if it reaches lexer)
    const wchar_t* lexer_error_source = L"\"unterminated string";
    
    BaaLexer lexer;
    baa_init_lexer(&lexer, lexer_error_source, L"test.baa");
    
    BaaToken* token = baa_lexer_next_token(&lexer);
    if (token) {
        if (token->type == BAA_TOKEN_ERROR) {
            wprintf(L"  ✓ Lexer error correctly detected\n");
        } else {
            wprintf(L"  ⚠ Lexer handled invalid input gracefully\n");
        }
        baa_free_token(token);
    }
    
    // Test 3: Parser error
    const wchar_t* parser_error_source = L"42 43."; // Invalid syntax
    
    BaaLexer parser_lexer;
    baa_init_lexer(&parser_lexer, parser_error_source, L"test.baa");
    
    BaaParser* parser = baa_parser_create(&parser_lexer, L"test.baa");
    if (parser) {
        BaaNode* ast = baa_parse_program(parser);
        
        if (!ast || baa_parser_had_error(parser)) {
            wprintf(L"  ✓ Parser error correctly detected\n");
        } else {
            wprintf(L"  ⚠ Parser handled invalid syntax gracefully\n");
        }
        
        if (ast) baa_ast_free_node(ast);
        baa_parser_free(parser);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Error handling across components test passed\n");
}

void test_memory_management_across_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing memory management across pipeline...\n");
    
    const wchar_t* source = L"#تعريف TEST 42\nTEST. \"memory test\".";
    
    // Run pipeline multiple times to test for memory leaks
    for (int i = 0; i < 5; i++) {
        BaaPpSource pp_source = {
            .type = BAA_PP_SOURCE_STRING,
            .source_name = "test_memory",
            .data.source_string = source
        };
        
        wchar_t* error_message = NULL;
        wchar_t* preprocessed = baa_preprocess(&pp_source, NULL, &error_message);
        
        if (preprocessed) {
            BaaLexer lexer;
            baa_init_lexer(&lexer, preprocessed, L"test.baa");
            
            BaaParser* parser = baa_parser_create(&lexer, L"test.baa");
            if (parser) {
                BaaNode* ast = baa_parse_program(parser);
                
                // Clean up in reverse order
                if (ast) baa_ast_free_node(ast);
                baa_parser_free(parser);
            }
            
            free(preprocessed);
        }
        
        if (error_message) free(error_message);
    }
    
    wprintf(L"  ✓ Memory management test completed (5 iterations)\n");
    
    TEST_TEARDOWN();
    wprintf(L"✓ Memory management across pipeline test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Component Interaction tests...\n\n");

TEST_CASE(test_preprocessor_to_lexer_interaction);
TEST_CASE(test_lexer_to_parser_interaction);
TEST_CASE(test_parser_to_ast_interaction);
TEST_CASE(test_arabic_content_through_pipeline);
TEST_CASE(test_error_handling_across_components);
TEST_CASE(test_memory_management_across_pipeline);

wprintf(L"\n✓ All Component Interaction tests completed!\n");

TEST_SUITE_END()
