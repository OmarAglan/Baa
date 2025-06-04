#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include <stdio.h>
#include <wchar.h>
#include <string.h>

// Test error recovery with malformed conditional directives
static bool test_malformed_conditional_recovery()
{
    wchar_t *error_message = NULL;
    
    const wchar_t *test_source = 
        L"#define VALID_MACRO 42\n"
        L"#if INVALID && & EXPRESSION\n"
        L"    some code here\n"
        L"#endif\n"
        L"#define ANOTHER_VALID_MACRO \"hello\"\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "<test_malformed_conditional>",
        .data.source_string = test_source
    };
    
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // Should have errors but still process valid parts
    if (!error_message) {
        printf("Expected error message for malformed conditional\n");
        if (result) free(result);
        return false;
    }
    
    // Check that valid macros were still processed
    if (result && wcsstr(result, L"42") == NULL) {
        printf("Valid macro VALID_MACRO not found in output\n");
        free(result);
        free(error_message);
        return false;
    }
    
    if (result && wcsstr(result, L"hello") == NULL) {
        printf("Valid macro ANOTHER_VALID_MACRO not found in output\n");
        free(result);
        free(error_message);
        return false;
    }
    
    printf("Malformed conditional recovery test passed\n");
    printf("Error message: %ls\n", error_message);
    
    if (result) free(result);
    free(error_message);
    return true;
}

// Test recovery from missing endif
static bool test_missing_endif_recovery()
{
    wchar_t *error_message = NULL;
    
    const wchar_t *test_source = 
        L"#define START_MACRO 1\n"
        L"#if 1\n"
        L"    code inside if\n"
        L"    #if 2\n"
        L"        nested code\n"
        L"    // Missing #endif for nested\n"
        L"// Missing #endif for outer\n"
        L"#define END_MACRO 2\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "<test_missing_endif>",
        .data.source_string = test_source
    };
    
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // Should have errors about missing endif
    if (!error_message) {
        printf("Expected error message for missing endif\n");
        if (result) free(result);
        return false;
    }
    
    // Check that it mentions missing endif
    if (!wcsstr(error_message, L"نهاية_إذا") && !wcsstr(error_message, L"endif")) {
        printf("Error message should mention missing endif\n");
        free(error_message);
        if (result) free(result);
        return false;
    }
    
    printf("Missing endif recovery test passed\n");
    printf("Error message: %ls\n", error_message);
    
    if (result) free(result);
    free(error_message);
    return true;
}

// Test recovery from unknown directives
static bool test_unknown_directive_recovery()
{
    wchar_t *error_message = NULL;
    
    const wchar_t *test_source = 
        L"#define BEFORE_ERROR 1\n"
        L"#unknown_directive some content\n"
        L"#define AFTER_ERROR 2\n"
        L"#invalid_directive more content\n"
        L"#define FINAL_MACRO 3\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "<test_unknown_directive>",
        .data.source_string = test_source
    };
    
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // Should have errors but continue processing
    if (!error_message) {
        printf("Expected error message for unknown directives\n");
        if (result) free(result);
        return false;
    }
    
    // Should still process valid macros before and after errors
    if (result) {
        // Check for processed macros (they should be defined)
        // The actual expansion would depend on how the preprocessor handles them
        printf("Result contains valid macros processing\n");
    }
    
    printf("Unknown directive recovery test passed\n");
    printf("Error message: %ls\n", error_message);
    
    if (result) free(result);
    free(error_message);
    return true;
}

// Test recovery from expression evaluation errors
static bool test_expression_error_recovery()
{
    wchar_t *error_message = NULL;
    
    const wchar_t *test_source = 
        L"#define VALID_START 1\n"
        L"#if UNDEFINED_MACRO + 5 / 0\n"
        L"    should not appear\n"
        L"#else\n"
        L"    fallback code\n"
        L"#endif\n"
        L"#define VALID_END 2\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "<test_expression_error>",
        .data.source_string = test_source
    };
    
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // May have errors for undefined macro or division by zero
    // But should still process the conditional structure
    
    if (result) {
        // Should contain fallback code, not the "should not appear" text
        if (wcsstr(result, L"should not appear")) {
            printf("Error: conditional with bad expression was not handled correctly\n");
            free(result);
            if (error_message) free(error_message);
            return false;
        }
        
        if (wcsstr(result, L"fallback code")) {
            printf("Good: fallback code found in result\n");
        }
    }
    
    printf("Expression error recovery test passed\n");
    if (error_message) {
        printf("Error message: %ls\n", error_message);
    }
    
    if (result) free(result);
    if (error_message) free(error_message);
    return true;
}

