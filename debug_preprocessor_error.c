#include "baa/preprocessor/preprocessor.h"
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <stdlib.h>

int main()
{
    setlocale(LC_ALL, "");

    printf("Testing preprocessor #خطأ directive...\n");

    // Test simple error directive
    const wchar_t *source_text = L"#خطأ \"Test error message\"";

    BaaPpSource source;
    source.type = BAA_PP_SOURCE_STRING;
    source.source_name = "debug_test";
    source.data.source_string = source_text;

    wchar_t *error_msg = NULL;
    wchar_t *result = baa_preprocess(&source, NULL, &error_msg);

    printf("Result: %p\n", (void *)result);
    printf("Error message: %p\n", (void *)error_msg);

    if (result)
    {
        wprintf(L"Unexpected result: %ls\n", result);
        free(result);
    }

    if (error_msg)
    {
        wprintf(L"Error message: %ls\n", error_msg);
        free(error_msg);
    }
    else
    {
        printf("ERROR: No error message was set!\n");
    }

    return 0;
}
