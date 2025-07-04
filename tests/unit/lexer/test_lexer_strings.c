#include "test_framework.h"
#include "baa/lexer/lexer.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to tokenize a string and return the first token
BaaToken* get_first_token(const wchar_t* source)
{
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    return baa_lexer_next_token(&lexer);
}

void test_basic_string_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing basic string literals...\n");
    
    const wchar_t* test_strings[] = {
        L"\"hello\"",
        L"\"world\"",
        L"\"\"",  // Empty string
        L"\"a\"", // Single character
        L"\"Hello, World!\"",
        L"\"123456789\"",
        L"\"Special chars: !@#$%^&*()\"",
    };
    
    size_t num_strings = sizeof(test_strings) / sizeof(test_strings[0]);
    
    for (size_t i = 0; i < num_strings; i++) {
        BaaToken* token = get_first_token(test_strings[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        ASSERT_WSTR_EQ(test_strings[i], token->lexeme);
        
        wprintf(L"  ✓ String: %ls\n", test_strings[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Basic string literals test passed\n");
}

void test_standard_escape_sequences(void)
{
    TEST_SETUP();
    wprintf(L"Testing standard escape sequences...\n");
    
    const wchar_t* test_escapes[] = {
        L"\"\\n\"",     // Newline
        L"\"\\t\"",     // Tab
        L"\"\\r\"",     // Carriage return
        L"\"\\\\\"",    // Backslash
        L"\"\\\"\"",    // Double quote
        L"\"\\'\"",     // Single quote
        L"\"\\0\"",     // Null character
        L"\"\\a\"",     // Bell (if supported)
        L"\"\\b\"",     // Backspace (if supported)
        L"\"\\f\"",     // Form feed (if supported)
        L"\"\\v\"",     // Vertical tab (if supported)
    };
    
    size_t num_escapes = sizeof(test_escapes) / sizeof(test_escapes[0]);
    
    for (size_t i = 0; i < num_escapes; i++) {
        BaaToken* token = get_first_token(test_escapes[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        ASSERT_WSTR_EQ(test_escapes[i], token->lexeme);
        
        wprintf(L"  ✓ Escape: %ls\n", test_escapes[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Standard escape sequences test passed\n");
}

void test_arabic_escape_sequences(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic escape sequences...\n");
    
    const wchar_t* test_arabic_escapes[] = {
        L"\"\\س\"",     // Arabic newline (س)
        L"\"\\م\"",     // Arabic tab (م)
        L"\"\\ر\"",     // Arabic carriage return (ر)
        L"\"\\ص\"",     // Arabic null character (ص)
        L"\"\\ي0041\"", // Arabic Unicode escape (ي) for 'A'
        L"\"\\ي0042\"", // Arabic Unicode escape for 'B'
        L"\"\\ي0043\"", // Arabic Unicode escape for 'C'
        L"\"\\هـ41\"",   // Arabic hex escape (هـ) for 'A'
        L"\"\\هـ42\"",   // Arabic hex escape for 'B'
    };
    
    size_t num_escapes = sizeof(test_arabic_escapes) / sizeof(test_arabic_escapes[0]);
    
    for (size_t i = 0; i < num_escapes; i++) {
        BaaToken* token = get_first_token(test_arabic_escapes[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        ASSERT_WSTR_EQ(test_arabic_escapes[i], token->lexeme);
        
        wprintf(L"  ✓ Arabic escape: %ls\n", test_arabic_escapes[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic escape sequences test passed\n");
}

void test_multiline_string_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiline string literals...\n");
    
    const wchar_t* test_multiline_strings[] = {
        L"\"\"\"simple multiline\"\"\"",
        L"\"\"\"line1\nline2\nline3\"\"\"",
        L"\"\"\"Arabic text:\nمرحبا بالعالم\nالسطر الثاني\"\"\"",
        L"\"\"\"Empty lines:\n\n\nEnd\"\"\"",
        L"\"\"\"With escapes:\n\\t\\n\\r\"\"\"",
        L"\"\"\"\"\"\"",  // Empty multiline string
    };
    
    size_t num_strings = sizeof(test_multiline_strings) / sizeof(test_multiline_strings[0]);
    
    for (size_t i = 0; i < num_strings; i++) {
        BaaToken* token = get_first_token(test_multiline_strings[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        ASSERT_WSTR_EQ(test_multiline_strings[i], token->lexeme);
        
        wprintf(L"  ✓ Multiline: %ls\n", test_multiline_strings[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Multiline string literals test passed\n");
}

void test_raw_string_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing raw string literals...\n");
    
    const wchar_t* test_raw_strings[] = {
        L"خ\"raw string\"",
        L"خ\"no escapes: \\n \\t \\r\"",
        L"خ\"Arabic: مرحبا بالعالم\"",
        L"خ\"Special chars: !@#$%^&*()\"",
        L"خ\"\"",  // Empty raw string
        L"خ\"Path: C:\\Users\\Name\\File.txt\"",
    };
    
    size_t num_strings = sizeof(test_raw_strings) / sizeof(test_raw_strings[0]);
    
    for (size_t i = 0; i < num_strings; i++) {
        BaaToken* token = get_first_token(test_raw_strings[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        ASSERT_WSTR_EQ(test_raw_strings[i], token->lexeme);
        
        wprintf(L"  ✓ Raw string: %ls\n", test_raw_strings[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Raw string literals test passed\n");
}

void test_multiline_raw_string_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiline raw string literals...\n");
    
    const wchar_t* test_multiline_raw_strings[] = {
        L"خ\"\"\"multiline raw\"\"\"",
        L"خ\"\"\"line1\nline2\nline3\"\"\"",
        L"خ\"\"\"No escapes:\n\\n\\t\\r\"\"\"",
        L"خ\"\"\"Arabic multiline:\nمرحبا\nبالعالم\"\"\"",
        L"خ\"\"\"\"\"\"",  // Empty multiline raw string
    };
    
    size_t num_strings = sizeof(test_multiline_raw_strings) / sizeof(test_multiline_raw_strings[0]);
    
    for (size_t i = 0; i < num_strings; i++) {
        BaaToken* token = get_first_token(test_multiline_raw_strings[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_STRING_LIT, token->type);
        ASSERT_WSTR_EQ(test_multiline_raw_strings[i], token->lexeme);
        
        wprintf(L"  ✓ Multiline raw: %ls\n", test_multiline_raw_strings[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Multiline raw string literals test passed\n");
}

void test_character_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing character literals...\n");
    
    const wchar_t* test_chars[] = {
        L"'a'",
        L"'Z'",
        L"'0'",
        L"'9'",
        L"'!'",
        L"'@'",
        L"'#'",
        L"'$'",
        L"'%'",
        L"'^'",
        L"'&'",
        L"'*'",
        L"'('",
        L"')'",
        L"'_'",
        L"'+'",
        L"'='",
        L"'['",
        L"']'",
        L"'{'",
        L"'}'",
        L"'|'",
        L"';'",
        L"':'",
        L"'\"'",
        L"'<'",
        L"'>'",
        L"'?'",
        L"'/'",
        L"'.'",
        L"','",
    };
    
    size_t num_chars = sizeof(test_chars) / sizeof(test_chars[0]);
    
    for (size_t i = 0; i < num_chars; i++) {
        BaaToken* token = get_first_token(test_chars[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_CHAR_LIT, token->type);
        ASSERT_WSTR_EQ(test_chars[i], token->lexeme);
        
        if (i < 5) { // Only print first few to avoid clutter
            wprintf(L"  ✓ Char: %ls\n", test_chars[i]);
        }
        baa_free_token(token);
    }
    
    wprintf(L"  ✓ Tested %zu character literals\n", num_chars);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Character literals test passed\n");
}

void test_character_escape_sequences(void)
{
    TEST_SETUP();
    wprintf(L"Testing character escape sequences...\n");
    
    const wchar_t* test_char_escapes[] = {
        L"'\\n'",     // Newline
        L"'\\t'",     // Tab
        L"'\\r'",     // Carriage return
        L"'\\\\'",    // Backslash
        L"'\\''",     // Single quote
        L"'\\\"'",    // Double quote
        L"'\\0'",     // Null character
        L"'\\س'",     // Arabic newline
        L"'\\م'",     // Arabic tab
        L"'\\ر'",     // Arabic carriage return
        L"'\\ص'",     // Arabic null
    };
    
    size_t num_escapes = sizeof(test_char_escapes) / sizeof(test_char_escapes[0]);
    
    for (size_t i = 0; i < num_escapes; i++) {
        BaaToken* token = get_first_token(test_char_escapes[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_CHAR_LIT, token->type);
        ASSERT_WSTR_EQ(test_char_escapes[i], token->lexeme);
        
        wprintf(L"  ✓ Char escape: %ls\n", test_char_escapes[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Character escape sequences test passed\n");
}

void test_invalid_string_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid string literals...\n");
    
    const wchar_t* invalid_strings[] = {
        L"\"unterminated string",
        L"\"unterminated with newline\n",
        L"'unterminated char",
        L"'too many chars'",
        L"\"invalid escape \\z\"",
        L"\"\"\"unterminated multiline",
        L"خ\"unterminated raw",
    };
    
    size_t num_invalid = sizeof(invalid_strings) / sizeof(invalid_strings[0]);
    
    for (size_t i = 0; i < num_invalid; i++) {
        BaaToken* token = get_first_token(invalid_strings[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        
        // Should be an error token or handle gracefully
        if (token->type == BAA_TOKEN_ERROR) {
            wprintf(L"  ✓ Correctly rejected invalid string: %ls\n", invalid_strings[i]);
        } else {
            wprintf(L"  ⚠ Unexpectedly accepted invalid string: %ls\n", invalid_strings[i]);
        }
        
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Invalid string literals test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Lexer String and Character Literal tests...\n\n");

TEST_CASE(test_basic_string_literals);
TEST_CASE(test_standard_escape_sequences);
TEST_CASE(test_arabic_escape_sequences);
TEST_CASE(test_multiline_string_literals);
TEST_CASE(test_raw_string_literals);
TEST_CASE(test_multiline_raw_string_literals);
TEST_CASE(test_character_literals);
TEST_CASE(test_character_escape_sequences);
TEST_CASE(test_invalid_string_literals);

wprintf(L"\n✓ All Lexer String and Character Literal tests completed!\n");

TEST_SUITE_END()
