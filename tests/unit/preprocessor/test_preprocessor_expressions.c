#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Helper function to preprocess a string
static wchar_t* preprocess_string(const wchar_t* source) {
    BaaPpSource pp_source;
    pp_source.type = BAA_PP_SOURCE_STRING;
    pp_source.source_name = "test_string";
    pp_source.data.source_string = source;
    
    wchar_t* error_message = NULL;
    wchar_t* result = baa_preprocess(&pp_source, NULL, &error_message);
    
    if (error_message) {
        wprintf(L"Preprocessing error: %ls\n", error_message);
        free(error_message);
    }
    
    return result;
}

// Test ternary operator in conditional expressions
void test_ternary_operator(void) {
    TEST_SETUP();
    wprintf(L"Testing ternary operator in conditional expressions...\n");
    
    // Test basic ternary operator
    const wchar_t* source1 = L"#تعريف FLAG 1\n#إذا FLAG ? 1 : 0\nTernary true branch\n#إلا\nTernary false branch\n#نهاية_إذا";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Ternary operator should work");
    ASSERT_WSTR_CONTAINS(result1, L"Ternary true branch");
    
    free(result1);
    
    // Test ternary operator with false condition
    const wchar_t* source2 = L"#تعريف FLAG 0\n#إذا FLAG ? 1 : 0\nTernary true branch\n#إلا\nTernary false branch\n#نهاية_إذا";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Ternary operator with false condition should work");
    ASSERT_WSTR_CONTAINS(result2, L"Ternary false branch");
    
    free(result2);
    
    // Test nested ternary operators
    const wchar_t* source3 = L"#تعريف LEVEL 2\n#إذا LEVEL == 1 ? 10 : LEVEL == 2 ? 20 : 30\nLevel 2 selected\n#إلا\nOther level\n#نهاية_إذا";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Nested ternary operators should work");
    ASSERT_WSTR_CONTAINS(result3, L"Level 2 selected");
    
    free(result3);
    
    // Test ternary operator with complex expressions
    const wchar_t* source4 = L"#تعريف BASE 10\n#إذا BASE > 5 ? BASE * 2 : BASE / 2\nGreater than 5\n#إلا\nLess than or equal to 5\n#نهاية_إذا";
    wchar_t* result4 = preprocess_string(source4);
    
    ASSERT_NOT_NULL(result4, L"Ternary operator with complex expressions should work");
    ASSERT_WSTR_CONTAINS(result4, L"Greater than 5");
    
    free(result4);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Ternary operator test passed\n");
}

// Test operator precedence in expressions
void test_operator_precedence(void) {
    TEST_SETUP();
    wprintf(L"Testing operator precedence in expressions...\n");
    
    // Test arithmetic precedence: multiplication before addition
    const wchar_t* source1 = L"#إذا 2 + 3 * 4 == 14\nCorrect precedence\n#إلا\nIncorrect precedence\n#نهاية_إذا";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Arithmetic precedence should work");
    ASSERT_WSTR_CONTAINS(result1, L"Correct precedence");
    
    free(result1);
    
    // Test logical precedence: && before ||
    const wchar_t* source2 = L"#تعريف A 1\n#تعريف B 0\n#تعريف C 1\n#إذا A || B && C\nLogical precedence correct\n#إلا\nLogical precedence incorrect\n#نهاية_إذا";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Logical precedence should work");
    ASSERT_WSTR_CONTAINS(result2, L"Logical precedence correct");
    
    free(result2);
    
    // Test bitwise precedence
    const wchar_t* source3 = L"#إذا 5 | 3 & 1 == 5\nBitwise precedence correct\n#إلا\nBitwise precedence incorrect\n#نهاية_إذا";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Bitwise precedence should work");
    ASSERT_WSTR_CONTAINS(result3, L"Bitwise precedence correct");
    
    free(result3);
    
    // Test ternary precedence (lowest)
    const wchar_t* source4 = L"#تعريف FLAG1 1\n#تعريف FLAG2 0\n#إذا FLAG1 && FLAG2 ? 100 : FLAG1 || FLAG2 ? 200 : 300\nTernary precedence test\n#نهاية_إذا";
    wchar_t* result4 = preprocess_string(source4);
    
    ASSERT_NOT_NULL(result4, L"Ternary precedence should work");
    ASSERT_WSTR_CONTAINS(result4, L"Ternary precedence test");
    
    free(result4);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Operator precedence test passed\n");
}

