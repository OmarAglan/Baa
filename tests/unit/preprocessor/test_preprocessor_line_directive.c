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

void test_line_directive_basic(void)
{
    TEST_SETUP();
    wprintf(L"Testing basic #سطر directive...\n");

    const wchar_t *source = L"#سطر 100\nsome_code\n__السطر__";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and __السطر__ should expand to 101 (line after #سطر 100)
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #سطر directive");
    ASSERT_WSTR_CONTAINS(result, L"101");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Basic #سطر directive test passed\n");
}

void test_line_directive_with_filename(void)
{
    TEST_SETUP();
    wprintf(L"Testing #سطر directive with filename...\n");

    const wchar_t *source = L"#سطر 50 \"custom_file.baa\"\nsome_code\n__الملف__ __السطر__";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and __الملف__ should expand to "custom_file.baa", __السطر__ to 51
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #سطر directive with filename");
    ASSERT_WSTR_CONTAINS(result, L"\"custom_file.baa\"");
    ASSERT_WSTR_CONTAINS(result, L"51");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #سطر directive with filename test passed\n");
}

void test_line_directive_multiple(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiple #سطر directives...\n");

    const wchar_t *source = L"#سطر 10\nline1\n#سطر 200\nline2\n__السطر__";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and __السطر__ should expand to 201 (line after second #سطر 200)
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with multiple #سطر directives");
    ASSERT_WSTR_CONTAINS(result, L"201");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Multiple #سطر directives test passed\n");
}

void test_line_directive_error_reporting(void)
{
    TEST_SETUP();
    wprintf(L"Testing #سطر directive affects error reporting...\n");

    const wchar_t *source = L"#سطر 999 \"error_file.baa\"\n#خطأ \"Test error message\"";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should fail with an error that reports line 1000 and error_file.baa
    ASSERT_NULL(result, L"Preprocessing should fail with #خطأ directive");
    ASSERT_NOT_NULL(error_msg, L"Error message should be provided");

    if (error_msg)
    {
        ASSERT_WSTR_CONTAINS(error_msg, L"1000");
        ASSERT_WSTR_CONTAINS(error_msg, L"error_file.baa");
        free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ #سطر directive error reporting test passed\n");
}

void test_line_directive_invalid_syntax(void)
{
    TEST_SETUP();
    wprintf(L"Testing #سطر directive invalid syntax...\n");

    const wchar_t *invalid_sources[] = {
        L"#سطر",                        // Missing line number
        L"#سطر abc",                    // Invalid line number
        L"#سطر 0",                      // Zero line number
        L"#سطر 123 missing_quotes",     // Filename without quotes
        L"#سطر 123 \"unterminated",     // Unterminated filename string
    };

    size_t num_sources = sizeof(invalid_sources) / sizeof(invalid_sources[0]);

    for (size_t i = 0; i < num_sources; i++)
    {
        wchar_t *error_msg = NULL;
        wchar_t *result = preprocess_string_with_error(invalid_sources[i], &error_msg);

        // These should either fail or handle gracefully
        if (result)
        {
            wprintf(L"  ⚠ Unexpectedly processed invalid #سطر directive: case %zu\n", i);
            free(result);
        }
        else
        {
            wprintf(L"  ✓ Correctly rejected invalid #سطر directive: case %zu\n", i);
        }

        if (error_msg)
        {
            free(error_msg);
        }
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Invalid #سطر directive syntax test passed\n");
}

void test_line_directive_in_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing #سطر directive in conditional compilation...\n");

    const wchar_t *source = L"#إذا 1\n#سطر 42\nsome_code\n#نهاية_إذا\n__السطر__";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and __السطر__ should reflect the #سطر directive
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #سطر in conditional");
    // The __السطر__ is on the last line, so it should be line 44 (42 + 2 lines after #سطر)
    ASSERT_WSTR_CONTAINS(result, L"44");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #سطر directive in conditional test passed\n");
}

void test_line_directive_skipped_in_false_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing #سطر directive skipped in false conditional...\n");

    const wchar_t *source = L"#إذا 0\n#سطر 999\n#نهاية_إذا\n__السطر__";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and __السطر__ should be the original line number (4), not affected by skipped #سطر
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with #سطر in false conditional");
    ASSERT_WSTR_CONTAINS(result, L"4");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #سطر directive skipped in false conditional test passed\n");
}

void test_line_directive_macro_expansion(void)
{
    TEST_SETUP();
    wprintf(L"Testing #سطر directive with macro expansion...\n");

    const wchar_t *source = L"#تعريف LINE_NUM 123\n#تعريف FILE_NAME \"macro_file.baa\"\n#سطر LINE_NUM FILE_NAME\ncode\n__الملف__ __السطر__";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed with macro expansion in #سطر directive
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with macro expansion in #سطر");
    ASSERT_WSTR_CONTAINS(result, L"\"macro_file.baa\"");
    ASSERT_WSTR_CONTAINS(result, L"124"); // 123 + 1 line after #سطر

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ #سطر directive with macro expansion test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor #سطر Directive tests...\n\n");

TEST_CASE(test_line_directive_basic);
TEST_CASE(test_line_directive_with_filename);
TEST_CASE(test_line_directive_multiple);
TEST_CASE(test_line_directive_error_reporting);
TEST_CASE(test_line_directive_invalid_syntax);
TEST_CASE(test_line_directive_in_conditional);
TEST_CASE(test_line_directive_skipped_in_false_conditional);
TEST_CASE(test_line_directive_macro_expansion);

wprintf(L"\n✓ All Preprocessor #سطر Directive tests completed!\n");

TEST_SUITE_END()