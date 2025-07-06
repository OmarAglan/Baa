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

// Test stringification operator (#)
void test_stringification_operator(void) {
    TEST_SETUP();
    wprintf(L"Testing stringification operator (#)...\n");
    
    // Test basic stringification
    const wchar_t* source1 = L"#تعريف STRINGIFY(x) #x\nSTRINGIFY(hello)";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Stringification should succeed");
    ASSERT_WSTR_CONTAINS(result1, L"\"hello\"");
    
    free(result1);
    
    // Test stringification with Arabic text
    const wchar_t* source2 = L"#تعريف STRINGIFY(x) #x\nSTRINGIFY(مرحبا)";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Arabic stringification should succeed");
    ASSERT_WSTR_CONTAINS(result2, L"\"مرحبا\"");
    
    free(result2);
    
    // Test stringification with expressions
    const wchar_t* source3 = L"#تعريف STRINGIFY(x) #x\nSTRINGIFY(1 + 2)";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Expression stringification should succeed");
    ASSERT_WSTR_CONTAINS(result3, L"\"1 + 2\"");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Stringification operator test passed\n");
}

// Test token pasting operator (##)
void test_token_pasting_operator(void) {
    TEST_SETUP();
    wprintf(L"Testing token pasting operator (##)...\n");
    
    // Test basic token pasting
    const wchar_t* source1 = L"#تعريف CONCAT(a, b) a##b\nCONCAT(hello, world)";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Token pasting should succeed");
    ASSERT_WSTR_CONTAINS(result1, L"helloworld");
    
    free(result1);
    
    // Test token pasting with numbers
    const wchar_t* source2 = L"#تعريف MAKE_VAR(prefix, num) prefix##num\nعدد_صحيح MAKE_VAR(var, 123) = 0;";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Number token pasting should succeed");
    ASSERT_WSTR_CONTAINS(result2, L"var123");
    
    free(result2);
    
    // Test token pasting with empty arguments
    const wchar_t* source3 = L"#تعريف PASTE_EMPTY(a, b) a##b\nPASTE_EMPTY(hello, )";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Empty token pasting should succeed");
    ASSERT_WSTR_CONTAINS(result3, L"hello");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Token pasting operator test passed\n");
}

// Test variadic macros
void test_variadic_macros(void) {
    TEST_SETUP();
    wprintf(L"Testing variadic macros...\n");
    
    // Test basic variadic macro
    const wchar_t* source1 = L"#تعريف LOG(format, وسائط_إضافية) printf(format, __وسائط_متغيرة__)\nLOG(\"Hello %s\", \"World\")";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Variadic macro should succeed");
    ASSERT_WSTR_CONTAINS(result1, L"printf");
    ASSERT_WSTR_CONTAINS(result1, L"\"Hello %s\"");
    ASSERT_WSTR_CONTAINS(result1, L"\"World\"");
    
    free(result1);
    
    // Test variadic macro with no additional arguments
    const wchar_t* source2 = L"#تعريف SIMPLE_LOG(وسائط_إضافية) log(__وسائط_متغيرة__)\nSIMPLE_LOG()";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Empty variadic macro should succeed");
    ASSERT_WSTR_CONTAINS(result2, L"log()");
    
    free(result2);
    
    // Test variadic macro with multiple arguments
    const wchar_t* source3 = L"#تعريف DEBUG(level, وسائط_إضافية) debug_print(level, __وسائط_متغيرة__)\nDEBUG(1, \"Error\", 42, \"test\")";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Multi-argument variadic macro should succeed");
    ASSERT_WSTR_CONTAINS(result3, L"debug_print");
    ASSERT_WSTR_CONTAINS(result3, L"1");
    ASSERT_WSTR_CONTAINS(result3, L"\"Error\"");
    ASSERT_WSTR_CONTAINS(result3, L"42");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Variadic macros test passed\n");
}

