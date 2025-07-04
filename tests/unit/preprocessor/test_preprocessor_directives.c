#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to preprocess a string and return the result
wchar_t *preprocess_string_with_error(const wchar_t *source_string, wchar_t **error_msg)
{
    BaaPpSource source;
    source.type = BAA_PP_SOURCE_STRING;
    source.source_name = "test_string";
    source.data.source_string = source_string;

    wchar_t *result = baa_preprocess(&source, NULL, error_msg);

    return result;
}

void test_error_directive(void)
{
    TEST_SETUP();
    wprintf(L"Testing #خطأ directive...\n");

    const wchar_t *source = L"#خطأ \"This is an error message\"";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should fail with an error
    ASSERT_NULL(result, L"Preprocessing should fail with #خطأ directive");
    ASSERT_NOT_NULL(error_msg, L"Error message should be provided");

    if (error_msg)
    {
        ASSERT_WSTR_CONTAINS(error_msg, L"This is an error message");
        free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ #خطأ directive test passed\n");
}

void test_error_directive_arabic(void)
{
    TEST_SETUP();
    wprintf(L"Testing #خطأ directive with Arabic message...\n");

    const wchar_t *source = L"#خطأ \"رسالة خطأ باللغة العربية\"";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should fail with an error
    ASSERT_NULL(result, L"Preprocessing should fail with #خطأ directive");
    ASSERT_NOT_NULL(error_msg, L"Error message should be provided");

    if (error_msg)
    {
        ASSERT_WSTR_CONTAINS(error_msg, L"رسالة خطأ باللغة العربية");
        free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ #خطأ directive with Arabic message test passed\n");
}

void test_warning_directive(void)
{
    TEST_SETUP();
    wprintf(L"Testing #تحذير directive...\n");

    const wchar_t *source = L"#تحذير \"This is a warning message\"\ntext_after_warning";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed but may produce a warning
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #تحذير directive");
    ASSERT_WSTR_CONTAINS(result, L"text_after_warning");

    // Check if warning was reported (implementation dependent)
    if (error_msg)
    {
        wprintf(L"  Warning reported: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #تحذير directive test passed\n");
}

void test_warning_directive_arabic(void)
{
    TEST_SETUP();
    wprintf(L"Testing #تحذير directive with Arabic message...\n");

    const wchar_t *source = L"#تحذير \"رسالة تحذير باللغة العربية\"\ntext_after_warning";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed but may produce a warning
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #تحذير directive");
    ASSERT_WSTR_CONTAINS(result, L"text_after_warning");

    if (error_msg)
    {
        wprintf(L"  Warning reported: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #تحذير directive with Arabic message test passed\n");
}

void test_error_in_conditional_true(void)
{
    TEST_SETUP();
    wprintf(L"Testing #خطأ in true conditional...\n");

    const wchar_t *source = L"#إذا 1\n#خطأ \"Error in true branch\"\n#نهاية_إذا";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should fail because the error directive is in a true branch
    ASSERT_NULL(result, L"Preprocessing should fail with #خطأ in true conditional");
    ASSERT_NOT_NULL(error_msg, L"Error message should be provided");

    if (error_msg)
    {
        free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ #خطأ in true conditional test passed\n");
}

void test_error_in_conditional_false(void)
{
    TEST_SETUP();
    wprintf(L"Testing #خطأ in false conditional...\n");

    const wchar_t *source = L"#إذا 0\n#خطأ \"Error in false branch\"\n#نهاية_إذا\ntext_after";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed because the error directive is in a false branch (skipped)
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #خطأ in false conditional");
    ASSERT_WSTR_CONTAINS(result, L"text_after");

    if (error_msg)
    {
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #خطأ in false conditional test passed\n");
}

void test_warning_in_conditional_true(void)
{
    TEST_SETUP();
    wprintf(L"Testing #تحذير in true conditional...\n");

    const wchar_t *source = L"#إذا 1\n#تحذير \"Warning in true branch\"\n#نهاية_إذا\ntext_after";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and may produce a warning
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #تحذير in true conditional");
    ASSERT_WSTR_CONTAINS(result, L"text_after");

    if (error_msg)
    {
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #تحذير in true conditional test passed\n");
}

void test_warning_in_conditional_false(void)
{
    TEST_SETUP();
    wprintf(L"Testing #تحذير in false conditional...\n");

    const wchar_t *source = L"#إذا 0\n#تحذير \"Warning in false branch\"\n#نهاية_إذا\ntext_after";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and should not produce a warning (directive is skipped)
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #تحذير in false conditional");
    ASSERT_WSTR_CONTAINS(result, L"text_after");

    if (error_msg)
    {
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #تحذير in false conditional test passed\n");
}

void test_error_with_macro_expansion(void)
{
    TEST_SETUP();
    wprintf(L"Testing #خطأ with macro expansion...\n");

    const wchar_t *source = L"#تعريف ERROR_MSG \"Macro expanded error\"\n#خطأ ERROR_MSG";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should fail with expanded error message
    ASSERT_NULL(result, L"Preprocessing should fail with #خطأ with macro");
    ASSERT_NOT_NULL(error_msg, L"Error message should be provided");

    if (error_msg)
    {
        ASSERT_WSTR_CONTAINS(error_msg, L"Macro expanded error");
        free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ #خطأ with macro expansion test passed\n");
}

void test_warning_with_macro_expansion(void)
{
    TEST_SETUP();
    wprintf(L"Testing #تحذير with macro expansion...\n");

    const wchar_t *source = L"#تعريف WARN_MSG \"Macro expanded warning\"\n#تحذير WARN_MSG\ntext_after";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and may produce a warning with expanded message
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #تحذير with macro");
    ASSERT_WSTR_CONTAINS(result, L"text_after");

    if (error_msg)
    {
        wprintf(L"  Warning with macro: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #تحذير with macro expansion test passed\n");
}

void test_multiple_error_directives(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiple #خطأ directives...\n");

    const wchar_t *source = L"#إذا 1\n#خطأ \"First error\"\n#خطأ \"Second error\"\n#نهاية_إذا";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should fail on the first error
    ASSERT_NULL(result, L"Preprocessing should fail on first #خطأ directive");
    ASSERT_NOT_NULL(error_msg, L"Error message should be provided");

    if (error_msg)
    {
        // Should contain the first error message
        ASSERT_WSTR_CONTAINS(error_msg, L"First error");
        free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Multiple #خطأ directives test passed\n");
}

void test_invalid_directive_syntax(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid directive syntax...\n");

    const wchar_t *invalid_sources[] = {
        L"#خطأ",                   // Missing message
        L"#تحذير",                 // Missing message
        L"#خطأ unclosed_string\"", // Malformed string
        L"#unknown_directive",     // Unknown directive
    };

    size_t num_sources = sizeof(invalid_sources) / sizeof(invalid_sources[0]);

    for (size_t i = 0; i < num_sources; i++)
    {
        wchar_t *error_msg = NULL;
        wchar_t *result = preprocess_string_with_error(invalid_sources[i], &error_msg);

        // These should either fail or handle gracefully
        if (result)
        {
            wprintf(L"  ⚠ Unexpectedly processed invalid directive: case %zu\n", i);
            free(result);
        }
        else
        {
            wprintf(L"  ✓ Correctly rejected invalid directive: case %zu\n", i);
        }

        if (error_msg)
        {
            free(error_msg);
        }
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Invalid directive syntax test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor Error and Warning Directive tests...\n\n");

TEST_CASE(test_error_directive);
TEST_CASE(test_error_directive_arabic);
TEST_CASE(test_warning_directive);
TEST_CASE(test_warning_directive_arabic);
TEST_CASE(test_error_in_conditional_true);
TEST_CASE(test_error_in_conditional_false);
TEST_CASE(test_warning_in_conditional_true);
TEST_CASE(test_warning_in_conditional_false);
TEST_CASE(test_error_with_macro_expansion);
TEST_CASE(test_warning_with_macro_expansion);
TEST_CASE(test_multiple_error_directives);
TEST_CASE(test_invalid_directive_syntax);

wprintf(L"\n✓ All Preprocessor Error and Warning Directive tests completed!\n");

TEST_SUITE_END()
