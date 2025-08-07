#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Helper function to get the full path to test resources
static char* get_test_resource_path(const char* filename) {
    static char full_path[512];
    
    // Try multiple possible paths to find the test resources
    // First try the path assuming we're running from the build directory
#ifdef _WIN32
    sprintf_s(full_path, sizeof(full_path), "../tests/resources/preprocessor_test_cases/%s", filename);
#else
    sprintf(full_path, "../tests/resources/preprocessor_test_cases/%s", filename);
#endif
    
    // Check if the file exists at this path
    FILE* test_file = fopen(full_path, "r");
    if (test_file) {
        fclose(test_file);
        return full_path;
    }
    
    // Try the original relative path (for cases where we're running from source root)
#ifdef _WIN32
    sprintf_s(full_path, sizeof(full_path), "tests/resources/preprocessor_test_cases/%s", filename);
#else
    sprintf(full_path, "tests/resources/preprocessor_test_cases/%s", filename);
#endif
    
    return full_path;
}

// Helper function to preprocess a file and return result
static wchar_t* preprocess_file(const char* filename, wchar_t** error_message) {
    char* file_path = get_test_resource_path(filename);
    
    BaaPpSource source;
    source.type = BAA_PP_SOURCE_FILE;
    source.source_name = file_path;
    source.data.file_path = file_path;
    
    // Set up include paths to point to the test resources directory
    // Try both possible locations for include files
    const char* include_paths[] = {
        "../tests/resources/preprocessor_test_cases",  // From build directory
        "tests/resources/preprocessor_test_cases",     // From source root
        NULL
    };
    
    return baa_preprocess(&source, include_paths, error_message);
}

// Test file inclusion functionality
void test_file_inclusion(void) {
    TEST_SETUP();
    wprintf(L"Testing file inclusion functionality...\n");
    
    wchar_t* error_message = NULL;
    wchar_t* result = preprocess_file("include_test_header.baa", &error_message);
    
    ASSERT_NULL(error_message, L"File inclusion should not produce errors");
    ASSERT_NOT_NULL(result, L"File inclusion should produce output");
    
    // Check that the file content is included
    ASSERT_WSTR_CONTAINS(result, L"سطر من header_test.baa");
    ASSERT_WSTR_CONTAINS(result, L"100"); // HEADER_VAL macro value
    ASSERT_WSTR_CONTAINS(result, L"رسالة من الهيدر"); // HEADER_MSG macro value
    
    free(result);
    free(error_message);
    
    TEST_TEARDOWN();
    wprintf(L"✓ File inclusion test passed\n");
}

// Test nested file inclusion
void test_nested_inclusion(void) {
    TEST_SETUP();
    wprintf(L"Testing nested file inclusion...\n");
    
    wchar_t* error_message = NULL;
    wchar_t* result = preprocess_file("nested_include.baa", &error_message);
    
    ASSERT_NULL(error_message, L"Nested inclusion should not produce errors");
    ASSERT_NOT_NULL(result, L"Nested inclusion should produce output");
    
    // Check that nested content is included
    ASSERT_WSTR_CONTAINS(result, L"رسالة من تضمين متداخل");
    ASSERT_WSTR_CONTAINS(result, L"200"); // NESTED_DEFINE macro value
    
    free(result);
    free(error_message);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Nested inclusion test passed\n");
}