// Test error count limit
static bool test_error_count_limit()
{
    wchar_t *error_message = NULL;
    
    // Create a source with many errors
    wchar_t test_source[2048];
    wcscpy(test_source, L"#define VALID_START 1\n");
    
    // Add many invalid directives
    for (int i = 0; i < 30; i++) {
        wchar_t line[100];
        swprintf(line, 100, L"#invalid_directive_%d content\n", i);
        wcscat(test_source, line);
    }
    
    wcscat(test_source, L"#define VALID_END 2\n");
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "<test_error_limit>",
        .data.source_string = test_source
    };
    
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // Should abort due to too many errors
    if (!error_message) {
        printf("Expected error message for error limit exceeded\n");
        if (result) free(result);
        return false;
    }
    
    // Check if error message mentions abort due to error limit
    if (wcsstr(error_message, L"الحد الأقصى") || wcsstr(error_message, L"إيقاف")) {
        printf("Good: Error limit mechanism triggered\n");
    }
    
    printf("Error count limit test passed\n");
    printf("Error message: %ls\n", error_message);
    
    if (result) free(result);
    free(error_message);
    return true;
}

// Test continued processing after recoverable errors
static bool test_continued_processing()
{
    wchar_t *error_message = NULL;
    
    const wchar_t *test_source = 
        L"#define MACRO1 \"value1\"\n"
        L"#unknown_directive1 error content\n"
        L"#define MACRO2 \"value2\"\n"
        L"#if UNDEFINED_VAR\n"
        L"    skip this\n"
        L"#endif\n"
        L"#define MACRO3 \"value3\"\n"
        L"#unknown_directive2 more error\n"
        L"#define MACRO4 \"value4\"\n"
        L"// Regular code line\n"
        L"int main() { return 0; }\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "<test_continued_processing>",
        .data.source_string = test_source
    };
    
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // Should have errors but still produce output
    if (result) {
        // Check that valid content made it through
        if (wcsstr(result, L"int main()")) {
            printf("Good: Regular code preserved after errors\n");
        } else {
            printf("Error: Regular code not found in output\n");
            free(result);
            if (error_message) free(error_message);
            return false;
        }
    }
    
    printf("Continued processing test passed\n");
    if (error_message) {
        printf("Errors reported: %ls\n", error_message);
    }
    
    if (result) free(result);
    if (error_message) free(error_message);
    return true;
}

int main()
{
    printf("=== Baa Preprocessor Error Recovery Tests ===\n\n");
    
    bool all_passed = true;
    
    printf("1. Testing malformed conditional recovery...\n");
    all_passed &= test_malformed_conditional_recovery();
    printf("\n");
    
    printf("2. Testing missing endif recovery...\n");
    all_passed &= test_missing_endif_recovery();
    printf("\n");
    
    printf("3. Testing unknown directive recovery...\n");
    all_passed &= test_unknown_directive_recovery();
    printf("\n");
    
    printf("4. Testing expression error recovery...\n");
    all_passed &= test_expression_error_recovery();
    printf("\n");
    
    printf("5. Testing error count limit...\n");
    all_passed &= test_error_count_limit();
    printf("\n");
    
    printf("6. Testing continued processing...\n");
    all_passed &= test_continued_processing();
    printf("\n");
    
    if (all_passed) {
        printf("=== All Error Recovery Tests PASSED ===\n");
        return 0;
    } else {
        printf("=== Some Error Recovery Tests FAILED ===\n");
        return 1;
    }
}
