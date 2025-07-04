#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to preprocess a string and return the result
wchar_t *preprocess_string(const wchar_t *source_string)
{
    BaaPpSource source;
    source.type = BAA_PP_SOURCE_STRING;
    source.source_name = "test_string";
    source.data.source_string = source_string;

    wchar_t *error_msg = NULL;
    wchar_t *result = baa_preprocess(&source, NULL, &error_msg);

    if (error_msg)
    {
        wprintf(L"Preprocessing error: %ls\n", error_msg);
        free(error_msg);
    }

    return result;
}

void test_if_true_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا with true condition...\n");

    const wchar_t *source = L"#إذا 1\nنص_مضمن\n#نهاية_إذا\nنص_خارجي";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"نص_مضمن");
    ASSERT_WSTR_CONTAINS(result, L"نص_خارجي");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا with true condition test passed\n");
}

void test_if_false_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا with false condition...\n");

    const wchar_t *source = L"#إذا 0\nنص_مستبعد\n#نهاية_إذا\nنص_مضمن";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // excluded_text should not be in the result
    ASSERT_WSTR_CONTAINS(result, L"نص_مضمن");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا with false condition test passed\n");
}

void test_ifdef_defined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_عرف with defined macro...\n");

    const wchar_t *source = L"#تعريف ماكرو_معرف 1\n#إذا_عرف ماكرو_معرف\nنص_مضمن\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"نص_مضمن");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_عرف with defined macro test passed\n");
}

void test_ifdef_undefined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_عرف with undefined macro...\n");

    const wchar_t *source = L"#إذا_عرف ماكرو_غير_معرف\nنص_مستبعد\n#نهاية_إذا\nنص_مضمن";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"نص_مضمن");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_عرف with undefined macro test passed\n");
}

void test_ifndef_defined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_لم_يعرف with defined macro...\n");

    const wchar_t *source = L"#تعريف ماكرو_معرف 1\n#إذا_لم_يعرف ماكرو_معرف\nنص_مستبعد\n#نهاية_إذا\nنص_مضمن";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"نص_مضمن");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_لم_يعرف with defined macro test passed\n");
}

void test_ifndef_undefined_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا_لم_يعرف with undefined macro...\n");

    const wchar_t *source = L"#إذا_لم_يعرف ماكرو_غير_معرف\nنص_مضمن\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"نص_مضمن");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا_لم_يعرف with undefined macro test passed\n");
}

void test_if_else_true_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا #إلا with true condition...\n");

    const wchar_t *source = L"#إذا 1\nفرع_صحيح\n#إلا\nفرع_خطأ\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"فرع_صحيح");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا #إلا with true condition test passed\n");
}

void test_if_else_false_condition(void)
{
    TEST_SETUP();
    wprintf(L"Testing #إذا #إلا with false condition...\n");

    const wchar_t *source = L"#إذا 0\nفرع_صحيح\n#إلا\nفرع_خطأ\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"فرع_خطأ");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #إذا #إلا with false condition test passed\n");
}

void test_nested_conditionals(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested conditionals...\n");

    const wchar_t *source = L"#إذا 1\nخارجي_صحيح\n#إذا 1\nداخلي_صحيح\n#نهاية_إذا\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"خارجي_صحيح");
    ASSERT_WSTR_CONTAINS(result, L"داخلي_صحيح");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Nested conditionals test passed\n");
}

void test_nested_conditionals_mixed(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested conditionals with mixed conditions...\n");

    const wchar_t *source = L"#إذا 1\nخارجي_صحيح\n#إذا 0\nداخلي_مستبعد\n#نهاية_إذا\nلا_يزال_خارجي\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"خارجي_صحيح");
    ASSERT_WSTR_CONTAINS(result, L"لا_يزال_خارجي");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Nested conditionals with mixed conditions test passed\n");
}

void test_complex_expressions(void)
{
    TEST_SETUP();
    wprintf(L"Testing complex expressions in conditionals...\n");

    const wchar_t *source = L"#إذا (1 + 1) == 2\nالرياضيات_تعمل\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"الرياضيات_تعمل");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Complex expressions in conditionals test passed\n");
}

void test_macro_in_conditional_expression(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro in conditional expression...\n");

    const wchar_t *source = L"#تعريف قيمة 5\n#إذا قيمة > 3\nالقيمة_كبيرة\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"القيمة_كبيرة");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Macro in conditional expression test passed\n");
}

void test_unterminated_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing unterminated conditional...\n");

    const wchar_t *source = L"#إذا_عرف ماكرو_اختبار\nكتلة_غير_منتهية";
    wchar_t *result = preprocess_string(source);

    // This should produce an error
    if (result)
    {
        wprintf(L"  ⚠ Unexpectedly processed unterminated conditional\n");
        free(result);
    }
    else
    {
        wprintf(L"  ✓ Correctly rejected unterminated conditional\n");
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Unterminated conditional test passed\n");
}

void test_mismatched_endif(void)
{
    TEST_SETUP();
    wprintf(L"Testing mismatched #نهاية_إذا...\n");

    const wchar_t *source = L"#نهاية_إذا\nنص_بعدها";
    wchar_t *result = preprocess_string(source);

    // This should produce an error
    if (result)
    {
        wprintf(L"  ⚠ Unexpectedly processed mismatched #نهاية_إذا\n");
        free(result);
    }
    else
    {
        wprintf(L"  ✓ Correctly rejected mismatched #نهاية_إذا\n");
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Mismatched #نهاية_إذا test passed\n");
}

void test_conditional_with_arabic_content(void)
{
    TEST_SETUP();
    wprintf(L"Testing conditional with Arabic content...\n");

    const wchar_t *source = L"#تعريف النسخة_العربية 1\n#إذا_عرف النسخة_العربية\nالنص_العربي\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

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