// Test معرف() operator in complex expressions
void test_defined_operator_complex(void) {
    TEST_SETUP();
    wprintf(L"Testing معرف() operator in complex expressions...\n");
    
    // Test معرف() with logical operators
    const wchar_t* source1 = L"#تعريف FEATURE_A 1\n#إذا معرف(FEATURE_A) && !معرف(FEATURE_B)\nFeature A defined, B not defined\n#إلا\nOther case\n#نهاية_إذا";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"معرف() with logical operators should work");
    ASSERT_WSTR_CONTAINS(result1, L"Feature A defined, B not defined");
    
    free(result1);
    
    // Test معرف() in ternary expressions
    const wchar_t* source2 = L"#تعريف OPTIONAL_FEATURE some_value\n#إذا معرف(OPTIONAL_FEATURE) ? 1 : 0\nOptional feature enabled\n#إلا\nOptional feature disabled\n#نهاية_إذا";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"معرف() in ternary expressions should work");
    ASSERT_WSTR_CONTAINS(result2, L"Optional feature enabled");
    
    free(result2);
    
    // Test معرف() with parentheses and whitespace
    const wchar_t* source3 = L"#تعريف SPACED_MACRO\n#إذا معرف(   SPACED_MACRO   )\nSpaced macro defined\n#إلا\nSpaced macro not defined\n#نهاية_إذا";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"معرف() with spaces should work");
    ASSERT_WSTR_CONTAINS(result3, L"Spaced macro defined");
    
    free(result3);
    
    // Test that معرف() doesn't expand its argument
    const wchar_t* source4 = L"#تعريف ACTUAL_MACRO real_value\n#تعريف POINTER_MACRO ACTUAL_MACRO\n#إذا معرف(POINTER_MACRO)\nPointer macro is defined\n#إلا\nPointer macro not defined\n#نهاية_إذا";
    wchar_t* result4 = preprocess_string(source4);
    
    ASSERT_NOT_NULL(result4, L"معرف() should not expand its argument");
    ASSERT_WSTR_CONTAINS(result4, L"Pointer macro is defined");
    
    free(result4);
    
    TEST_TEARDOWN();
    wprintf(L"✓ معرف() operator complex test passed\n");
}

// Test bitwise operations in expressions
void test_bitwise_operations(void) {
    TEST_SETUP();
    wprintf(L"Testing bitwise operations in expressions...\n");
    
    // Test basic bitwise operations
    const wchar_t* source1 = L"#تعريف FLAGS 0x05\n#إذا (FLAGS & 0x01) && (FLAGS | 0x02) == 0x07\nBitwise AND and OR work\n#إلا\nBitwise operations failed\n#نهاية_إذا";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Bitwise AND and OR should work");
    ASSERT_WSTR_CONTAINS(result1, L"Bitwise AND and OR work");
    
    free(result1);
    
    // Test XOR and NOT operations
    const wchar_t* source2 = L"#تعريف VAL 0x05\n#إذا (VAL ^ 0x04) == 0x01 && (~VAL & 0x0F) == 0x0A\nBitwise XOR and NOT work\n#إلا\nBitwise XOR/NOT failed\n#نهاية_إذا";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Bitwise XOR and NOT should work");
    ASSERT_WSTR_CONTAINS(result2, L"Bitwise XOR and NOT work");
    
    free(result2);
    
    // Test shift operations
    const wchar_t* source3 = L"#تعريف BASE 0x05\n#إذا (BASE << 1) == 0x0A && (BASE >> 1) == 0x02\nBitwise shifts work\n#إلا\nBitwise shifts failed\n#نهاية_إذا";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Bitwise shifts should work");
    ASSERT_WSTR_CONTAINS(result3, L"Bitwise shifts work");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Bitwise operations test passed\n");
}

