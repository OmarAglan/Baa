#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#include "baa/compiler.h"
#include "baa/utils/utils.h"
#include "baa/lexer/lexer.h"
// #include "baa/parser/parser.h" // Removed as parser is being removed
// #include "baa/ast/ast_printer.h" // Include the AST printer header - Removed as AST is being removed
#include "baa/codegen/codegen.h"
#include "baa/preprocessor/preprocessor.h"

// Helper: Convert char* to wchar_t* (might move to utils later)
static wchar_t *char_to_wchar_compiler(const char *str) {
    size_t len = strlen(str) + 1;
    wchar_t *wstr = malloc(len * sizeof(wchar_t));
    if (!wstr) {
        return NULL;
    }
    size_t converted;
#ifdef _WIN32
    mbstowcs_s(&converted, wstr, len, str, len - 1);
#else
    converted = mbstowcs(wstr, str, len - 1);
    wstr[converted] = L'\0'; // Ensure null termination
#endif
    return wstr;
}

int compile_baa_file(const char* filename) {
    // Set locale for wide character support
    setlocale(LC_ALL, "");

    // Call Preprocessor
    wchar_t* error_message = NULL;
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_FILE,
        .source_name = filename, // Use original filename for error reporting context
        .data.file_path = filename
    };
    wchar_t* source = baa_preprocess(&pp_source, NULL, &error_message);

    if (!source) {
        if (error_message) {
            fwprintf(stderr, L"Error (Preprocessor): %ls\n", error_message);
            free(error_message);
        } else {
            fwprintf(stderr, L"Error: Preprocessing failed for file %hs (unknown error).\n", filename);
        }
        return 1;
    }

    // Print preprocessed source for debugging
    wprintf(L"--- Preprocessed Source ---\n%ls\n--- End Preprocessed Source ---\n", source);

    // Convert filename to wide string for lexer/output path
    wchar_t* wfilename = char_to_wchar_compiler(filename);
    if (!wfilename) {
        fprintf(stderr, "Error: Failed to convert filename to wchar_t.\n");
        free(source);
        return 1;
    }

    // Initialize lexer
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, wfilename);

    // --- Lexer Debugging Loop ---
    wprintf(L"\n--- Lexer Tokens ---\n");
    int token_count = 0;
    for (;;) { // Use a for loop with explicit break
        BaaToken* token = baa_lexer_next_token(&lexer);
        if (!token) {
            // Handle case where baa_lexer_next_token returns NULL (e.g., allocation error)
            fprintf(stderr, "Error: baa_lexer_next_token returned NULL!\n");
            break; // Exit loop on allocation failure
        }

        // Print token details: Type (as string), Lexeme, Line, Column
        // Ensure lexeme is printed correctly, it's a const wchar_t*
            // Need to handle potential NULL lexeme for EOF/Error if lexer sets it that way
        const wchar_t* type_str = baa_token_type_to_string(token->type);
        wprintf(L"Token %03d: Type=%ls, Lexeme='%.*ls', Line=%zu, Col=%zu\n",
                token_count++,
                type_str ? type_str : L"UNKNOWN_TYPE",
                (int)token->length, token->lexeme ? token->lexeme : L"",
                token->line,
                token->column);

        // Check for termination condition *after* processing/printing
        BaaTokenType token_type = token->type; // Store type before freeing
        baa_free_token(token); // Free the current token

        if (token_type == BAA_TOKEN_EOF || token_type == BAA_TOKEN_ERROR) {
            break; // Exit loop on EOF or ERROR
        }
    }
    wprintf(L"--- End Lexer Tokens ---\n\n");

    // --- Enable Parser --- (Section removed as parser is being removed)
    // Reset lexer to be used by parser
    // Re-initialize the lexer with the same preprocessed source.
    // For simplicity in this debug step, we'll re-initialize.
    // This means the preprocessed 'source' is tokenized twice if parsing proceeds.
    // baa_init_lexer(&lexer, source, wfilename); // No longer needed as parser is removed
    // --- End Lexer Debugging Loop ---

    // Initialize parser (Section removed as parser is being removed)
    // BaaParser parser;
    // baa_init_parser(&parser, &lexer, wfilename); // Initialize the parser with the lexer and filename

    // Parse program (Section removed as parser is being removed)
    // BaaProgram* program = baa_parse_program(&parser); // Use the correct parsing function
    // if (!program) {
    //     // Check if the parser recorded an error message
    //     const wchar_t* parser_error = baa_get_parser_error(&parser);
    //     if (parser_error) {
    //          fwprintf(stderr, L"Error: Parsing failed: %ls\n", parser_error);
    //          // Potentially clear the error if needed: baa_clear_parser_error(&parser);
    //     } else {
    //         fprintf(stderr, "Error: Parsing failed (unknown parser error).\n");
    //     }
    //     free(source);
    //     free(wfilename);
    //     // Note: No need to free 'program' as it's NULL or invalid here
    //      return 1;
    //  }

     // --- AST Printing --- (Section removed as AST is being removed)
     // wprintf(L"\n"); // Add a newline before printing AST
     // baa_print_ast(stdout, program); // Print AST to standard output
     // wprintf(L"\n"); // Add a newline after printing AST
     // --- End AST Printing ---

     // Initialize code generator
     BaaCodeGen codegen;
     BaaCodeGenOptions options;
    options.target = BAA_TARGET_X86_64;
    options.optimize = true;
    options.debug_info = true;

    // Create output filename (replace .ب with .ll)
    size_t wlen = wcslen(wfilename);
    wchar_t* output_filename = (wchar_t*)malloc((wlen + 4) * sizeof(wchar_t)); // .ll is 3 chars + null terminator
    if (!output_filename) {
        fprintf(stderr, "Error: Memory allocation failed for output filename.\n");
        // baa_free_program(program); // Removed as AST is being removed
        free(source);
        free(wfilename);
        return 1;
    }

    // Use secure version wcscpy_s
    errno_t err_cpy = wcscpy_s(output_filename, wlen + 4, wfilename);
    if (err_cpy != 0) {
        fprintf(stderr, "Error: wcscpy_s failed for output filename.\n");
        // baa_free_program(program); // Removed as AST is being removed
        free(source);
        free(wfilename);
        free(output_filename); // Free allocated memory
        return 1;
    }

    wchar_t* ext = wcsrchr(output_filename, L'.');
    if (ext) {
        // Use secure version wcscpy_s
        size_t remaining_size = (wlen + 4) - (ext - output_filename);
        errno_t err_ext_cpy = wcscpy_s(ext, remaining_size, L".ll");
         if (err_ext_cpy != 0) {
            fprintf(stderr, "Error: wcscpy_s failed for extension replacement.\n");
            // baa_free_program(program); // Removed as AST is being removed
            free(source);
            free(wfilename);
            free(output_filename);
            return 1;
        }
    } else {
        // Use secure version wcscat_s
        errno_t err_cat = wcscat_s(output_filename, wlen + 4, L".ll");
        if (err_cat != 0) {
            fprintf(stderr, "Error: wcscat_s failed for appending extension.\n");
            // baa_free_program(program); // Removed as AST is being removed
            free(source);
            free(wfilename);
            free(output_filename);
            return 1;
        }
    }

    options.output_file = output_filename;

    // Initialize code generator
    // baa_init_codegen(&codegen, program, &options); // AST 'program' argument removed
    baa_init_codegen(&codegen, NULL /* program, */, &options);


    // Generate code (This will likely fail or need significant rework without an AST)
    // For now, let's comment it out to get a clean compile.
    // if (!baa_generate_code(&codegen)) {
    //     fprintf(stderr, "Error: Code generation failed: %ls\n", baa_get_codegen_error(&codegen));
    //     free(output_filename);
    //     // baa_free_program(program); // Removed as AST is being removed
    //     free(source);
    //     free(wfilename);
    //     return 1;
    // }

    // wprintf(L"Code generation successful. Output written to %ls\n", output_filename); // Commented out as codegen is disabled

    // Clean up
    // baa_cleanup_codegen(); // Assuming this exists based on init - Called earlier now
    free(output_filename); // Free output_filename as it's not used by a successful codegen path now
    // baa_free_program(program); // Free the AST - Removed as AST is being removed

    // Clean up resources used by preprocessor/lexer
    free(source);
    free(wfilename);

    return 0;
}