// Test complex macro rescanning
void test_complex_macro_rescanning(void) {
    TEST_SETUP();
    wprintf(L"Testing complex macro rescanning...\n");
    
    // Test multi-level macro expansion
    const wchar_t* source1 = L"#تعريف A B\n#تعريف B C\n#تعريف C final_value\nA";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Multi-level expansion should succeed");
    ASSERT_WSTR_CONTAINS(result1, L"final_value");
    
    free(result1);
    
    // Test macro expansion with token pasting and rescanning
    const wchar_t* source2 = L"#تعريف PREFIX pre\n#تعريف SUFFIX post\n#تعريف COMBINE(a,b) a##b\n#تعريف MAKE_NAME COMBINE(PREFIX, SUFFIX)\nMAKE_NAME";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Token pasting with rescanning should succeed");
    ASSERT_WSTR_CONTAINS(result2, L"prepost");
    
    free(result2);
    
    // Test function-like macro with rescanning
    const wchar_t* source3 = L"#تعريف DOUBLE(x) x x\n#تعريف VALUE test\nDOUBLE(VALUE)";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Function-like macro rescanning should succeed");
    ASSERT_WSTR_CONTAINS(result3, L"test test");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Complex macro rescanning test passed\n");
}

// Test macro recursion detection
void test_macro_recursion_detection(void) {
    TEST_SETUP();
    wprintf(L"Testing macro recursion detection...\n");
    
    // Test direct recursion
    const wchar_t* source1 = L"#تعريف RECURSIVE RECURSIVE\nRECURSIVE";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Direct recursion should be handled");
    // Should not expand infinitely - should remain as RECURSIVE
    ASSERT_WSTR_CONTAINS(result1, L"RECURSIVE");
    
    free(result1);
    
    // Test indirect recursion
    const wchar_t* source2 = L"#تعريف A B\n#تعريف B A\nA";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Indirect recursion should be handled");
    // Should detect the cycle and stop expansion
    
    free(result2);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Macro recursion detection test passed\n");
}

// Test macro argument edge cases
void test_macro_argument_edge_cases(void) {
    TEST_SETUP();
    wprintf(L"Testing macro argument edge cases...\n");
    
    // Test macro with parentheses in arguments
    const wchar_t* source1 = L"#تعريف FUNC(x) (x)\nFUNC((1 + 2))";
    wchar_t* result1 = preprocess_string(source1);
    
    ASSERT_NOT_NULL(result1, L"Parentheses in arguments should work");
    ASSERT_WSTR_CONTAINS(result1, L"((1 + 2))");
    
    free(result1);
    
    // Test macro with comma in arguments (should be protected by parentheses)
    const wchar_t* source2 = L"#تعريف PAIR(x) x\nPAIR((a, b))";
    wchar_t* result2 = preprocess_string(source2);
    
    ASSERT_NOT_NULL(result2, L"Comma in parentheses should work");
    ASSERT_WSTR_CONTAINS(result2, L"(a, b)");
    
    free(result2);
    
    // Test empty macro arguments
    const wchar_t* source3 = L"#تعريف OPTIONAL(x, y) x y\nOPTIONAL(hello, )";
    wchar_t* result3 = preprocess_string(source3);
    
    ASSERT_NOT_NULL(result3, L"Empty arguments should work");
    ASSERT_WSTR_CONTAINS(result3, L"hello");
    
    free(result3);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Macro argument edge cases test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Advanced Preprocessor Macro tests...\n\n");

TEST_CASE(test_stringification_operator);
TEST_CASE(test_token_pasting_operator);
TEST_CASE(test_variadic_macros);
TEST_CASE(test_complex_macro_rescanning);
TEST_CASE(test_macro_recursion_detection);
TEST_CASE(test_macro_argument_edge_cases);

wprintf(L"\n✓ All Advanced Preprocessor Macro tests completed!\n");

TEST_SUITE_END()
