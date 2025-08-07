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

void test_pragma_once_basic(void)
{
    TEST_SETUP();
    wprintf(L"Testing basic #براغما مرة_واحدة directive...\n");

    const wchar_t *source = L"#براغما مرة_واحدة\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process the content normally
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #براغما مرة_واحدة directive");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Basic #براغما مرة_واحدة directive test passed\n");
}

void test_pragma_unknown_ignored(void)
{
    TEST_SETUP();
    wprintf(L"Testing unknown #براغما directive is ignored...\n");

    const wchar_t *source = L"#براغما unknown_pragma_name some_args\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and ignore the unknown pragma
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with unknown #براغما directive");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Unknown #براغما directive ignored test passed\n");
}

void test_pragma_empty(void)
{
    TEST_SETUP();
    wprintf(L"Testing empty #براغما directive...\n");

    const wchar_t *source = L"#براغما\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and ignore the empty pragma
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with empty #براغما directive");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Empty #براغما directive test passed\n");
}

void test_pragma_in_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing #براغما directive in conditional compilation...\n");

    const wchar_t *source = L"#إذا 1\n#براغما مرة_واحدة\ntest_content\n#نهاية_إذا";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process the pragma in true conditional
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #براغما in conditional");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #براغما directive in conditional test passed\n");
}

void test_pragma_skipped_in_false_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing #براغما directive skipped in false conditional...\n");

    const wchar_t *source = L"#إذا 0\n#براغما مرة_واحدة\n#نهاية_إذا\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and skip the pragma in false conditional
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #براغما in false conditional");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #براغما directive skipped in false conditional test passed\n");
}

void test_pragma_with_whitespace(void)
{
    TEST_SETUP();
    wprintf(L"Testing #براغما directive with various whitespace...\n");

    const wchar_t *sources[] = {
        L"#براغما  مرة_واحدة\ntest_content",      // Extra spaces
        L"#براغما\tمرة_واحدة\ntest_content",     // Tab
        L"#براغما مرة_واحدة  \ntest_content",    // Trailing spaces
        L"  #براغما مرة_واحدة\ntest_content",    // Leading spaces on line
    };

    size_t num_sources = sizeof(sources) / sizeof(sources[0]);

    for (size_t i = 0; i < num_sources; i++)
    {
        wchar_t *error_msg = NULL;
        wchar_t *result = preprocess_string_with_error(sources[i], &error_msg);

        ASSERT_NOT_NULL(result, L"Preprocessing should succeed with whitespace variations");
        ASSERT_WSTR_CONTAINS(result, L"test_content");

        if (error_msg)
        {
            wprintf(L"  Unexpected error in case %zu: %ls\n", i, error_msg);
            free(error_msg);
        }

        free(result);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ #براغما directive with whitespace test passed\n");
}

void test_pragma_case_sensitivity(void)
{
    TEST_SETUP();
    wprintf(L"Testing #براغما directive case sensitivity...\n");

    // The pragma name should be case-sensitive
    const wchar_t *source = L"#براغما مَرَّة_وَاحِدَة\ntest_content"; // Different diacritics
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed but treat as unknown pragma (not pragma once)
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with different case");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #براغما directive case sensitivity test passed\n");
}

void test_pragma_multiple_unknown(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiple unknown #براغما directives...\n");

    const wchar_t *source = L"#براغما first_unknown\n#براغما second_unknown arg1 arg2\n#براغما third\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and ignore all unknown pragmas
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with multiple unknown pragmas");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Multiple unknown #براغما directives test passed\n");
}

void test_pragma_mixed_with_other_directives(void)
{
    TEST_SETUP();
    wprintf(L"Testing #براغما directive mixed with other directives...\n");

    const wchar_t *source = L"#تعريف MACRO 123\n#براغما مرة_واحدة\n#إذا 1\nMACRO\n#نهاية_إذا";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process all directives correctly
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with mixed directives");
    ASSERT_WSTR_CONTAINS(result, L"123");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #براغما directive mixed with other directives test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor #براغما Directive tests...\n\n");

TEST_CASE(test_pragma_once_basic);
TEST_CASE(test_pragma_unknown_ignored);
TEST_CASE(test_pragma_empty);
TEST_CASE(test_pragma_in_conditional);
TEST_CASE(test_pragma_skipped_in_false_conditional);
TEST_CASE(test_pragma_with_whitespace);
TEST_CASE(test_pragma_case_sensitivity);
TEST_CASE(test_pragma_multiple_unknown);
TEST_CASE(test_pragma_mixed_with_other_directives);

wprintf(L"\n✓ All Preprocessor #براغما Directive tests completed!\n");

TEST_SUITE_END()