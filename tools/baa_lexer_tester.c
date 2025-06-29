// tools/baa_lexer_tester.c
#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h" // For baa_file_content if reading from file, and baa_strdup
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <stdlib.h> // For free

#ifdef _WIN32
#include <windows.h> // For GetCommandLineW, CommandLineToArgvW, LocalFree
#endif

// Helper function to print wide strings correctly
void print_wide_string_tester(FILE *stream, const wchar_t *wstr)
{
    if (!wstr)
    {
        fwprintf(stream, L"(null_lexeme)"); // Indicate if lexeme is NULL
        return;
    }
    if (fwprintf(stream, L"%ls", wstr) < 0)
    {
        // Fallback for console - avoid adding extra newlines if fwprintf fails
        while (*wstr)
        {
            if (putwc(*wstr, stream) == WEOF)
                break;
            wstr++;
        }
    }
}

void print_token_for_tester(const BaaToken *token, int count)
{
    if (!token)
    {
        fwprintf(stderr, L"Token %03d: NULL TOKEN (Critical Lexer Error)\n", count);
        return;
    }
    const wchar_t *type_str = baa_token_type_to_string(token->type);
    // Simplified single-line output
    wprintf(L"[%03d] %-30ls (L%zu C%zu Len%zu) '",
            count, type_str ? type_str : L"UNKNOWN_TYPE_STR",
            token->line, token->column, token->length);
    print_wide_string_tester(stdout, token->lexeme);
    wprintf(L"' [Hex: ");
    if (token->lexeme) {
        for (size_t i = 0; i < token->length; ++i) {
            wprintf(L"%04X ", (unsigned int)token->lexeme[i]);
        }
    }
    wprintf(L"]\n");

    // Print enhanced error information if available
    if (token->error) {
        wprintf(L"      ERROR INFO:\n");
        wprintf(L"        Code: %u\n", token->error->error_code);
        wprintf(L"        Category: %hs\n", token->error->category ? token->error->category : "(null)");
        if (token->error->suggestion) {
            wprintf(L"        Suggestion: ");
            print_wide_string_tester(stdout, token->error->suggestion);
            wprintf(L"\n");
        }
        if (token->error->context_before) {
            wprintf(L"        Context Before: ");
            print_wide_string_tester(stdout, token->error->context_before);
            wprintf(L"\n");
        }
        if (token->error->context_after) {
            wprintf(L"        Context After: ");
            print_wide_string_tester(stdout, token->error->context_after);
            wprintf(L"\n");
        }
    }
}

int main(int argc, char *argv_char[])
{
    setlocale(LC_ALL, "");

    const wchar_t *source_to_lex = NULL;
    wchar_t *file_content_buffer = NULL; // To hold content if read from file
    const wchar_t *source_name_for_lexer = L"<string_input>";

#ifdef _WIN32
    int nArgs;
    LPWSTR *szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (nArgs > 1)
    {
        source_name_for_lexer = baa_strdup(szArgList[1]);     // For lexer init
        file_content_buffer = baa_file_content(szArgList[1]); // Read file content
        if (!file_content_buffer)
        {
            fwprintf(stderr, L"Error: Could not read file: ");
            print_wide_string_tester(stderr, szArgList[1]);
            fwprintf(stderr, L"\n");
            if (source_name_for_lexer)
                baa_free((void *)source_name_for_lexer);
            LocalFree(szArgList);
            return 1;
        }
        source_to_lex = file_content_buffer;
    }
    if (szArgList)
        LocalFree(szArgList);
#else
    if (argc > 1)
    {
        // Convert char* filename to wchar_t* for baa_file_content and lexer init
        size_t len_needed = mbstowcs(NULL, argv_char[1], 0);
        if (len_needed == (size_t)-1)
        {
            fprintf(stderr, "Error: Invalid multibyte sequence in filename.\n");
            return 1;
        }
        wchar_t *w_filename = (wchar_t *)malloc((len_needed + 1) * sizeof(wchar_t));
        if (!w_filename)
        {
            fprintf(stderr, "Error: Memory allocation failed for wide filename.\n");
            return 1;
        }
        mbstowcs(w_filename, argv_char[1], len_needed + 1);

        source_name_for_lexer = baa_strdup(w_filename);     // For lexer init
        file_content_buffer = baa_file_content(w_filename); // Read file
        free(w_filename);

        if (!file_content_buffer)
        {
            fwprintf(stderr, L"Error: Could not read file: %hs\n", argv_char[1]);
            if (source_name_for_lexer)
                baa_free((void *)source_name_for_lexer);
            return 1;
        }
        source_to_lex = file_content_buffer;
    }
#endif

    if (!source_to_lex)
    {
        // Default to a simple test string if no file is provided
        // This string includes error cases to test enhanced error handling
        static const wchar_t default_source[] = L"\"unterminated string\n\"invalid\\q escape\"\n'unterminated\n0x\n/* unterminated comment";
        source_to_lex = default_source;
        source_name_for_lexer = L"<default_test_string>";
        wprintf(L"No input file provided. Using default test string:\n\"");
        print_wide_string_tester(stdout, source_to_lex);
        wprintf(L"\"\n\n");
    }
    else
    {
        wprintf(L"Lexing file: ");
        print_wide_string_tester(stdout, source_name_for_lexer);
        wprintf(L"\nContent:\n\"");
        print_wide_string_tester(stdout, source_to_lex);
        wprintf(L"\"\n\n");
    }

    BaaLexer lexer;
    // Pass source_name_for_lexer (which is wchar_t*) to baa_init_lexer
    baa_init_lexer(&lexer, source_to_lex, source_name_for_lexer);

    wprintf(L"--- Lexer Tokens ---\n");
    int token_count = 0;
    BaaToken *token;

    do
    {
        token = baa_lexer_next_token(&lexer);
        print_token_for_tester(token, token_count++);

        BaaTokenType current_type = BAA_TOKEN_UNKNOWN; // Default if token is NULL
        if (token)
        {
            current_type = token->type;
            baa_free_token(token); // Free after use
        }
        else
        {
            break; // Critical error from lexer
        }

        if (current_type == BAA_TOKEN_EOF || current_type == BAA_TOKEN_ERROR)
        {
            break;
        }
    } while (true);

    wprintf(L"--- End Lexer Tokens ---\n");

    if (file_content_buffer)
    {
        free(file_content_buffer);
    }
    // If source_name_for_lexer was strdup'd (for file input case)
    if (source_name_for_lexer != L"<string_input>" && source_name_for_lexer != L"<default_test_string>")
    {
        baa_free((void *)source_name_for_lexer);
    }

    return 0;
}
