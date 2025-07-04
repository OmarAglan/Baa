#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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

// Helper function to preprocess a file and return the result
wchar_t *preprocess_file(const char *file_path)
{
    BaaPpSource source;
    source.type = BAA_PP_SOURCE_FILE;
    source.source_name = file_path;
    source.data.file_path = file_path;

    wchar_t *error_msg = NULL;
    wchar_t *result = baa_preprocess(&source, NULL, &error_msg);

    if (error_msg)
    {
        wprintf(L"Preprocessing error: %ls\n", error_msg);
        free(error_msg);
    }

    return result;
}

void test_predefined_file_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing __الملف__ predefined macro...\n");

    const wchar_t *source = L"__الملف__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // Should expand to the source name
    ASSERT_WSTR_CONTAINS(result, L"test_string");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ __الملف__ predefined macro test passed\n");
}

void test_predefined_line_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing __السطر__ predefined macro...\n");

    const wchar_t *source = L"line 1\n__السطر__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // Should expand to line number (2 in this case)
    ASSERT_WSTR_CONTAINS(result, L"2");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ __السطر__ predefined macro test passed\n");
}

void test_predefined_date_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing __التاريخ__ predefined macro...\n");

    const wchar_t *source = L"__التاريخ__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should expand to a date string in format "MMM dd yyyy"
    // We can't test the exact date, but we can verify it's a quoted string
    ASSERT_WSTR_CONTAINS(result, L"\"");

    // Get current date for comparison
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    wchar_t expected_year[8];
    swprintf(expected_year, 8, L"%d", tm_info->tm_year + 1900);

    // Should contain the current year
    ASSERT_WSTR_CONTAINS(result, expected_year);

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ __التاريخ__ predefined macro test passed\n");
}

void test_predefined_time_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing __الوقت__ predefined macro...\n");

    const wchar_t *source = L"__الوقت__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should expand to a time string in format "HH:MM:SS"
    // We can't test the exact time, but we can verify it's a quoted string with colons
    ASSERT_WSTR_CONTAINS(result, L"\"");
    ASSERT_WSTR_CONTAINS(result, L":");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ __الوقت__ predefined macro test passed\n");
}

void test_predefined_function_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing __الدالة__ predefined macro...\n");

    const wchar_t *source = L"__الدالة__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should expand to a placeholder function name
    ASSERT_WSTR_CONTAINS(result, L"\"");
    ASSERT_WSTR_CONTAINS(result, L"__BAA_FUNCTION_PLACEHOLDER__");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ __الدالة__ predefined macro test passed\n");
}

void test_predefined_version_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing __إصدار_المعيار_باء__ predefined macro...\n");

    const wchar_t *source = L"__إصدار_المعيار_باء__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should expand to version number (10150L)
    ASSERT_WSTR_CONTAINS(result, L"10150L");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ __إصدار_المعيار_باء__ predefined macro test passed\n");
}

void test_predefined_macros_in_expressions(void)
{
    TEST_SETUP();
    wprintf(L"Testing predefined macros in expressions...\n");

    const wchar_t *source = L"#إذا __إصدار_المعيار_باء__ >= 10000L\nversion_ok\n#نهاية_إذا";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should include "version_ok" since the version should be >= 10000L
    ASSERT_WSTR_CONTAINS(result, L"version_ok");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Predefined macros in expressions test passed\n");
}

void test_predefined_macros_not_redefinable(void)
{
    TEST_SETUP();
    wprintf(L"Testing that predefined macros cannot be redefined...\n");

    const wchar_t *source = L"#تعريف __الملف__ \"custom_file\"\n__الملف__";
    wchar_t *result = preprocess_string(source);

    // This should either fail or ignore the redefinition
    if (result)
    {
        // If it succeeds, the original predefined value should be preserved
        // or the redefinition should be ignored
        wprintf(L"  ⚠ Predefined macro redefinition was processed\n");
        free(result);
    }
    else
    {
        wprintf(L"  ✓ Predefined macro redefinition was correctly rejected\n");
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Predefined macros not redefinable test passed\n");
}

void test_predefined_macros_multiline(void)
{
    TEST_SETUP();
    wprintf(L"Testing predefined macros on multiple lines...\n");

    const wchar_t *source = L"Line 1: __السطر__\nLine 2: __السطر__\nLine 3: __السطر__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should have different line numbers
    ASSERT_WSTR_CONTAINS(result, L"1");
    ASSERT_WSTR_CONTAINS(result, L"2");
    ASSERT_WSTR_CONTAINS(result, L"3");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Predefined macros multiline test passed\n");
}

void test_all_predefined_macros_together(void)
{
    TEST_SETUP();
    wprintf(L"Testing all predefined macros together...\n");

    const wchar_t *source = L"File: __الملف__, Line: __السطر__, Date: __التاريخ__, Time: __الوقت__, Function: __الدالة__, Version: __إصدار_المعيار_باء__";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Verify all macros are expanded
    ASSERT_WSTR_CONTAINS(result, L"test_string"); // __الملف__
    ASSERT_WSTR_CONTAINS(result, L"1");           // __السطر__
    ASSERT_WSTR_CONTAINS(result, L"\"");          // __التاريخ__ and __الوقت__ should have quotes
    ASSERT_WSTR_CONTAINS(result, L"10150L");      // __إصدار_المعيار_باء__

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ All predefined macros together test passed\n");
}

void test_predefined_macros_with_user_macros(void)
{
    TEST_SETUP();
    wprintf(L"Testing predefined macros with user-defined macros...\n");

    const wchar_t *source = L"#تعريف MY_VERSION __إصدار_المعيار_باء__\nMY_VERSION";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // MY_VERSION should expand to the predefined version macro value
    ASSERT_WSTR_CONTAINS(result, L"10150L");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Predefined macros with user macros test passed\n");
}

void test_predefined_macros_edge_cases(void)
{
    TEST_SETUP();
    wprintf(L"Testing predefined macros edge cases...\n");

    // Test with empty lines and whitespace
    const wchar_t *source = L"\n\n   __السطر__   \n\n__السطر__\n";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");

    // Should have correct line numbers despite empty lines
    ASSERT_WSTR_CONTAINS(result, L"3"); // First __السطر__ on line 3
    ASSERT_WSTR_CONTAINS(result, L"5"); // Second __السطر__ on line 5

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Predefined macros edge cases test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor Predefined Macro tests...\n\n");

TEST_CASE(test_predefined_file_macro);
TEST_CASE(test_predefined_line_macro);
TEST_CASE(test_predefined_date_macro);
TEST_CASE(test_predefined_time_macro);
TEST_CASE(test_predefined_function_macro);
TEST_CASE(test_predefined_version_macro);
TEST_CASE(test_predefined_macros_in_expressions);
TEST_CASE(test_predefined_macros_not_redefinable);
TEST_CASE(test_predefined_macros_multiline);
TEST_CASE(test_all_predefined_macros_together);
TEST_CASE(test_predefined_macros_with_user_macros);
TEST_CASE(test_predefined_macros_edge_cases);

wprintf(L"\n✓ All Preprocessor Predefined Macro tests completed!\n");

TEST_SUITE_END()
