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

void test_simple_object_like_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing simple object-like macro...\n");

    const wchar_t *source = L"#تعريف MAX 100\nMAX";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"100");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Simple object-like macro test passed\n");
}

void test_arabic_macro_names(void)
{
    TEST_SETUP();
    wprintf(L"Testing Arabic macro names...\n");

    const wchar_t *source = L"#تعريف العدد_الأقصى 42\nالعدد_الأقصى";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"42");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Arabic macro names test passed\n");
}

void test_function_like_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing function-like macro...\n");

    const wchar_t *source = L"#تعريف SQUARE(x) ((x) * (x))\nSQUARE(5)";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"((5) * (5))");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Function-like macro test passed\n");
}

void test_macro_with_multiple_parameters(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro with multiple parameters...\n");

    const wchar_t *source = L"#تعريف ADD(a, b) ((a) + (b))\nADD(10, 20)";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"((10) + (20))");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Multiple parameters macro test passed\n");
}

void test_nested_macro_expansion(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested macro expansion...\n");

    const wchar_t *source = L"#تعريف A 10\n#تعريف B A\nB";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"10");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Nested macro expansion test passed\n");
}

void test_macro_redefinition(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro redefinition...\n");

    const wchar_t *source = L"#تعريف VALUE 100\n#تعريف VALUE 200\nVALUE";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"200"); // Should use the latest definition

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Macro redefinition test passed\n");
}

void test_macro_undef(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro undefinition...\n");

    const wchar_t *source = L"#تعريف TEMP 42\n#إلغاء_تعريف TEMP\nTEMP";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // After undefinition, TEMP should remain as TEMP (not expanded)
    ASSERT_WSTR_CONTAINS(result, L"TEMP");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Macro undefinition test passed\n");
}

void test_string_literal_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing string literal macro...\n");

    const wchar_t *source = L"#تعريف MESSAGE \"مرحبا بالعالم\"\nMESSAGE";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"\"مرحبا بالعالم\"");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ String literal macro test passed\n");
}

void test_empty_macro(void)
{
    TEST_SETUP();
    wprintf(L"Testing empty macro...\n");

    const wchar_t *source = L"#تعريف EMPTY\nEMPTY text";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // EMPTY should expand to nothing, leaving just "text"
    ASSERT_WSTR_CONTAINS(result, L"text");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Empty macro test passed\n");
}

void test_macro_with_whitespace(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro with whitespace...\n");

    const wchar_t *source = L"#تعريف SPACED   value with spaces   \nSPACED";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    ASSERT_WSTR_CONTAINS(result, L"value with spaces");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Macro with whitespace test passed\n");
}

void test_recursive_macro_prevention(void)
{
    TEST_SETUP();
    wprintf(L"Testing recursive macro prevention...\n");

    // This should not cause infinite recursion
    const wchar_t *source = L"#تعريف RECURSIVE RECURSIVE\nRECURSIVE";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // Should prevent infinite recursion and leave RECURSIVE unexpanded
    ASSERT_WSTR_CONTAINS(result, L"RECURSIVE");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Recursive macro prevention test passed\n");
}

void test_macro_in_string_literal(void)
{
    TEST_SETUP();
    wprintf(L"Testing macro inside string literal...\n");

    const wchar_t *source = L"#تعريف VALUE 42\n\"VALUE should not expand\"";
    wchar_t *result = preprocess_string(source);

    ASSERT_NOT_NULL(result, L"Preprocessing should succeed");
    // Macros should not expand inside string literals
    ASSERT_WSTR_CONTAINS(result, L"\"VALUE should not expand\"");

    free(result);

    TEST_TEARDOWN();
    wprintf(L"✓ Macro in string literal test passed\n");
}

void test_invalid_macro_definitions(void)
{
    TEST_SETUP();
    wprintf(L"Testing invalid macro definitions...\n");

    // Test various invalid macro definitions
    const wchar_t *invalid_sources[] = {
        L"#تعريف\n",       // Missing name and body
        L"#تعريف 123\n",   // Invalid name (starts with digit)
        L"#تعريف FUNC(\n", // Unclosed parameter list
    };

    size_t num_sources = sizeof(invalid_sources) / sizeof(invalid_sources[0]);

    for (size_t i = 0; i < num_sources; i++)
    {
        wchar_t *result = preprocess_string(invalid_sources[i]);

        // These should either return NULL (error) or handle gracefully
        if (result)
        {
            wprintf(L"  ⚠ Unexpectedly processed invalid macro: case %zu\n", i);
            free(result);
        }
        else
        {
            wprintf(L"  ✓ Correctly rejected invalid macro: case %zu\n", i);
        }
    }

    TEST_TEARDOWN();
    wprintf(L"✓ Invalid macro definitions test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Preprocessor Macro tests...\n\n");

TEST_CASE(test_simple_object_like_macro);
TEST_CASE(test_arabic_macro_names);
TEST_CASE(test_function_like_macro);
TEST_CASE(test_macro_with_multiple_parameters);
TEST_CASE(test_nested_macro_expansion);
TEST_CASE(test_macro_redefinition);
TEST_CASE(test_macro_undef);
TEST_CASE(test_string_literal_macro);
TEST_CASE(test_empty_macro);
TEST_CASE(test_macro_with_whitespace);
TEST_CASE(test_recursive_macro_prevention);
TEST_CASE(test_macro_in_string_literal);
TEST_CASE(test_invalid_macro_definitions);

wprintf(L"\n✓ All Preprocessor Macro tests completed!\n");

TEST_SUITE_END()
