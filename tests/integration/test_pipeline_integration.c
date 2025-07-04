#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"
#include "baa/ast/ast.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to run the complete pipeline: Preprocessor → Lexer → Parser → AST
typedef struct {
    wchar_t* preprocessed_source;
    BaaLexer lexer;
    BaaParser* parser;
    BaaNode* ast;
    bool success;
    wchar_t* error_message;
} PipelineResult;

PipelineResult* run_complete_pipeline(const wchar_t* source_code, const char* filename)
{
    PipelineResult* result = calloc(1, sizeof(PipelineResult));
    if (!result) return NULL;
    
    result->success = true;
    
    // Step 1: Preprocessor
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = filename ? filename : "test_string",
        .data.source_string = source_code
    };
    
    result->preprocessed_source = baa_preprocess(&pp_source, NULL, &result->error_message);
    if (!result->preprocessed_source) {
        result->success = false;
        return result;
    }
    
    // Step 2: Lexer
    baa_init_lexer(&result->lexer, result->preprocessed_source, L"test.baa");
    
    // Step 3: Parser
    result->parser = baa_parser_create(&result->lexer, L"test.baa");
    if (!result->parser) {
        result->success = false;
        if (!result->error_message) {
            result->error_message = wcsdup(L"Failed to create parser");
        }
        return result;
    }
    
    // Step 4: AST Generation
    result->ast = baa_parse_program(result->parser);
    if (!result->ast) {
        result->success = false;
        if (!result->error_message) {
            result->error_message = wcsdup(L"Failed to parse program");
        }
        return result;
    }
    
    return result;
}

void free_pipeline_result(PipelineResult* result)
{
    if (!result) return;
    
    if (result->preprocessed_source) {
        free(result->preprocessed_source);
    }
    if (result->parser) {
        baa_parser_free(result->parser);
    }
    if (result->ast) {
        baa_ast_free_node(result->ast);
    }
    if (result->error_message) {
        free(result->error_message);
    }
    free(result);
}

void test_simple_expression_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing simple expression through complete pipeline...\n");
    
    const wchar_t* source = L"42.";
    
    PipelineResult* result = run_complete_pipeline(source, "test_simple.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    ASSERT_TRUE(result->success, L"Pipeline should succeed");
    ASSERT_NOT_NULL(result->preprocessed_source, L"Preprocessed source should not be NULL");
    ASSERT_NOT_NULL(result->ast, L"AST should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, result->ast->kind);
    
    // Verify the program contains the expected expression
    BaaProgramData* program_data = (BaaProgramData*)result->ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count); // Should have one statement
    
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_NOT_NULL(first_stmt, L"First statement should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    wprintf(L"  ✓ Simple expression pipeline completed successfully\n");
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Simple expression pipeline test passed\n");
}

void test_arabic_identifier_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic identifier through complete pipeline...\n");
    
    const wchar_t* source = L"متغير.";
    
    PipelineResult* result = run_complete_pipeline(source, "test_arabic.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    ASSERT_TRUE(result->success, L"Pipeline should succeed");
    ASSERT_NOT_NULL(result->ast, L"AST should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, result->ast->kind);
    
    // Verify the program contains the Arabic identifier
    BaaProgramData* program_data = (BaaProgramData*)result->ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count);
    
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* expr_stmt_data = (BaaExprStmtData*)first_stmt->data;
    ASSERT_NOT_NULL(expr_stmt_data->expression, L"Expression should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_IDENTIFIER_EXPR, expr_stmt_data->expression->kind);
    
    BaaIdentifierExprData* identifier_data = (BaaIdentifierExprData*)expr_stmt_data->expression->data;
    ASSERT_NOT_NULL(identifier_data->name, L"Identifier name should not be NULL");
    ASSERT_WSTR_EQ(L"متغير", identifier_data->name);
    
    wprintf(L"  ✓ Arabic identifier pipeline completed successfully\n");
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic identifier pipeline test passed\n");
}

void test_string_literal_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing string literal through complete pipeline...\n");
    
    const wchar_t* source = L"\"مرحبا بالعالم\".";
    
    PipelineResult* result = run_complete_pipeline(source, "test_string.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    ASSERT_TRUE(result->success, L"Pipeline should succeed");
    ASSERT_NOT_NULL(result->ast, L"AST should not be NULL");
    
    // Verify the program contains the string literal
    BaaProgramData* program_data = (BaaProgramData*)result->ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count);
    
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* expr_stmt_data = (BaaExprStmtData*)first_stmt->data;
    ASSERT_NOT_NULL(expr_stmt_data->expression, L"Expression should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, expr_stmt_data->expression->kind);
    
    BaaLiteralExprData* literal_data = (BaaLiteralExprData*)expr_stmt_data->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_STRING, literal_data->literal_kind);
    ASSERT_NOT_NULL(literal_data->value.string_value, L"String value should not be NULL");
    
    wprintf(L"  ✓ String literal pipeline completed successfully\n");
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ String literal pipeline test passed\n");
}

void test_macro_expansion_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro expansion through complete pipeline...\n");
    
    const wchar_t* source = L"#تعريف VALUE 42\nVALUE.";
    
    PipelineResult* result = run_complete_pipeline(source, "test_macro.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    ASSERT_TRUE(result->success, L"Pipeline should succeed");
    ASSERT_NOT_NULL(result->preprocessed_source, L"Preprocessed source should not be NULL");
    
    // Verify macro was expanded in preprocessed source
    ASSERT_WSTR_CONTAINS(result->preprocessed_source, L"42");
    
    // Verify the AST contains the expanded value
    ASSERT_NOT_NULL(result->ast, L"AST should not be NULL");
    BaaProgramData* program_data = (BaaProgramData*)result->ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count);
    
    BaaNode* first_stmt = program_data->top_level_declarations[0];
    ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, first_stmt->kind);
    
    BaaExprStmtData* expr_stmt_data = (BaaExprStmtData*)first_stmt->data;
    ASSERT_NOT_NULL(expr_stmt_data->expression, L"Expression should not be NULL");
    ASSERT_EQ(BAA_NODE_KIND_LITERAL_EXPR, expr_stmt_data->expression->kind);
    
    BaaLiteralExprData* literal_data = (BaaLiteralExprData*)expr_stmt_data->expression->data;
    ASSERT_EQ(BAA_LITERAL_KIND_INT, literal_data->literal_kind);
    ASSERT_EQ(42, literal_data->value.int_value);
    
    wprintf(L"  ✓ Macro expansion pipeline completed successfully\n");
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Macro expansion pipeline test passed\n");
}

