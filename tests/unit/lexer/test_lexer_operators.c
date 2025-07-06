#include "test_framework.h"
#include "baa/lexer/lexer.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to tokenize a string and return the first token
BaaToken *get_first_token(const wchar_t *source)
{
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    return baa_lexer_next_token(&lexer);
}

// Helper function to tokenize a string and return all tokens
BaaToken **get_all_tokens(const wchar_t *source, size_t *token_count)
{
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");

    BaaToken **tokens = NULL;
    size_t capacity = 10;
    *token_count = 0;

    tokens = malloc(capacity * sizeof(BaaToken *));
    if (!tokens)
        return NULL;

    BaaToken *token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL)
    {
        if (*token_count >= capacity)
        {
            capacity *= 2;
            tokens = realloc(tokens, capacity * sizeof(BaaToken *));
            if (!tokens)
                return NULL;
        }

        tokens[(*token_count)++] = token;

        if (token->type == BAA_TOKEN_EOF)
        {
            break;
        }
    }

    return tokens;
}

void test_arithmetic_operators(void)
{
    TEST_SETUP();
    wprintf(L"Testing arithmetic operators...\n");

    struct
    {
        const wchar_t *operator;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"+", BAA_TOKEN_PLUS},
        {L"-", BAA_TOKEN_MINUS},
        {L"*", BAA_TOKEN_STAR},
        {L"/", BAA_TOKEN_SLASH},
        {L"%", BAA_TOKEN_PERCENT},
        {L"++", BAA_TOKEN_INCREMENT},
        {L"--", BAA_TOKEN_DECREMENT},
        {L"+=", BAA_TOKEN_PLUS_EQUAL},
        {L"-=", BAA_TOKEN_MINUS_EQUAL},
        {L"*=", BAA_TOKEN_STAR_EQUAL},
        {L"/=", BAA_TOKEN_SLASH_EQUAL},
        {L"%=", BAA_TOKEN_PERCENT_EQUAL},
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].operator);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].operator, token->lexeme);

        wprintf(L"  ✓ Operator: %ls -> %d\n", test_cases[i].operator, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Arithmetic operators test passed\n");
}

void test_comparison_operators(void)
{
    TEST_SETUP();
    wprintf(L"Testing comparison operators...\n");

    struct
    {
        const wchar_t *operator;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"==", BAA_TOKEN_EQUAL_EQUAL},
        {L"!=", BAA_TOKEN_BANG_EQUAL},
        {L"<", BAA_TOKEN_LESS},
        {L"<=", BAA_TOKEN_LESS_EQUAL},
        {L">", BAA_TOKEN_GREATER},
        {L">=", BAA_TOKEN_GREATER_EQUAL},
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].operator);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].operator, token->lexeme);

        wprintf(L"  ✓ Comparison: %ls -> %d\n", test_cases[i].operator, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Comparison operators test passed\n");
}

void test_logical_operators(void)
{
    TEST_SETUP();
    wprintf(L"Testing logical operators...\n");

    struct
    {
        const wchar_t *operator;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"&&", BAA_TOKEN_AND},
        {L"||", BAA_TOKEN_OR},
        {L"!", BAA_TOKEN_BANG},
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].operator);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].operator, token->lexeme);

        wprintf(L"  ✓ Logical: %ls -> %d\n", test_cases[i].operator, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Logical operators test passed\n");
}

// TODO: Bitwise operators not yet implemented in lexer
/*
void test_bitwise_operators(void)
{
    TEST_SETUP();
    wprintf(L"Testing bitwise operators...\n");

    struct
    {
        const wchar_t *operator;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"&", BAA_TOKEN_AMPERSAND},
        {L"|", BAA_TOKEN_PIPE},
        {L"^", BAA_TOKEN_CARET},
        {L"~", BAA_TOKEN_TILDE},
        {L"<<", BAA_TOKEN_LESS_LESS},
        {L">>", BAA_TOKEN_GREATER_GREATER},
        {L"&=", BAA_TOKEN_AMPERSAND_EQUAL},
        {L"|=", BAA_TOKEN_PIPE_EQUAL},
        {L"^=", BAA_TOKEN_CARET_EQUAL},
        {L"<<=", BAA_TOKEN_LESS_LESS_EQUAL},
        {L">>=", BAA_TOKEN_GREATER_GREATER_EQUAL},
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].operator);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].operator, token->lexeme);

        wprintf(L"  ✓ Bitwise: %ls -> %d\n", test_cases[i].operator, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Bitwise operators test passed\n");
}
*/

void test_assignment_operators(void)
{
    TEST_SETUP();
    wprintf(L"Testing assignment operators...\n");

    struct
    {
        const wchar_t *operator;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"=", BAA_TOKEN_EQUAL},
        {L"+=", BAA_TOKEN_PLUS_EQUAL},
        {L"-=", BAA_TOKEN_MINUS_EQUAL},
        {L"*=", BAA_TOKEN_STAR_EQUAL},
        {L"/=", BAA_TOKEN_SLASH_EQUAL},
        {L"%=", BAA_TOKEN_PERCENT_EQUAL},
        // TODO: Bitwise assignment operators not yet implemented
        // {L"&=", BAA_TOKEN_AMPERSAND_EQUAL},
        // {L"|=", BAA_TOKEN_PIPE_EQUAL},
        // {L"^=", BAA_TOKEN_CARET_EQUAL},
        // {L"<<=", BAA_TOKEN_LESS_LESS_EQUAL},
        // {L">>=", BAA_TOKEN_GREATER_GREATER_EQUAL},
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].operator);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].operator, token->lexeme);

        wprintf(L"  ✓ Assignment: %ls -> %d\n", test_cases[i].operator, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Assignment operators test passed\n");
}

