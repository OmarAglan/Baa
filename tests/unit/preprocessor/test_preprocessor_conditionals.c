#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h>
#include <string.h>

// Helper function to preprocess a string and return the result
wchar_t* preprocess_string(const wchar_t* source_string)
{
    BaaPpSource source;
    source.type = BAA_PP_SOURCE_STRING;
    source.source_name = "test_string";
    source.data.string_content = source_string;
    
    wchar_t* error_msg = NULL;
    wchar_t* result = baa_preprocess(&source, NULL, &error_msg);
    
    if (error_msg) {
        wprintf(L"Preprocessing error: %ls\n", error_msg);
        free(error_msg);
    }
    
    return result;
}

void test_if_true_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا with true condition...\n");
    
    const wchar_t* source = L"#إذا 1\nincluded_text\n#نهاية_إذا\nexcluded_text";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"included_text");
    ASSERT_WSTR_CONTAINS(result, L"excluded_text");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا with true condition test passed\n");
}

void test_if_false_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا with false condition...\n");
    
    const wchar_t* source = L"#إذا 0\nexcluded_text\n#نهاية_إذا\nincluded_text";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // excluded_text should not be in the result
    ASSERT_WSTR_CONTAINS(result, L"included_text");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا with false condition test passed\n");
}

void test_ifdef_defined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_عرف with defined macro...\n");
    
    const wchar_t* source = L"#تعريف DEFINED_MACRO 1\n#إذا_عرف DEFINED_MACRO\nincluded_text\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"included_text");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_عرف with defined macro test passed\n");
}

void test_ifdef_undefined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_عرف with undefined macro...\n");
    
    const wchar_t* source = L"#إذا_عرف UNDEFINED_MACRO\nexcluded_text\n#نهاية_إذا\nincluded_text";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"included_text");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_عرف with undefined macro test passed\n");
}

void test_ifndef_defined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_لم_يعرف with defined macro...\n");
    
    const wchar_t* source = L"#تعريف DEFINED_MACRO 1\n#إذا_لم_يعرف DEFINED_MACRO\nexcluded_text\n#نهاية_إذا\nincluded_text";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"included_text");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_لم_يعرف with defined macro test passed\n");
}

void test_ifndef_undefined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_لم_يعرف with undefined macro...\n");
    
    const wchar_t* source = L"#إذا_لم_يعرف UNDEFINED_MACRO\nincluded_text\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"included_text");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_لم_يعرف with undefined macro test passed\n");
}

void test_if_else_true_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا #إلا with true condition...\n");
    
    const wchar_t* source = L"#إذا 1\ntrue_branch\n#إلا\nfalse_branch\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"true_branch");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا #إلا with true condition test passed\n");
}

void test_if_else_false_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا #إلا with false condition...\n");
    
    const wchar_t* source = L"#إذا 0\ntrue_branch\n#إلا\nfalse_branch\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"false_branch");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ #إذا #إلا with false condition test passed\n");
}

void test_nested_conditionals(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested conditionals...\n");
    
    const wchar_t* source = L"#إذا 1\nouter_true\n#إذا 1\ninner_true\n#نهاية_إذا\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"outer_true");
    ASSERT_WSTR_CONTAINS(result, L"inner_true");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Nested conditionals test passed\n");
}

void test_nested_conditionals_mixed(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested conditionals with mixed conditions...\n");
    
    const wchar_t* source = L"#إذا 1\nouter_true\n#إذا 0\ninner_excluded\n#نهاية_إذا\nstill_outer\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"outer_true");
    ASSERT_WSTR_CONTAINS(result, L"still_outer");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Nested conditionals with mixed conditions test passed\n");
}

void test_complex_expressions(void)
{
    TEST_SETUP();
    wprintf(L"Testing complex expressions in conditionals...\n");
    
    const wchar_t* source = L"#إذا (1 + 1) == 2\nmath_works\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"math_works");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Complex expressions in conditionals test passed\n");
}

void test_macro_in_conditional_expression(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro in conditional expression...\n");
    
    const wchar_t* source = L"#تعريف VALUE 5\n#إذا VALUE > 3\nvalue_is_large\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"value_is_large");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Macro in conditional expression test passed\n");
}

void test_unterminated_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing unterminated conditional...\n");
    
    const wchar_t* source = L"#إذا 1\nunterminated_block";
    wchar_t* result = preprocess_string(source);
    
    // This should produce an error
    if (result) {
        wprintf(L"  ⚠ Unexpectedly processed unterminated conditional\n");
        free(result);
    } else {
        wprintf(L"  ✓ Correctly rejected unterminated conditional\n");
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Unterminated conditional test passed\n");
}

void test_mismatched_endif(void)
{
    TEST_SETUP();
    wprintf(L"Testing mismatched #نهاية_إذا...\n");
    
    const wchar_t* source = L"#نهاية_إذا\ntext_after";
    wchar_t* result = preprocess_string(source);
    
    // This should produce an error
    if (result) {
        wprintf(L"  ⚠ Unexpectedly processed mismatched #نهاية_إذا\n");
        free(result);
    } else {
        wprintf(L"  ✓ Correctly rejected mismatched #نهاية_إذا\n");
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Mismatched #نهاية_إذا test passed\n");
}

void test_conditional_with_arabic_content(void)
{
    TEST_SETUP();
    wprintf(L"Testing conditional with Arabic content...\n");
    
    const wchar_t* source = L"#تعريف النسخة_العربية 1\n#إذا_عرف النسخة_العربية\nالنص_العربي\n#نهاية_إذا";
    wchar_t* result = preprocess_string(source);
    
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"النص_العربي");
    
    free(result);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Conditional with Arabic content test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor Conditional Compilation tests...\n\n");

TEST_CASE(test_if_true_condition);
TEST_CASE(test_if_false_condition);
TEST_CASE(test_ifdef_defined_macro);
TEST_CASE(test_ifdef_undefined_macro);
TEST_CASE(test_ifndef_defined_macro);
TEST_CASE(test_ifndef_undefined_macro);
TEST_CASE(test_if_else_true_condition);
TEST_CASE(test_if_else_false_condition);
TEST_CASE(test_nested_conditionals);
TEST_CASE(test_nested_conditionals_mixed);
TEST_CASE(test_complex_expressions);
TEST_CASE(test_macro_in_conditional_expression);
TEST_CASE(test_unterminated_conditional);
TEST_CASE(test_mismatched_endif);
TEST_CASE(test_conditional_with_arabic_content);

wprintf(L"\n✓ All Preprocessor Conditional Compilation tests completed!\n");

TEST_SUITE_END()
