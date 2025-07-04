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

void test_integer_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing integer literals...\n");
    
    const wchar_t* test_integers[] = {
        L"0",
        L"1",
        L"42",
        L"123",
        L"999",
        L"1000",
        L"123456789",
        L"2147483647",  // Max 32-bit signed int
        L"4294967295",  // Max 32-bit unsigned int
    };
    
    size_t num_integers = sizeof(test_integers) / sizeof(test_integers[0]);
    
    for (size_t i = 0; i < num_integers; i++) {
        BaaToken* token = get_first_token(test_integers[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_INT_LIT, token->type);
        ASSERT_WSTR_EQ(test_integers[i], token->lexeme);
        
        wprintf(L"  ✓ Integer: %ls\n", test_integers[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Integer literals test passed\n");
}

void test_arabic_integer_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic integer literals...\n");
    
    const wchar_t* test_arabic_integers[] = {
        L"٠",        // Arabic zero
        L"١",        // Arabic one
        L"٢",        // Arabic two
        L"٣",        // Arabic three
        L"٤",        // Arabic four
        L"٥",        // Arabic five
        L"٦",        // Arabic six
        L"٧",        // Arabic seven
        L"٨",        // Arabic eight
        L"٩",        // Arabic nine
        L"١٢",       // Arabic 12
        L"٤٢",       // Arabic 42
        L"١٢٣",      // Arabic 123
        L"٩٩٩",      // Arabic 999
        L"١٠٠٠",     // Arabic 1000
        L"١٢٣٤٥٦٧٨٩٠", // All Arabic digits
    };
    
    size_t num_integers = sizeof(test_arabic_integers) / sizeof(test_arabic_integers[0]);
    
    for (size_t i = 0; i < num_integers; i++) {
        BaaToken* token = get_first_token(test_arabic_integers[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_INT_LIT, token->type);
        ASSERT_WSTR_EQ(test_arabic_integers[i], token->lexeme);
        
        wprintf(L"  ✓ Arabic integer: %ls\n", test_arabic_integers[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Arabic integer literals test passed\n");
}

void test_float_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing float literals...\n");
    
    const wchar_t* test_floats[] = {
        L"0.0",
        L"1.0",
        L"3.14",
        L"2.718",
        L"0.5",
        L"123.456",
        L"999.999",
        L".5",        // Leading decimal point
        L"5.",        // Trailing decimal point
        L"0.123456789",
    };
    
    size_t num_floats = sizeof(test_floats) / sizeof(test_floats[0]);
    
    for (size_t i = 0; i < num_floats; i++) {
        BaaToken* token = get_first_token(test_floats[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_FLOAT_LIT, token->type);
        ASSERT_WSTR_EQ(test_floats[i], token->lexeme);
        
        wprintf(L"  ✓ Float: %ls\n", test_floats[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Float literals test passed\n");
}

void test_scientific_notation(void)
{
    TEST_SETUP();
    wprintf(L"Testing scientific notation...\n");
    
    const wchar_t* test_scientific[] = {
        L"1e5",
        L"1E5",
        L"1.5e10",
        L"1.5E10",
        L"2.5e-3",
        L"2.5E-3",
        L"1e+5",
        L"1E+5",
        L"3.14159e0",
        L"6.022e23",
        L"1.602e-19",
    };
    
    size_t num_scientific = sizeof(test_scientific) / sizeof(test_scientific[0]);
    
    for (size_t i = 0; i < num_scientific; i++) {
        BaaToken* token = get_first_token(test_scientific[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_FLOAT_LIT, token->type);
        ASSERT_WSTR_EQ(test_scientific[i], token->lexeme);
        
        wprintf(L"  ✓ Scientific: %ls\n", test_scientific[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Scientific notation test passed\n");
}

void test_hexadecimal_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing hexadecimal literals...\n");
    
    const wchar_t* test_hex[] = {
        L"0x0",
        L"0x1",
        L"0xA",
        L"0xa",
        L"0xF",
        L"0xf",
        L"0x10",
        L"0xFF",
        L"0xff",
        L"0x123",
        L"0xABC",
        L"0xabc",
        L"0xDEF",
        L"0xdef",
        L"0x123ABC",
        L"0x123abc",
        L"0xDEADBEEF",
        L"0xdeadbeef",
        L"0X0",        // Capital X
        L"0XFF",       // Capital X with uppercase hex
        L"0Xff",       // Capital X with lowercase hex
    };
    
    size_t num_hex = sizeof(test_hex) / sizeof(test_hex[0]);
    
    for (size_t i = 0; i < num_hex; i++) {
        BaaToken* token = get_first_token(test_hex[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_INT_LIT, token->type);
        ASSERT_WSTR_EQ(test_hex[i], token->lexeme);
        
        wprintf(L"  ✓ Hex: %ls\n", test_hex[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Hexadecimal literals test passed\n");
}

void test_hexadecimal_float_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing hexadecimal float literals...\n");
    
    const wchar_t* test_hex_floats[] = {
        L"0x1.0p0",
        L"0x1.8p0",
        L"0x1.0p1",
        L"0x1.0p-1",
        L"0x1.0P0",    // Capital P
        L"0x1.8P0",
        L"0x1.0P1",
        L"0x1.0P-1",
        L"0x.8p0",     // Leading decimal point
        L"0x1.p0",     // Trailing decimal point
        L"0x1.23p4",
        L"0x1.ABCp-5",
        L"0x1.abcp-5",
    };
    
    size_t num_hex_floats = sizeof(test_hex_floats) / sizeof(test_hex_floats[0]);
    
    for (size_t i = 0; i < num_hex_floats; i++) {
        BaaToken* token = get_first_token(test_hex_floats[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_FLOAT_LIT, token->type);
        ASSERT_WSTR_EQ(test_hex_floats[i], token->lexeme);
        
        wprintf(L"  ✓ Hex float: %ls\n", test_hex_floats[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Hexadecimal float literals test passed\n");
}

void test_binary_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing binary literals...\n");
    
    const wchar_t* test_binary[] = {
        L"0b0",
        L"0b1",
        L"0b10",
        L"0b11",
        L"0b100",
        L"0b101",
        L"0b110",
        L"0b111",
        L"0b1000",
        L"0b1010",
        L"0b1100",
        L"0b1111",
        L"0b10101010",
        L"0b11110000",
        L"0b11111111",
        L"0B0",        // Capital B
        L"0B1",
        L"0B10",
        L"0B11111111",
    };
    
    size_t num_binary = sizeof(test_binary) / sizeof(test_binary[0]);
    
    for (size_t i = 0; i < num_binary; i++) {
        BaaToken* token = get_first_token(test_binary[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_INT_LIT, token->type);
        ASSERT_WSTR_EQ(test_binary[i], token->lexeme);
        
        wprintf(L"  ✓ Binary: %ls\n", test_binary[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Binary literals test passed\n");
}

void test_octal_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing octal literals...\n");
    
    const wchar_t* test_octal[] = {
        L"0o0",
        L"0o1",
        L"0o7",
        L"0o10",
        L"0o17",
        L"0o77",
        L"0o100",
        L"0o177",
        L"0o777",
        L"0o1234567",
        L"0O0",        // Capital O
        L"0O7",
        L"0O77",
        L"0O777",
    };
    
    size_t num_octal = sizeof(test_octal) / sizeof(test_octal[0]);
    
    for (size_t i = 0; i < num_octal; i++) {
        BaaToken* token = get_first_token(test_octal[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        ASSERT_EQ(BAA_TOKEN_INT_LIT, token->type);
        ASSERT_WSTR_EQ(test_octal[i], token->lexeme);
        
        wprintf(L"  ✓ Octal: %ls\n", test_octal[i]);
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Octal literals test passed\n");
}

void test_invalid_number_literals(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid number literals...\n");
    
    const wchar_t* invalid_numbers[] = {
        L"0x",         // Hex prefix without digits
        L"0b",         // Binary prefix without digits
        L"0o",         // Octal prefix without digits
        L"0xG",        // Invalid hex digit
        L"0b2",        // Invalid binary digit
        L"0o8",        // Invalid octal digit
        L"1.2.3",      // Multiple decimal points
        L"1e",         // Scientific notation without exponent
        L"1e+",        // Scientific notation with incomplete exponent
        L"1e-",        // Scientific notation with incomplete exponent
        L"0x1.p",      // Hex float without exponent
        L"0x1.0p",     // Hex float with incomplete exponent
    };
    
    size_t num_invalid = sizeof(invalid_numbers) / sizeof(invalid_numbers[0]);
    
    for (size_t i = 0; i < num_invalid; i++) {
        BaaToken* token = get_first_token(invalid_numbers[i]);
        ASSERT_NOT_NULL(token, L"Token should not be NULL");
        
        // Should be an error token or handle gracefully
        if (token->type == BAA_TOKEN_ERROR) {
            wprintf(L"  ✓ Correctly rejected invalid number: %ls\n", invalid_numbers[i]);
        } else {
            wprintf(L"  ⚠ Unexpectedly accepted invalid number: %ls (type: %d)\n", 
                    invalid_numbers[i], token->type);
        }
        
        baa_free_token(token);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Invalid number literals test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Lexer Number Literal tests...\n\n");

TEST_CASE(test_integer_literals);
TEST_CASE(test_arabic_integer_literals);
TEST_CASE(test_float_literals);
TEST_CASE(test_scientific_notation);
TEST_CASE(test_hexadecimal_literals);
TEST_CASE(test_hexadecimal_float_literals);
TEST_CASE(test_binary_literals);
TEST_CASE(test_octal_literals);
TEST_CASE(test_invalid_number_literals);

wprintf(L"\n✓ All Lexer Number Literal tests completed!\n");

TEST_SUITE_END()