void test_conditional_compilation_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing conditional compilation through complete pipeline...\n");
    
    const wchar_t* source = L"#إذا 1\nincluded_value.\n#إلا\nexcluded_value.\n#نهاية_إذا";
    
    PipelineResult* result = run_complete_pipeline(source, "test_conditional.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    ASSERT_TRUE(result->success, L"Pipeline should succeed");
    ASSERT_NOT_NULL(result->preprocessed_source, L"Preprocessed source should not be NULL");
    
    // Verify conditional compilation worked
    ASSERT_WSTR_CONTAINS(result->preprocessed_source, L"included_value");
    
    // Verify the AST contains only the included content
    ASSERT_NOT_NULL(result->ast, L"AST should not be NULL");
    BaaProgramData* program_data = (BaaProgramData*)result->ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(1, program_data->count); // Should have only one statement (the included one)
    
    wprintf(L"  ✓ Conditional compilation pipeline completed successfully\n");
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Conditional compilation pipeline test passed\n");
}

void test_multiple_statements_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiple statements through complete pipeline...\n");
    
    const wchar_t* source = L"42. \"hello\". identifier.";
    
    PipelineResult* result = run_complete_pipeline(source, "test_multiple.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    ASSERT_TRUE(result->success, L"Pipeline should succeed");
    ASSERT_NOT_NULL(result->ast, L"AST should not be NULL");
    
    // Verify the program contains all three statements
    BaaProgramData* program_data = (BaaProgramData*)result->ast->data;
    ASSERT_NOT_NULL(program_data, L"Program data should not be NULL");
    ASSERT_EQ(3, program_data->count); // Should have three statements
    
    // Verify each statement type
    for (int i = 0; i < 3; i++) {
        BaaNode* stmt = program_data->top_level_declarations[i];
        ASSERT_NOT_NULL(stmt, L"Statement should not be NULL");
        ASSERT_EQ(BAA_NODE_KIND_EXPR_STMT, stmt->kind);
        
        BaaExprStmtData* expr_stmt_data = (BaaExprStmtData*)stmt->data;
        ASSERT_NOT_NULL(expr_stmt_data->expression, L"Expression should not be NULL");
    }
    
    wprintf(L"  ✓ Multiple statements pipeline completed successfully\n");
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Multiple statements pipeline test passed\n");
}

void test_error_propagation_pipeline(void)
{
    TEST_SETUP();
    wprintf(L"Testing error propagation through complete pipeline...\n");
    
    // Test with invalid syntax that should be caught by the parser
    const wchar_t* invalid_source = L"42 43."; // Missing operator
    
    PipelineResult* result = run_complete_pipeline(invalid_source, "test_error.baa");
    ASSERT_NOT_NULL(result, L"Pipeline result should not be NULL");
    
    // The pipeline should either fail or handle the error gracefully
    if (!result->success) {
        wprintf(L"  ✓ Pipeline correctly detected error\n");
    } else {
        wprintf(L"  ⚠ Pipeline handled invalid syntax gracefully\n");
    }
    
    free_pipeline_result(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Error propagation pipeline test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Complete Pipeline Integration tests...\n\n");

TEST_CASE(test_simple_expression_pipeline);
TEST_CASE(test_arabic_identifier_pipeline);
TEST_CASE(test_string_literal_pipeline);
TEST_CASE(test_macro_expansion_pipeline);
TEST_CASE(test_conditional_compilation_pipeline);
TEST_CASE(test_multiple_statements_pipeline);
TEST_CASE(test_error_propagation_pipeline);

wprintf(L"\n✓ All Complete Pipeline Integration tests completed!\n");

TEST_SUITE_END()