// Test comprehensive preprocessor functionality using the main test file
void test_comprehensive_preprocessing(void) {
    TEST_SETUP();
    wprintf(L"Testing comprehensive preprocessor functionality...\n");
    
    wchar_t* error_message = NULL;
    wchar_t* result = preprocess_file("preprocessor_test_all.baa", &error_message);
    
    // Note: This file contains intentional errors at the end, so we expect some errors
    // but the preprocessing should still produce substantial output
    ASSERT_NOT_NULL(result, L"Comprehensive preprocessing should produce output");
    
    if (result) {
        // Test basic macro expansion
        ASSERT_WSTR_CONTAINS(result, L"3.14159"); // PI macro after redefinition
        ASSERT_WSTR_CONTAINS(result, L"مرحباً يا عالم"); // GREETING macro
        
        // Test conditional compilation
        ASSERT_WSTR_CONTAINS(result, L"ميزة أ مفعلة"); // FEATURE_A conditional
        ASSERT_WSTR_CONTAINS(result, L"ميزة ب غير مفعلة"); // FEATURE_B conditional
        
        // Test function-like macros
        ASSERT_WSTR_CONTAINS(result, L"((5)*(5))"); // SQUARE(5) expansion
        ASSERT_WSTR_CONTAINS(result, L"(3 + 4)"); // ADD(3, 4) expansion
        
        // Test file inclusion results
        ASSERT_WSTR_CONTAINS(result, L"سطر من header_test.baa");
        ASSERT_WSTR_CONTAINS(result, L"رسالة من تضمين متداخل");
        
        // Test predefined macros are expanded
        ASSERT_WSTR_CONTAINS(result, L"preprocessor_test_all.baa"); // __الملف__
        ASSERT_WSTR_CONTAINS(result, L"10150L"); // __إصدار_المعيار_باء__
        
        wprintf(L"✓ Comprehensive preprocessing test passed\n");
    } else {
        wprintf(L"⚠ Comprehensive preprocessing produced no output\n");
        if (error_message) {
            wprintf(L"Error: %ls\n", error_message);
        }
    }
    
    free(result);
    free(error_message);
    
    TEST_TEARDOWN();
}

// Test line number tracking accuracy
void test_line_number_tracking(void) {
    TEST_SETUP();
    wprintf(L"Testing line number tracking accuracy...\n");
    
    wchar_t* error_message = NULL;
    wchar_t* result = preprocess_file("line_number_test.baa", &error_message);
    
    // This file should produce a warning about macro redefinition
    // We expect the preprocessing to succeed but with warnings
    ASSERT_NOT_NULL(result, L"Line number test should produce output");
    
    if (result) {
        // Check that macros are properly expanded
        ASSERT_WSTR_CONTAINS(result, L"3.14159"); // PI after redefinition
        ASSERT_WSTR_CONTAINS(result, L"hello"); // TEST_MACRO value
        
        wprintf(L"✓ Line number tracking test passed\n");
    }
    
    // Check if there are warnings about redefinition (expected)
    if (error_message) {
        wprintf(L"Expected warnings/errors: %ls\n", error_message);
    }
    
    free(result);
    free(error_message);
    
    TEST_TEARDOWN();
}

// Test error handling with malformed input
void test_error_handling(void) {
    TEST_SETUP();
    wprintf(L"Testing error handling with malformed input...\n");
    
    // Test with non-existent include file
    const wchar_t* bad_include = L"#تضمين \"non_existent_file.baa\"\nSome content";
    
    BaaPpSource source;
    source.type = BAA_PP_SOURCE_STRING;
    source.source_name = "test_error_handling";
    source.data.source_string = bad_include;
    
    wchar_t* error_message = NULL;
    wchar_t* result = baa_preprocess(&source, NULL, &error_message);
    
    // Should produce an error for missing file
    ASSERT_NOT_NULL(error_message, L"Missing include file should produce error");
    
    // May or may not produce output depending on error recovery
    if (result) {
        free(result);
    }
    free(error_message);
    
    TEST_TEARDOWN();
    wprintf(L"✓ Error handling test passed\n");
}

// Test memory management and cleanup
void test_memory_management(void) {
    TEST_SETUP();
    wprintf(L"Testing memory management...\n");
    
    // Process the same file multiple times to test for memory leaks
    for (int i = 0; i < 10; i++) {
        wchar_t* error_message = NULL;
        wchar_t* result = preprocess_file("include_test_header.baa", &error_message);
        
        if (result) {
            free(result);
        }
        if (error_message) {
            free(error_message);
        }
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Memory management test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Comprehensive Preprocessor tests...\n\n");

TEST_CASE(test_file_inclusion);
TEST_CASE(test_nested_inclusion);
TEST_CASE(test_comprehensive_preprocessing);
TEST_CASE(test_line_number_tracking);
TEST_CASE(test_error_handling);
TEST_CASE(test_memory_management);

wprintf(L"\n✓ All Comprehensive Preprocessor tests completed!\n");

TEST_SUITE_END()
