#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h"
#include "test_framework.h"
#include <wchar.h>
#include <stdlib.h>

void test_simple_program(void)
{
    const wchar_t *test_file = L"tests/resources/test_files/simple.txt";
    wchar_t *content = baa_read_file(test_file);
    ASSERT_NOT_NULL(content, L"Failed to read simple.txt");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test basic tokens
    BaaToken *token;

    // First line: comment
    token = baa_lexer_next_token(lexer);
    ASSERT_NOT_NULL(token, L"Failed to get comment token");
    ASSERT_EQUAL(token->type, BAA_TOKEN_SINGLE_LINE_COMMENT, L"Expected SINGLE_LINE_COMMENT token");

    // Skip preprocessor directive (#تضمين) as lexer ignores them
    // <
    token = baa_lexer_next_token(lexer);
    ASSERT_NOT_NULL(token, L"Failed to get less token");
    ASSERT_EQUAL(token->type, BAA_TOKEN_LESS, L"Expected LESS token");
    ASSERT_STR_EQ(token->lexeme, L"<");

    // مكتبة_طباعة
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IDENTIFIER, L"Expected IDENTIFIER token");
    ASSERT_STR_EQ(token->lexeme, L"مكتبة_طباعة");

    // >
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_GREATER, L"Expected GREATER token");

    // .
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_DOT, L"Expected DOT token");

    baa_free_lexer(lexer);
    free(content);
}

void test_program_test(void)
{
    const wchar_t *test_file = L"tests/resources/test_files/program_test.txt";
    wchar_t *content = baa_read_file(test_file);
    ASSERT_NOT_NULL(content, L"Failed to read program_test.txt");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test basic tokens
    BaaToken *token;

    // First line: comment
    token = baa_lexer_next_token(lexer);
    ASSERT_NOT_NULL(token, L"Failed to get comment token");
    ASSERT_EQUAL(token->type, BAA_TOKEN_SINGLE_LINE_COMMENT, L"Expected SINGLE_LINE_COMMENT token");

    // Skip preprocessor directive (#تضمين) as lexer ignores them
    // <
    token = baa_lexer_next_token(lexer);
    ASSERT_NOT_NULL(token, L"Failed to get less token");
    ASSERT_EQUAL(token->type, BAA_TOKEN_LESS, L"Expected LESS token");
    ASSERT_STR_EQ(token->lexeme, L"<");

    // نظام/طباعة
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IDENTIFIER, L"Expected IDENTIFIER token");
    ASSERT_STR_EQ(token->lexeme, L"نظام/طباعة");

    baa_free_lexer(lexer);
    free(content);
}

TEST_SUITE_BEGIN()
    TEST_CASE(test_simple_program);
    TEST_CASE(test_program_test);
TEST_SUITE_END()
