#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include "include/baa/preprocessor/preprocessor.h"

int main() {
    // Set locale for proper Arabic text display
    setlocale(LC_ALL, "");
    
    printf("Enhanced Error System Demo\n");
    printf("==========================\n\n");
    
    // Test input with multiple types of errors
    const wchar_t *test_input = L"#تعريف VALID_MACRO 42\n"
                                L"#unknown_directive invalid syntax\n"
                                L"#تعريف ANOTHER_VALID 123\n"
                                L"#إذا VALID_MACRO > 40\n"
                                L"    int x = ANOTHER_VALID;\n"
                                L"# Missing endif will be detected\n";
    
    BaaPpSource source = {
        .type = BAA_PP_SOURCE_STRING,
        .source_name = "demo_test.baa",
        .data.source_string = test_input
    };
    
    wchar_t *error_message = NULL;
    wchar_t *result = baa_preprocess(&source, NULL, &error_message);
    
    if (result) {
        wprintf(L"Preprocessing succeeded with output:\n");
        wprintf(L"=====================================\n");
        wprintf(L"%ls\n", result);
        wprintf(L"=====================================\n\n");
        free(result);
    } else {
        printf("Preprocessing failed.\n\n");
    }
    
    if (error_message) {
        wprintf(L"Enhanced Error Report:\n");
        wprintf(L"=====================\n");
        wprintf(L"%ls\n", error_message);
        free(error_message);
    } else {
        printf("No errors reported.\n");
    }
    
    return 0;
}