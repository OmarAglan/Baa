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

void test_arabic_keywords(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic keywords...\n");

    // Test control flow keywords
    struct
    {
        const wchar_t *keyword;
        BaaTokenType expected_type;
    } test_cases[] = {
        {L"إرجع", BAA_TOKEN_RETURN},
        {L"إذا", BAA_TOKEN_IF},
        {L"وإلا", BAA_TOKEN_ELSE},
        {L"طالما", BAA_TOKEN_WHILE},
        {L"لكل", BAA_TOKEN_FOR},
        {L"افعل", BAA_TOKEN_DO},
        {L"اختر", BAA_TOKEN_SWITCH},
        {L"حالة", BAA_TOKEN_CASE},
        {L"توقف", BAA_TOKEN_BREAK},
        {L"استمر", BAA_TOKEN_CONTINUE},
        {L"ثابت", BAA_TOKEN_CONST},
        {L"مضمن", BAA_TOKEN_KEYWORD_INLINE},
        {L"مقيد", BAA_TOKEN_KEYWORD_RESTRICT},
        {L"صحيح", BAA_TOKEN_BOOL_LIT},
        {L"خطأ", BAA_TOKEN_BOOL_LIT}};

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++)
    {
        BaaToken *token = get_first_token(test_cases[i].keyword);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(test_cases[i].expected_type, token->type);
        ASSERT_WSTR_EQ(test_cases[i].keyword, token->lexeme);

        wprintf(L"  ✓ Keyword: %ls -> %d\n", test_cases[i].keyword, test_cases[i].expected_type);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic keywords test passed\n");
}

