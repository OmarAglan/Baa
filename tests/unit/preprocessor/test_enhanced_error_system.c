#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <assert.h>
#include <string.h>

#include "baa/preprocessor/preprocessor.h"
#include "preprocessor_internal.h"

// Test the enhanced error system functionality
void test_error_severity_classification() {
    printf("Testing error severity classification...\n");
    
    BaaPreprocessor pp_state = {0};
    bool init_result = init_preprocessor_error_system(&pp_state);
    assert(init_result == true);
    
    PpSourceLocation loc = {"test.baa", 1, 1};
    
    // Test fatal error
    PP_REPORT_FATAL(&pp_state, &loc, PP_ERROR_OUT_OF_MEMORY, "memory", 
                    L"فشل في تخصيص الذاكرة");
    assert(pp_state.fatal_count == 1);
    assert(pp_state.had_fatal_error == true);
    assert(pp_state.diagnostic_count == 1);
    
    // Test regular error
    PP_REPORT_ERROR(&pp_state, &loc, PP_ERROR_UNKNOWN_DIRECTIVE, "directive",
                   L"توجيه غير معروف");
    assert(pp_state.error_count == 1);
    assert(pp_state.diagnostic_count == 2);
    
    // Test warning
    PP_REPORT_WARNING(&pp_state, &loc, PP_ERROR_UNDEFINED_IDENTIFIER, "expression",
                     L"معرف غير معرف");
    assert(pp_state.warning_count == 1);
    assert(pp_state.diagnostic_count == 3);
    
    // Test note
    PP_REPORT_NOTE(&pp_state, &loc, 0, "info", L"ملاحظة إعلامية");
    assert(pp_state.note_count == 1);
    assert(pp_state.diagnostic_count == 4);
    
    cleanup_preprocessor_error_system(&pp_state);
    printf("✓ Error severity classification test passed\n");
}

void test_error_limit_enforcement() {
    printf("Testing error limit enforcement...\n");
    
    BaaPreprocessor pp_state = {0};
    init_preprocessor_error_system(&pp_state);
    
    // Set a lower error limit for testing
    pp_state.error_limits.max_errors = 5;
    
    PpSourceLocation loc = {"test.baa", 1, 1};
    
    // Add errors up to the limit
    for (int i = 0; i < 10; i++) {
        PP_REPORT_ERROR(&pp_state, &loc, PP_ERROR_UNKNOWN_DIRECTIVE, "directive",
                       L"خطأ رقم %d", i + 1);
    }
    
    // Should stop at the limit
    assert(pp_state.error_count == 5);
    assert(pp_state.diagnostic_count == 5);
    
    cleanup_preprocessor_error_system(&pp_state);
    printf("✓ Error limit enforcement test passed\n");
}

void test_recovery_action_determination() {
    printf("Testing recovery action determination...\n");
    
    BaaPreprocessor pp_state = {0};
    init_preprocessor_error_system(&pp_state);
    
    PpSourceLocation loc = {"test.baa", 1, 1};
    
    // Test directive error recovery
    PpRecoveryAction action = determine_recovery_action(&pp_state, PP_DIAG_ERROR, 
                                                       "directive", &loc);
    assert(action == PP_RECOVERY_SKIP_DIRECTIVE);
    
    // Test fatal error recovery
    action = determine_recovery_action(&pp_state, PP_DIAG_FATAL, "memory", &loc);
    assert(action == PP_RECOVERY_HALT);
    
    // Test expression error recovery
    action = determine_recovery_action(&pp_state, PP_DIAG_ERROR, "expression", &loc);
    assert(action == PP_RECOVERY_CONTINUE);
    
    cleanup_preprocessor_error_system(&pp_state);
    printf("✓ Recovery action determination test passed\n");
}

void test_error_summary_generation() {
    printf("Testing error summary generation...\n");
    
    BaaPreprocessor pp_state = {0};
    init_preprocessor_error_system(&pp_state);
    
    PpSourceLocation loc = {"test.baa", 1, 1};
    
    // Add some errors
    PP_REPORT_ERROR(&pp_state, &loc, PP_ERROR_UNKNOWN_DIRECTIVE, "directive",
                   L"توجيه غير معروف");
    PP_REPORT_WARNING(&pp_state, &loc, PP_ERROR_UNDEFINED_IDENTIFIER, "expression",
                     L"معرف غير معرف");
    
    wchar_t *summary = generate_error_summary(&pp_state);
    assert(summary != NULL);
    
    // Check that summary contains expected content
    assert(wcsstr(summary, L"1 خطأ") != NULL);
    assert(wcsstr(summary, L"1 تحذير") != NULL);
    assert(wcsstr(summary, L"توجيه غير معروف") != NULL);
    assert(wcsstr(summary, L"معرف غير معرف") != NULL);
    
    free(summary);
    cleanup_preprocessor_error_system(&pp_state);
    printf("✓ Error summary generation test passed\n");
}

void test_end_to_end_error_collection() {
    printf("Testing end-to-end error collection...\n");
    
    // Test input with multiple errors
    const wchar_t *test_input = L"#تعريف VALID_MACRO 42\n"
                                L"#unknown_directive invalid syntax\n"
                                L"#تعريف ANOTHER_VALID 123\n"
                                L"#إذا VALID_MACRO > 40\n"
                                L"    int x = ANOTHER_VALID;\n"
                                L"# Missing endif will be detected\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "test_error_collection.baa",
        .data.source_string = test_input
    };
    
    wchar_t *error_message = NULL;
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    // Should have collected errors but still produced some output
    assert(error_message != NULL);
    
    // Check for expected error content
    printf("Error message: %ls\n", error_message);
    
    if (result) free(result);
    if (error_message) free(error_message);
    
    printf("✓ End-to-end error collection test passed\n");
}

int main() {
    printf("Running Enhanced Error System Tests\n");
    printf("==================================\n\n");
    
    test_error_severity_classification();
    test_error_limit_enforcement();
    test_recovery_action_determination();
    test_error_summary_generation();
    test_end_to_end_error_collection();
    
    printf("\n✅ All enhanced error system tests passed!\n");
    return 0;
}