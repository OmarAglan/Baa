#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h"
#include "test_framework.h"
#include <wchar.h>
#include <stdlib.h>

// Helper function to create a test file with UTF-16LE encoding
static void create_test_file(const wchar_t *filename, const wchar_t *content)
{
    FILE *file;
    _wfopen_s(&file, filename, L"wb, ccs=UTF-16LE");
    ASSERT_NOT_NULL(file, L"Failed to create test file");

    // Write BOM
    const wchar_t bom = 0xFEFF;
    fwrite(&bom, sizeof(wchar_t), 1, file);

    // Write content
    fwrite(content, sizeof(wchar_t), wcslen(content), file);
    fclose(file);
}

void test_keywords(void)
{
    const wchar_t *test_filename = L"test_keywords.txt";
    const wchar_t *test_content = L"دالة متغير ثابت إذا وإلا طالما لكل إرجع";

    create_test_file(test_filename, test_content);
    wchar_t *content = baa_file_content(test_filename);
    ASSERT_NOT_NULL(content, L"Failed to read test file");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test each keyword
    BaaToken *token;

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CONST, L"Expected CONST token");
    ASSERT_STR_EQ(token->lexeme, L"ثابت");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IF, L"Expected IF token");
    ASSERT_STR_EQ(token->lexeme, L"إذا");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_ELSE, L"Expected ELSE token");
    ASSERT_STR_EQ(token->lexeme, L"وإلا");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_WHILE, L"Expected WHILE token");
    ASSERT_STR_EQ(token->lexeme, L"طالما");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_FOR, L"Expected FOR token");
    ASSERT_STR_EQ(token->lexeme, L"لكل");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_RETURN, L"Expected RETURN token");
    ASSERT_STR_EQ(token->lexeme, L"إرجع");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EOF, L"Expected EOF token");

    baa_free_lexer(lexer);
    free(content);
    _wremove(test_filename);
}

void test_operators(void)
{
    const wchar_t *test_filename = L"test_operators.txt";
    const wchar_t *test_content = L"+ - * / % = == != < <= > >= && || ! ++ -- += -= *= /= %=";

    create_test_file(test_filename, test_content);
    wchar_t *content = baa_file_content(test_filename);
    ASSERT_NOT_NULL(content, L"Failed to read test file");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test each operator
    BaaToken *token;
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_PLUS, L"Expected PLUS token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_MINUS, L"Expected MINUS token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_STAR, L"Expected STAR token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_SLASH, L"Expected SLASH token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_PERCENT, L"Expected PERCENT token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EQUAL, L"Expected EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EQUAL_EQUAL, L"Expected EQUAL_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_BANG_EQUAL, L"Expected BANG_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_LESS, L"Expected LESS token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_LESS_EQUAL, L"Expected LESS_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_GREATER, L"Expected GREATER token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_GREATER_EQUAL, L"Expected GREATER_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_AND, L"Expected AND token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_OR, L"Expected OR token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_BANG, L"Expected BANG token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_INCREMENT, L"Expected INCREMENT token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_DECREMENT, L"Expected DECREMENT token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_PLUS_EQUAL, L"Expected PLUS_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_MINUS_EQUAL, L"Expected MINUS_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_STAR_EQUAL, L"Expected STAR_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_SLASH_EQUAL, L"Expected SLASH_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_PERCENT_EQUAL, L"Expected PERCENT_EQUAL token");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EOF, L"Expected EOF token");

    baa_free_lexer(lexer);
    free(content);
    _wremove(test_filename);
}

void test_string_literals(void)
{
    const wchar_t *test_filename = L"test_strings.txt";
    const wchar_t *test_content = L"\"Hello\" \"مرحبا\" \"\\n\\t\\\"\\\\\"";

    create_test_file(test_filename, test_content);
    wchar_t *content = baa_file_content(test_filename);
    ASSERT_NOT_NULL(content, L"Failed to read test file");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test string literals
    BaaToken *token;
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_STRING_LIT, L"Expected STRING_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"\"Hello\"");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_STRING_LIT, L"Expected STRING_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"\"مرحبا\"");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_STRING_LIT, L"Expected STRING_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"\"\\n\\t\\\"\\\\\"");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EOF, L"Expected EOF token");

    baa_free_lexer(lexer);
    free(content);
    _wremove(test_filename);
}

void test_character_literals(void)
{
    const wchar_t *test_filename = L"test_chars.txt";
    const wchar_t *test_content = L"'a' 'ب' '\\n' '\\t' '\\'' '\\\\'";

    create_test_file(test_filename, test_content);
    wchar_t *content = baa_file_content(test_filename);
    ASSERT_NOT_NULL(content, L"Failed to read test file");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test character literals
    BaaToken *token;
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CHAR_LIT, L"Expected CHAR_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"'a'");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CHAR_LIT, L"Expected CHAR_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"'ب'");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CHAR_LIT, L"Expected CHAR_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"'\\n'");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CHAR_LIT, L"Expected CHAR_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"'\\t'");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CHAR_LIT, L"Expected CHAR_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"'\\''");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_CHAR_LIT, L"Expected CHAR_LIT token");
    ASSERT_STR_EQ(token->lexeme, L"'\\\\'");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EOF, L"Expected EOF token");

    baa_free_lexer(lexer);
    free(content);
    _wremove(test_filename);
}

void test_identifiers(void)
{
    const wchar_t *test_filename = L"test_identifiers.txt";
    const wchar_t *test_content = L"variable متغير_عربي _underscore _123 123_abc";

    create_test_file(test_filename, test_content);
    wchar_t *content = baa_file_content(test_filename);
    ASSERT_NOT_NULL(content, L"Failed to read test file");

    BaaLexer *lexer = baa_create_lexer(content);
    ASSERT_NOT_NULL(lexer, L"Failed to create lexer");

    // Test identifiers
    BaaToken *token;
    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IDENTIFIER, L"Expected IDENTIFIER token");
    ASSERT_STR_EQ(token->lexeme, L"variable");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IDENTIFIER, L"Expected IDENTIFIER token");
    ASSERT_STR_EQ(token->lexeme, L"متغير_عربي");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IDENTIFIER, L"Expected IDENTIFIER token");
    ASSERT_STR_EQ(token->lexeme, L"_underscore");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_IDENTIFIER, L"Expected IDENTIFIER token");
    ASSERT_STR_EQ(token->lexeme, L"_123");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_ERROR, L"Expected ERROR token for invalid identifier");

    token = baa_lexer_next_token(lexer);
    ASSERT_EQUAL(token->type, BAA_TOKEN_EOF, L"Expected EOF token");

    baa_free_lexer(lexer);
    free(content);
    _wremove(test_filename);
}

TEST_SUITE_BEGIN()
    TEST_CASE(test_keywords);
    TEST_CASE(test_operators);
    TEST_CASE(test_string_literals);
    TEST_CASE(test_character_literals);
    TEST_CASE(test_identifiers);
TEST_SUITE_END()