// Test function-like macros in conditional expressions
void test_function_macros_in_conditionals(void) {
    TEST_SETUP();
    wprintf(L"Testing function-like macros in conditional expressions...\n");
    
    // Test simple function macro in conditional
    const wchar_t* source1 = L"#تعريف IS_GREATER(a,b) ((a) > (b))\n#تعريف MAX_SIZE 100\n#تعريف CURRENT_SIZE 50\n#إذا IS_GREATER(MAX_SIZE, CURRENT_SIZE)\nMax is greater\n#إلا\nCurrent is greater or equal\n#نهاية_إذا";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Function macro in conditional should work");
    ASSERT_WSTR_CONTAINS(result1, L"Max is greater");
    
    free(result1);
    
    // Test nested function macros
    const wchar_t* source2 = L"#تعريف ADD(a,b) ((a) + (b))\n#تعريف MULT(a,b) ((a) * (b))\n#تعريف CALC(x,y,z) ADD(MULT(x, y), z)\n#إذا CALC(5, 3, 2) == 17\nNested calculation correct\n#إلا\nNested calculation incorrect\n#نهاية_إذا";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Nested function macros should work");
    ASSERT_WSTR_CONTAINS(result2, L"Nested calculation correct");
    
    free(result2);
    
    // Test function macro with rescanning
    const wchar_t* source3 = L"#تعريف GET_BASE() BASE_VALUE\n#تعريف BASE_VALUE 42\n#تعريف IS_EQUAL(x,y) ((x) == (y))\n#إذا IS_EQUAL(GET_BASE(), 42)\nRescanning in function macro works\n#إلا\nRescanning failed\n#نهاية_إذا";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Function macro with rescanning should work");
    ASSERT_WSTR_CONTAINS(result3, L"Rescanning in function macro works");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Function macros in conditionals test passed\n");
}

// Test number formats in expressions
void test_number_formats_in_expressions(void) {
    TEST_SETUP();
    wprintf(L"Testing different number formats in expressions...\n");
    
    // Test hexadecimal numbers
    const wchar_t* source1 = L"#تعريف HEX_VAL 0x10\n#إذا HEX_VAL == 16\nHexadecimal works\n#إلا\nHexadecimal failed\n#نهاية_إذا";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Hexadecimal numbers should work");
    ASSERT_WSTR_CONTAINS(result1, L"Hexadecimal works");
    
    free(result1);
    
    // Test binary numbers
    const wchar_t* source2 = L"#تعريف BIN_VAL 0b101\n#إذا BIN_VAL == 5\nBinary works\n#إلا\nBinary failed\n#نهاية_إذا";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Binary numbers should work");
    ASSERT_WSTR_CONTAINS(result2, L"Binary works");
    
    free(result2);
    
    // Test octal numbers
    const wchar_t* source3 = L"#تعريف OCT_VAL 010\n#إذا OCT_VAL == 8\nOctal works\n#إلا\nOctal failed\n#نهاية_إذا";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Octal numbers should work");
    ASSERT_WSTR_CONTAINS(result3, L"Octal works");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Number formats in expressions test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Advanced Preprocessor Expression tests...\n\n");

TEST_CASE(test_ternary_operator);
TEST_CASE(test_operator_precedence);
TEST_CASE(test_defined_operator_complex);
TEST_CASE(test_bitwise_operations);
TEST_CASE(test_function_macros_in_conditionals);
TEST_CASE(test_number_formats_in_expressions);

wprintf(L"\n✓ All Advanced Preprocessor Expression tests completed!\n");

TEST_SUITE_END()
