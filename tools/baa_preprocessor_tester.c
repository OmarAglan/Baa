#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h> // For setlocale
#include <string.h> // For strcmp

// Include the public preprocessor header
#include "baa/preprocessor/preprocessor.h"

// Helper function to print wide strings correctly, handling potential errors
void print_wide_string(FILE* stream, const wchar_t* wstr) {
    if (!wstr) return;

    // Attempt to print using fwprintf - this relies on the locale being set correctly
    if (fwprintf(stream, L"%ls", wstr) < 0) {
        // Fallback: Try to convert to multibyte and print char by char
        // This is less reliable but might work if fwprintf fails
        fprintf(stderr, "\n[Warning: fwprintf failed. Attempting fallback print.]\n");
        size_t buffer_size = wcslen(wstr) * MB_CUR_MAX + 1;
        char* mb_buffer = (char*)malloc(buffer_size);
        if (mb_buffer) {
            size_t converted_bytes = wcstombs(mb_buffer, wstr, buffer_size);
            if (converted_bytes != (size_t)-1) {
                fprintf(stream, "%s", mb_buffer);
            } else {
                fprintf(stderr, "[Error: Fallback wcstombs conversion failed.]\n");
            }
            free(mb_buffer);
        } else {
             fprintf(stderr, "[Error: Failed to allocate buffer for fallback print.]\n");
        }
    }
}


int main(int argc, char *argv[]) {
    // Set locale to allow printing wide characters (like Arabic) correctly to console
    // Use "" to respect the system's locale settings
    setlocale(LC_ALL, "");

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file.baa | ->\n", argv[0]);
        fprintf(stderr, "       Pass '-' as the argument to read from stdin.\n");
        return 1;
    }

    const char *input_arg = argv[1];
    wchar_t *error_message = NULL;
    const char *include_paths[] = {NULL}; // No standard include paths for this simple test
    BaaPpSource pp_source;

    if (strcmp(input_arg, "-") == 0) {
        // Read from stdin
        pp_source.type = BAA_PP_SOURCE_STDIN;
        pp_source.source_name = "<stdin>";
        // data.file_path or data.source_string is not used for stdin
    } else {
        // Read from file
        pp_source.type = BAA_PP_SOURCE_FILE;
        pp_source.source_name = input_arg; // Use input_file path as the name
        pp_source.data.file_path = input_arg;
    }

    // Call the preprocessor
    wchar_t *processed_output = baa_preprocess(&pp_source, include_paths, &error_message);

    // --- Process Results ---
    if (error_message) {
        fprintf(stderr, "Preprocessor Error (from %s):\n", pp_source.source_name); // Show source name
        print_wide_string(stderr, error_message);
        fprintf(stderr, "\n"); // Ensure newline after error
        free(error_message);
        if (processed_output) {
            free(processed_output); // Free output even if error occurred
        }
        return 1; // Indicate failure
    }

    if (processed_output) {
        // Print the processed output to stdout
        print_wide_string(stdout, processed_output);
        // Ensure output ends with a newline if it doesn't already
        size_t len = wcslen(processed_output);
        if (len == 0 || processed_output[len - 1] != L'\n') {
             fwprintf(stdout, L"\n");
        }
        free(processed_output);
    } else {
        // Should not happen if error_message is NULL, but handle defensively
        fprintf(stderr, "Preprocessor returned NULL output without setting an error message.\n");
        return 1;
    }

    return 0; // Indicate success
}
