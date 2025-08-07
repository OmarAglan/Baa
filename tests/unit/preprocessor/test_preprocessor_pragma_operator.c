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

void test_pragma_operator_basic_arabic(void)
{
    TEST_SETUP();
    wprintf(L"Testing basic أمر_براغما operator...\n");

    const wchar_t *source = L"أمر_براغما(\"مرة_واحدة\")\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process the pragma
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with أمر_براغما operator");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Basic أمر_براغما operator test passed\n");
}

void test_pragma_operator_short_form(void)
{
    TEST_SETUP();
    wprintf(L"Testing براغما operator (short form)...\n");

    const wchar_t *source = L"براغما(\"مرة_واحدة\")\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process the pragma
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with براغما operator");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ براغما operator (short form) test passed\n");
}

void test_pragma_operator_unknown_pragma(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator with unknown pragma...\n");

    const wchar_t *source = L"أمر_براغما(\"unknown_pragma_name\")\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and ignore unknown pragma
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with unknown pragma");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ أمر_براغما operator with unknown pragma test passed\n");
}

void test_pragma_operator_empty_string(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator with empty string...\n");

    const wchar_t *source = L"أمر_براغما(\"\")\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and ignore empty pragma
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with empty pragma string");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ أمر_براغما operator with empty string test passed\n");
}

void test_pragma_operator_escape_sequences(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator with escape sequences...\n");

    const wchar_t *source = L"أمر_براغما(\"test\\nwith\\tescapes\")\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process escape sequences
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with escape sequences");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ أمر_براغما operator with escape sequences test passed\n");
}

void test_pragma_operator_with_whitespace(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator with whitespace variations...\n");

    const wchar_t *sources[] = {
        L"أمر_براغما  (\"مرة_واحدة\")\ntest_content",      // Spaces before (
        L"أمر_براغما( \"مرة_واحدة\")\ntest_content",      // Space after (
        L"أمر_براغما(\"مرة_واحدة\" )\ntest_content",      // Space before )
        L"أمر_براغما  (  \"مرة_واحدة\"  )\ntest_content", // Multiple spaces
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
    wprintf(L"✓ أمر_براغما operator with whitespace test passed\n");
}

void test_pragma_operator_syntax_errors(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator syntax errors...\n");

    const wchar_t *error_sources[] = {
        L"أمر_براغما \"مرة_واحدة\"",           // Missing parentheses
        L"أمر_براغما(مرة_واحدة)",            // Missing quotes
        L"أمر_براغما(\"مرة_واحدة\"",          // Missing closing )
        L"أمر_براغما(\"unterminated",         // Unterminated string
    };

    size_t num_sources = sizeof(error_sources) / sizeof(error_sources[0]);

    for (size_t i = 0; i < num_sources; i++)
    {
        wchar_t *error_msg = NULL;
        wchar_t *result = preprocess_string_with_error(error_sources[i], &error_msg);

        // Should fail with syntax error
        ASSERT_NULL(result, L"Preprocessing should fail with syntax errors");
        ASSERT_NOT_NULL(error_msg, L"Should provide error message for syntax errors");

        if (result) free(result);
        if (error_msg) free(error_msg);
    }

    TEST_TEARDOWN();
    wprintf(L"✓ أمر_براغما operator syntax errors test passed\n");
}

void test_pragma_operator_in_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator in macro expansion...\n");

    const wchar_t *source = L"#تعريف PRAGMA_ONCE أمر_براغما(\"مرة_واحدة\")\nPRAGMA_ONCE\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and expand macro containing pragma operator
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with pragma operator in macro");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ أمر_براغما operator in macro test passed\n");
}

void test_pragma_operator_in_conditional(void)
{
    TEST_SETUP();
    wprintf(L"Testing أمر_براغما operator in conditional compilation...\n");

    const wchar_t *source = L"#إذا 1\nأمر_براغما(\"مرة_واحدة\")\ntest_content\n#نهاية_إذا";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process pragma in true conditional
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with pragma operator in conditional");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ أمر_براغما operator in conditional test passed\n");
}

void test_pragma_operator_multiple(void)
{
    TEST_SETUP();
    wprintf(L"Testing multiple أمر_براغما operators...\n");

    const wchar_t *source = L"أمر_براغما(\"first_pragma\")\nأمر_براغما(\"second_pragma\")\ntest_content";
    wchar_t *error_msg = NULL;
    wchar_t *result = preprocess_string_with_error(source, &error_msg);

    // Should succeed and process multiple pragma operators
    ASSERT_NOT_NULL(result, L"Preprocessing should succeed with multiple pragma operators");
    ASSERT_WSTR_CONTAINS(result, L"test_content");

    if (error_msg)
    {
        wprintf(L"  Unexpected error: %ls\n", error_msg);
        free(error_msg);
    }

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Multiple أمر_براغما operators test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor أمر_براغما (_Pragma) Operator tests...\n\n");

TEST_CASE(test_pragma_operator_basic_arabic);
TEST_CASE(test_pragma_operator_short_form);
TEST_CASE(test_pragma_operator_unknown_pragma);
TEST_CASE(test_pragma_operator_empty_string);
TEST_CASE(test_pragma_operator_escape_sequences);
TEST_CASE(test_pragma_operator_with_whitespace);
TEST_CASE(test_pragma_operator_syntax_errors);
TEST_CASE(test_pragma_operator_in_macro);
TEST_CASE(test_pragma_operator_in_conditional);
TEST_CASE(test_pragma_operator_multiple);

wprintf(L"\n✓ All Preprocessor أمر_براغما (_Pragma) Operator tests completed!\n");

TEST_SUITE_END()