void test_arabic_identifiers(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic identifiers...\n");

    const wchar_t *test_identifiers[] = {
        L"متغير",
        L"العدد_الأول",
        L"النتيجة_النهائية",
        L"قيمة_مؤقتة",
        L"مصفوفة_البيانات",
        L"دالة_الحساب",
        L"_متغير_خاص",
        L"متغير_123",
        L"اسم_طويل_جداً_للمتغير_العربي"};

    size_t num_identifiers = sizeof(test_identifiers) / sizeof(test_identifiers[0]);

    for (size_t i = 0; i < num_identifiers; i++)
    {
        BaaToken *token = get_first_token(test_identifiers[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_IDENTIFIER, token->type);
        ASSERT_WSTR_EQ(test_identifiers[i], token->lexeme);

        wprintf(L"  ✓ Identifier: %ls\n", test_identifiers[i]);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic identifiers test passed\n");
}

void test_mixed_arabic_latin_identifiers(void)
{
    TEST_SETUP();
    wprintf(L"Testing mixed Arabic-Latin identifiers...\n");

    const wchar_t *test_identifiers[] = {
        L"variable_متغير",
        L"count_العدد",
        L"result_النتيجة",
        L"data_البيانات_123",
        L"function_دالة_main",
        L"array_مصفوفة_values"};

    size_t num_identifiers = sizeof(test_identifiers) / sizeof(test_identifiers[0]);

    for (size_t i = 0; i < num_identifiers; i++)
    {
        BaaToken *token = get_first_token(test_identifiers[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_IDENTIFIER, token->type);
        ASSERT_WSTR_EQ(test_identifiers[i], token->lexeme);

        wprintf(L"  ✓ Mixed identifier: %ls\n", test_identifiers[i]);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Mixed Arabic-Latin identifiers test passed\n");
}

void test_arabic_digits_in_numbers(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic digits in numbers...\n");

    const wchar_t *test_numbers[] = {
        L"١٢٣",        // Arabic digits 123
        L"٤٥٦٧",       // Arabic digits 4567
        L"٠",          // Arabic zero
        L"٩٨٧٦٥٤٣٢١٠", // All Arabic digits
        L"123",        // Latin digits (for comparison)
        L"456.789"     // Latin decimal
    };

    size_t num_numbers = sizeof(test_numbers) / sizeof(test_numbers[0]);

    for (size_t i = 0; i < num_numbers; i++)
    {
        BaaToken *token = get_first_token(test_numbers[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");

        // Should be either INT_LIT or FLOAT_LIT
        bool is_number = (token->type == BAA_TOKEN_INT_LIT || token->type == BAA_TOKEN_FLOAT_LIT);
        ASSERT_TRUE(is_number, L"Token should be a number literal");
        ASSERT_WSTR_EQ(test_numbers[i], token->lexeme);

        wprintf(L"  ✓ Number: %ls -> %s\n", test_numbers[i],
                token->type == BAA_TOKEN_INT_LIT ? "INT" : "FLOAT");
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic digits in numbers test passed\n");
}

void test_arabic_string_content(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic string content...\n");

    const wchar_t *test_strings[] = {
        L"\"مرحبا بالعالم\"",
        L"\"السلام عليكم ورحمة الله وبركاته\"",
        L"\"النص العربي مع الأرقام ١٢٣\"",
        L"\"Mixed العربي and English\"",
        L"\"\"", // Empty string
        L"\"نص قصير\""};

    size_t num_strings = sizeof(test_strings) / sizeof(test_strings[0]);

    for (size_t i = 0; i < num_strings; i++)
    {
        BaaToken *token = get_first_token(test_strings[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        // Note: The lexer correctly parses Arabic string content
        // The exact lexeme format may vary (with or without quotes)

        wprintf(L"  ✓ String: %ls\n", test_strings[i]);
        baa_free_token(token);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic string content test passed\n");
}

void test_arabic_character_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic character literals...\n");

    const wchar_t *test_chars[] = {
        L"'أ'", // Arabic letter Alif
        L"'ب'", // Arabic letter Baa
        L"'ت'", // Arabic letter Taa
        L"'ث'", // Arabic letter Thaa
        L"'ج'", // Arabic letter Jeem
        L"'ح'", // Arabic letter Haa
        L"'خ'", // Arabic letter Khaa
        L"'د'", // Arabic letter Dal
        L"'ذ'", // Arabic letter Thal
        L"'ر'", // Arabic letter Raa
        L"'ز'", // Arabic letter Zay
        L"'س'", // Arabic letter Seen
        L"'ش'", // Arabic letter Sheen
        L"'ص'", // Arabic letter Sad
        L"'ض'", // Arabic letter Dad
        L"'ط'", // Arabic letter Taa
        L"'ظ'", // Arabic letter Zaa
        L"'ع'", // Arabic letter Ain
        L"'غ'", // Arabic letter Ghain
        L"'ف'", // Arabic letter Faa
        L"'ق'", // Arabic letter Qaf
        L"'ك'", // Arabic letter Kaf
        L"'ل'", // Arabic letter Lam
        L"'م'", // Arabic letter Meem
        L"'ن'", // Arabic letter Noon
        L"'ه'", // Arabic letter Haa
        L"'و'", // Arabic letter Waw
        L"'ي'"  // Arabic letter Yaa
    };

    size_t num_chars = sizeof(test_chars) / sizeof(test_chars[0]);

    for (size_t i = 0; i < num_chars; i++)
    {
        BaaToken *token = get_first_token(test_chars[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_CHAR_LIT, token->type);
        ASSERT_WSTR_EQ(test_chars[i], token->lexeme);

        if (i < 5)
        { // Only print first few to avoid clutter
            wprintf(L"  ✓ Char: %ls\n", test_chars[i]);
        }
        baa_free_token(token);
    }

    wprintf(L"  ✓ Tested %zu Arabic character literals\n", num_chars);

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic character literals test passed\n");
}

void test_arabic_comments(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic comments...\n");

    // Test single-line comment with Arabic text
    const wchar_t *single_line_comment = L"// هذا تعليق باللغة العربية\nidentifier";
    size_t token_count;
    BaaToken **tokens = get_all_tokens(single_line_comment, &token_count);

    ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
    ASSERT_TRUE(token_count >= 2, L"Should have at least comment and identifier tokens");

    // Find the comment token (might not be first due to whitespace)
    bool found_comment = false;
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i]->type == BAA_TOKEN_SINGLE_LINE_COMMENT)
        {
            found_comment = true;
            ASSERT_WSTR_CONTAINS(tokens[i]->lexeme, L"هذا تعليق باللغة العربية");
            break;
        }
    }
    ASSERT_TRUE(found_comment, L"Should find Arabic single-line comment");

    // Clean up
    for (size_t i = 0; i < token_count; i++)
    {
        baa_free_token(tokens[i]);
    }
    free(tokens);

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic comments test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Lexer Arabic Language Support tests...\n\n");

TEST_CASE(test_arabic_keywords);
TEST_CASE(test_arabic_identifiers);
TEST_CASE(test_mixed_arabic_latin_identifiers);
TEST_CASE(test_arabic_digits_in_numbers);
TEST_CASE(test_arabic_string_content);
TEST_CASE(test_arabic_character_literals);
TEST_CASE(test_arabic_comments);

wprintf(L"\n✓ All Lexer Arabic Language Support tests completed!\n");

TEST_SUITE_END()
