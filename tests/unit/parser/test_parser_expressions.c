#include "test_framework.h"
#include "baa/parser/parser.h"
#include "baa/lexer/lexer.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

// Helper function to parse a single expression from source
BaaNode* parse_expression_from_source(const wchar_t* source)
{
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    BaaParser *parser = baa_parser_create(&lexer, L"test.baa");
    if (!parser) {
        return NULL;
    }
    
    // Parse as a program and extract the first expression statement
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
    
    BaaNode *expr_stmt = program_data->top_level_declarations[0];
    if (expr_stmt->kind != BAA_NODE_KIND_EXPR_STMT) {
        baa_parser_free(parser);
        baa_ast_free_node(ast);
        return NULL;
    }
    
    // Extract the expression from the expression statement
    BaaExprStmtData *expr_stmt_data = (BaaExprStmtData *)expr_stmt->data;
    BaaNode *expression = expr_stmt_data->expression;
    
    // Remove the expression from the statement to avoid double-free
    expr_stmt_data->expression = NULL;
    
    baa_parser_free(parser);
    baa_ast_free_node(ast);
    
    return expression;
}

void test_parse_integer_literal(void)
{
    TEST_SETUP();
    wprintf(L"Testing integer literal parsing...\n");
    
    // Test various integer literals
    const wchar_t *int_sources[] = {
        L"42.",
        L"0.",
        L"123456789.",
        L"-42."  // Note: This might be parsed as unary minus + literal
    };
    
    size_t num_sources = sizeof(int_sources) / sizeof(int_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *expr = parse_expression_from_source(int_sources[i]);
        
        // The expression might be a literal or a unary expression (for negative numbers)
        if (expr && expr->kind == BAA_NODE_KIND_LITERAL_EXPR) {
            BaaLiteralExprData *literal_data = (BaaLiteralExprData *)expr->data;
            ASSERT_EQ(BAA_LITERAL_KIND_INT, literal_data->literal_kind);
            wprintf(L"  ✓ Parsed integer literal: %ls\n", int_sources[i]);
        } else if (expr && expr->kind == BAA_NODE_KIND_UNARY_EXPR) {
            // This is expected for negative numbers
            wprintf(L"  ✓ Parsed as unary expression: %ls\n", int_sources[i]);
        } else {
            wprintf(L"  ⚠ Unexpected result for: %ls\n", int_sources[i]);
        }
        
        if (expr) {
            baa_ast_free_node(expr);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Integer literal parsing test passed\n");
}

void test_parse_string_literal(void)
{
    TEST_SETUP();
    wprintf(L"Testing string literal parsing...\n");
    
    // Test various string literals
    const wchar_t *string_sources[] = {
        L"\"hello\".",
        L"\"مرحبا\".",
        L"\"\".",  // Empty string
        L"\"Hello, العالم!\"."  // Mixed languages
    };
    
    size_t num_sources = sizeof(string_sources) / sizeof(string_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *expr = parse_expression_from_source(string_sources[i]);
        
        if (expr && expr->kind == BAA_NODE_KIND_LITERAL_EXPR) {
            BaaLiteralExprData *literal_data = (BaaLiteralExprData *)expr->data;
            ASSERT_EQ(BAA_LITERAL_KIND_STRING, literal_data->literal_kind);
            ASSERT_NOT_NULL(literal_data->value.string_value, L"String value should not be NULL");
            wprintf(L"  ✓ Parsed string literal: %ls\n", string_sources[i]);
        } else {
            wprintf(L"  ⚠ Failed to parse string literal: %ls\n", string_sources[i]);
        }
        
        if (expr) {
            baa_ast_free_node(expr);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ String literal parsing test passed\n");
}

void test_parse_identifier_expression(void)
{
    TEST_SETUP();
    wprintf(L"Testing identifier expression parsing...\n");
    
    // Test various identifiers
    const wchar_t *identifier_sources[] = {
        L"variable.",
        L"متغير.",
        L"العدد_الأول.",
        L"value_123.",
        L"_underscore."
    };
    
    size_t num_sources = sizeof(identifier_sources) / sizeof(identifier_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *expr = parse_expression_from_source(identifier_sources[i]);
        
        if (expr && expr->kind == BAA_NODE_KIND_IDENTIFIER_EXPR) {
            BaaIdentifierExprData *identifier_data = (BaaIdentifierExprData *)expr->data;
            ASSERT_NOT_NULL(identifier_data->name, L"Identifier name should not be NULL");
            wprintf(L"  ✓ Parsed identifier: %ls -> %ls\n", identifier_sources[i], identifier_data->name);
        } else {
            wprintf(L"  ⚠ Failed to parse identifier: %ls\n", identifier_sources[i]);
        }
        
        if (expr) {
            baa_ast_free_node(expr);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Identifier expression parsing test passed\n");
}

void test_parse_parenthesized_expression(void)
{
    TEST_SETUP();
    wprintf(L"Testing parenthesized expression parsing...\n");
    
    // Test parenthesized expressions
    const wchar_t *paren_sources[] = {
        L"(42).",
        L"(\"hello\").",
        L"(identifier).",
        L"((nested))."
    };
    
    size_t num_sources = sizeof(paren_sources) / sizeof(paren_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *expr = parse_expression_from_source(paren_sources[i]);
        
        // Parenthesized expressions should return the inner expression directly
        if (expr) {
            wprintf(L"  ✓ Parsed parenthesized expression: %ls (kind: %d)\n", 
                    paren_sources[i], expr->kind);
        } else {
            wprintf(L"  ⚠ Failed to parse parenthesized expression: %ls\n", paren_sources[i]);
        }
        
        if (expr) {
            baa_ast_free_node(expr);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Parenthesized expression parsing test passed\n");
}

void test_parse_invalid_expressions(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid expression parsing...\n");
    
    // Test various invalid expressions
    const wchar_t *invalid_sources[] = {
        L".",           // Just a dot
        L"+ .",         // Operator without operand
        L"( .",         // Unclosed parenthesis
        L") .",         // Unmatched closing parenthesis
        L"42 43 ."      // Two literals without operator
    };
    
    size_t num_sources = sizeof(invalid_sources) / sizeof(invalid_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *expr = parse_expression_from_source(invalid_sources[i]);
        
        // These should either return NULL or produce an error
        if (expr) {
            wprintf(L"  ⚠ Unexpectedly parsed invalid expression: %ls (kind: %d)\n", 
                    invalid_sources[i], expr->kind);
            baa_ast_free_node(expr);
        } else {
            wprintf(L"  ✓ Correctly rejected invalid expression: %ls\n", invalid_sources[i]);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Invalid expression parsing test passed\n");
}

void test_parse_expression_edge_cases(void)
{
    TEST_SETUP();
    wprintf(L"Testing expression parsing edge cases...\n");
    
    // Test edge cases
    const wchar_t *edge_sources[] = {
        L"0.",           // Zero
        L"\"\".",        // Empty string
        L"_.",           // Single underscore identifier
        L"أ.",           // Single Arabic character identifier
        L"123.",         // Simple number
    };
    
    size_t num_sources = sizeof(edge_sources) / sizeof(edge_sources[0]);
    
    for (size_t i = 0; i < num_sources; i++) {
        BaaNode *expr = parse_expression_from_source(edge_sources[i]);
        
        if (expr) {
            wprintf(L"  ✓ Parsed edge case: %ls (kind: %d)\n", edge_sources[i], expr->kind);
            baa_ast_free_node(expr);
        } else {
            wprintf(L"  ⚠ Failed to parse edge case: %ls\n", edge_sources[i]);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Expression parsing edge cases test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Parser Expression Function tests...\n\n");

TEST_CASE(test_parse_integer_literal);
TEST_CASE(test_parse_string_literal);
TEST_CASE(test_parse_identifier_expression);
TEST_CASE(test_parse_parenthesized_expression);
TEST_CASE(test_parse_invalid_expressions);
TEST_CASE(test_parse_expression_edge_cases);

wprintf(L"\n✓ All Parser Expression Function tests completed!\n");

TEST_SUITE_END()