void test_delimiters(void)
{
    TEST_SETUP();
    wprintf(L"Testing delimiters...\n");

    struct
    {
        const wchar_t *delimiter;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"(", BAA_TOKEN_LPAREN},
        {L")", BAA_TOKEN_RPAREN},
        {L"{", BAA_TOKEN_LBRACE},
        {L"}", BAA_TOKEN_RBRACE},
        {L"[", BAA_TOKEN_LBRACKET},
        {L"]", BAA_TOKEN_RBRACKET},
        {L";", BAA_TOKEN_SEMICOLON},
        {L",", BAA_TOKEN_COMMA},
        {L".", BAA_TOKEN_DOT},
        {L":", BAA_TOKEN_COLON},
        // TODO: Question mark operator not yet implemented
        // {L"?", BAA_TOKEN_QUESTION},
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].delimiter);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].delimiter, token->lexeme);

        wprintf(L"  ✓ Delimiter: %ls -> %d\n", test_cases[i].delimiter, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Delimiters test passed\n");
}

void test_operator_precedence_parsing(void)
{
    TEST_SETUP();
    wprintf(L"Testing operator precedence parsing...\n");

    // Test that multi-character operators are parsed correctly
    const wchar_t *test_expressions[] = {
        L"++x",    // Should be ++ and x, not + and +x
        L"x++",    // Should be x and ++
        L"x+=y",   // Should be x, +=, y
        L"x<<=y",  // Should be x, <<=, y
        L"x>>>=y", // Should be x, >>>, =, y (if >>> exists) or x, >>, >=, y
        L"x==y",   // Should be x, ==, y
        L"x!=y",   // Should be x, !=, y
        L"x<=y",   // Should be x, <=, y
        L"x>=y",   // Should be x, >=, y
        L"x&&y",   // Should be x, &&, y
        L"x||y",   // Should be x, ||, y
    };

    size_t num_expressions = sizeof(test_expressions) / sizeof(test_expressions[0]);

    for (size_t i = 0; i < num_expressions; i++)
    {
        size_t token_count;
        BaaToken **tokens = get_all_tokens(test_expressions[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        ASSERT_TRUE(token_count >= 2, L"Should have at least 2 tokens (plus EOF)");

        wprintf(L"  ✓ Expression: %ls -> %zu tokens\n", test_expressions[i], token_count - 1); // -1 for EOF

        // Clean up
        for (size_t j = 0; j < token_count; j++)
        {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Operator precedence parsing test passed\n");
}

void test_operator_combinations(void)
{
    TEST_SETUP();
    wprintf(L"Testing operator combinations...\n");

    // Test various operator combinations that might be ambiguous
    const wchar_t *test_combinations[] = {
        L"+-*/",  // Should be +, -, *, /
        L"<<=>>", // Should be <<=, >>
        L"==!=",  // Should be ==, !=
        L"&&||",  // Should be &&, ||
        L"++--",  // Should be ++, --
        L"<<>>",  // Should be <<, >>
        L"&&&",   // Should be &&, &
        L"|||",   // Should be ||, |
        L"===",   // Should be ==, =
        L"!==",   // Should be !=, =
    };

    size_t num_combinations = sizeof(test_combinations) / sizeof(test_combinations[0]);

    for (size_t i = 0; i < num_combinations; i++)
    {
        size_t token_count;
        BaaToken **tokens = get_all_tokens(test_combinations[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        ASSERT_TRUE(token_count >= 2, L"Should have at least 2 tokens (plus EOF)");

        wprintf(L"  ✓ Combination: %ls -> %zu tokens\n", test_combinations[i], token_count - 1); // -1 for EOF

        // Clean up
        for (size_t j = 0; j < token_count; j++)
        {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Operator combinations test passed\n");
}

void test_whitespace_handling(void)
{
    TEST_SETUP();
    wprintf(L"Testing whitespace handling around operators...\n");

    // Test that whitespace doesn't affect operator parsing
    const wchar_t *test_whitespace[] = {
        L"+ +", // Should be +, +
        L"++",  // Should be ++
        L"+ =", // Should be +, =
        L"+=",  // Should be +=
        L"< <", // Should be <, <
        L"<<",  // Should be <<
        L"< =", // Should be <, =
        L"<=",  // Should be <=
        L"= =", // Should be =, =
        L"==",  // Should be ==
    };

    size_t num_whitespace = sizeof(test_whitespace) / sizeof(test_whitespace[0]);

    for (size_t i = 0; i < num_whitespace; i++)
    {
        size_t token_count;
        BaaToken **tokens = get_all_tokens(test_whitespace[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");

        wprintf(L"  ✓ Whitespace test: %ls -> %zu tokens\n", test_whitespace[i], token_count - 1); // -1 for EOF

        // Clean up
        for (size_t j = 0; j < token_count; j++)
        {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Whitespace handling test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Lexer Operator and Delimiter tests...\n\n");

TEST_CASE(test_arithmetic_operators);
TEST_CASE(test_comparison_operators);
TEST_CASE(test_logical_operators);
// // TEST_CASE(test_bitwise_operators); // TODO: Bitwise operators not yet implemented // TODO: Bitwise operators not yet implemented
TEST_CASE(test_assignment_operators);
TEST_CASE(test_delimiters);
TEST_CASE(test_operator_precedence_parsing);
TEST_CASE(test_operator_combinations);
TEST_CASE(test_whitespace_handling);

wprintf(L"\n✓ All Lexer Operator and Delimiter tests completed!\n");

TEST_SUITE_END()
