#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h> // For setlocale
#include <string.h> // For strncmp

// Include the public lexer header
#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h" // For baa_read_file_to_wide_string
#include "baa/utils/errors.h" // For BaaError and BAA_ERROR_NONE

// Helper function to print wide strings correctly, handling potential errors
void print_wide_string_lex(FILE* stream, const wchar_t* wstr) {
    if (!wstr) return;

    if (fwprintf(stream, L"%ls", wstr) < 0) {
        fprintf(stderr, "\n[Warning: fwprintf failed for lexeme. Attempting fallback print.]\n");
        size_t buffer_size = wcslen(wstr) * MB_CUR_MAX + 1;
        char* mb_buffer = (char*)malloc(buffer_size);
        if (mb_buffer) {
            size_t converted_bytes = wcstombs(mb_buffer, wstr, buffer_size);
            if (converted_bytes != (size_t)-1) {
                fprintf(stream, "%s", mb_buffer);
            } else {
                fprintf(stderr, "[Error: Fallback wcstombs conversion failed for lexeme.]\n");
            }
            free(mb_buffer);
        } else {
             fprintf(stderr, "[Error: Failed to allocate buffer for lexeme fallback print.]\n");
        }
    }
}

// Function to convert BaaTokenType to its string representation
// (This might be a simplified version or could call baa_token_type_to_string if it's comprehensive)
const wchar_t* token_type_to_display_string(BaaTokenType type) {
    // Use the existing utility if it's suitable, otherwise, a local one.
    // For this tester, directly using baa_token_type_to_string is fine.
    return baa_token_type_to_string(type);
}


int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file.baa>\n", argv[0]);
        return 1;
    }

    const char *input_file_path = argv[1];
    wchar_t *source_code = NULL;
    long file_size = 0; // File size might not be directly available from baa_file_content

    // Convert input_file_path to wchar_t for baa_file_content
    size_t input_file_path_w_len = mbstowcs(NULL, input_file_path, 0) + 1;
    wchar_t* input_file_path_w = (wchar_t*)malloc(input_file_path_w_len * sizeof(wchar_t));
    if (!input_file_path_w) {
        fprintf(stderr, "Failed to allocate memory for widechar input file path.\n");
        return 1;
    }
    mbstowcs(input_file_path_w, input_file_path, input_file_path_w_len);

    // Read the file content using baa_file_content
    source_code = baa_file_content(input_file_path_w);

    if (!source_code) {
        // Assuming baa_file_content calls baa_set_error on failure and returns NULL
        const wchar_t* error_msg = baa_get_error_message(); // Retrieve error set by baa_file_content
        BaaError err_code = baa_get_error(); // Get the error code
        if (error_msg && wcslen(error_msg) > 0) {
            fwprintf(stderr, L"Error reading file '%ls': %ls (Code: %d)\n", input_file_path_w, error_msg, err_code);
        } else {
            // Fallback if no specific message was set
            fwprintf(stderr, L"Error reading file '%ls' (Code: %d). Check if file exists and is readable.\n", input_file_path_w, err_code);
        }
        free(input_file_path_w);
        return 1;
    }
    // file_size is not directly returned by baa_file_content,
    // but lexer doesn't strictly need it if source_code is null-terminated.
    // baa_init_lexer takes source_length, which can be wcslen(source_code).

    BaaLexer lexer;
    // Initialize the lexer with the source code and filename
    // The filename argument to baa_init_lexer is for error reporting context,
    // so passing input_file_path (converted to wchar_t if necessary, or just as char*)
    // is appropriate. For simplicity, if baa_init_lexer takes const char* for filename:
    // baa_init_lexer(&lexer, source_code, input_file_path);

    // If baa_init_lexer expects wchar_t for filename, we'd convert input_file_path.
    // Let's assume baa_init_lexer can take const char* for filename for now,
    // or we adapt if the actual signature is different.
    // Based on preprocessor_tester, it seems a simple char* is fine for the "name".
    // The `baa_init_lexer` in `lexer.h` takes `const wchar_t* filename`.
    // We use input_file_path_w which was already converted for baa_file_content.

    // The old separate filename_w conversion is no longer needed.
    // wchar_t* filename_w = input_file_path_w; // Use the already converted path

    baa_init_lexer(&lexer, source_code, input_file_path_w); // Pass widechar filename

    fwprintf(stdout, L"Tokens from file: %ls\n", input_file_path_w);
    fwprintf(stdout, L"-------------------------------------------------\n");
    fwprintf(stdout, L"| %-20ls | %-5ls | %-5ls | Lexeme\n", L"Type", L"Line", L"Col");
    fwprintf(stdout, L"-------------------------------------------------\n");

    BaaToken *token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL && token->type != BAA_TOKEN_EOF) {
        const wchar_t* type_str = token_type_to_display_string(token->type);

        // For BAA_TOKEN_ERROR, token->lexeme might contain the error message.
        // For other tokens, it's the source text.
        // Ensure lexeme is safely printable.
        wchar_t temp_lexeme_buffer[256]; // Buffer for potentially long lexemes/errors
        if (token->length < 255) {
            wcsncpy(temp_lexeme_buffer, token->lexeme, token->length);
            temp_lexeme_buffer[token->length] = L'\0';
        } else {
            wcsncpy(temp_lexeme_buffer, token->lexeme, 251);
            wcscpy(temp_lexeme_buffer + 251, L"...");
            temp_lexeme_buffer[254] = L'\0';
        }


        fwprintf(stdout, L"| %-20ls | %-5zu | %-5zu | ", type_str, token->line, token->column);
        print_wide_string_lex(stdout, temp_lexeme_buffer);
        fwprintf(stdout, L"\n");

        // If it's an error token, the lexeme is the error message.
        // If it's a string literal, the lexeme is the *processed* content (escapes handled).
        // For other tokens, lexeme is a direct slice from source.
        // The BaaToken struct has `lexeme` and `length`.
        // `make_token` in `lexer_internal.c` typically allocates memory for token->lexeme
        // and copies the segment from source. String/Char literals might have their own allocated lexemes.

        baa_free_token(token); // Free token after processing
    }

    if (token && token->type == BAA_TOKEN_EOF) {
        fwprintf(stdout, L"| %-20ls | %-5zu | %-5zu | <EOF>\n", token_type_to_display_string(token->type), token->line, token->column);
        baa_free_token(token); // Free the EOF token
    }

    fwprintf(stdout, L"-------------------------------------------------\n");

    // No explicit baa_free_lexer(&lexer) if BaaLexer is stack-allocated and doesn't own source_code.
    // If baa_init_lexer allocates internal resources in lexer, a corresponding free function would be needed.
    // Assuming BaaLexer itself doesn't need freeing beyond its members if they were dynamically allocated.
    // The `source_code` was read by this tester, so it must free it.
    free(source_code);
    free(input_file_path_w); // Free the widechar version of the input file path

    return 0;
}
