#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

// Include necessary Baa headers
#include "baa/utils/utils.h" // For potential helper like char_to_wchar
#include "baa/preprocessor/preprocessor.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"
#include "baa/ast/ast.h"
#include "baa/ast/ast_printer.h"

// Helper: Convert char* to wchar_t* (copied from compiler.c for now)
static wchar_t *char_to_wchar_tester(const char *str)
{
    size_t len = strlen(str) + 1;
    wchar_t *wstr = malloc(len * sizeof(wchar_t));
    if (!wstr)
    {
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

// Helper function to print wide strings correctly (copied from preprocessor_tester.c)
void print_wide_string_parser_tester(FILE *stream, const wchar_t *wstr)
{
    if (!wstr)
        return;
    if (fwprintf(stream, L"%ls", wstr) < 0)
    {
        fprintf(stderr, "\n[Warning: fwprintf failed.]\n");
    }
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <input_file.baa>\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    wchar_t *w_input_filename = char_to_wchar_tester(input_filename);
    if (!w_input_filename)
    {
        fprintf(stderr, "Error: Failed to convert input filename to wchar_t.\n");
        return 1;
    }

    // 1. Preprocess
    wchar_t *pp_error_message = NULL;
    BaaPpSource pp_source = {
        .type = BAA_PP_SOURCE_FILE,
        .source_name = input_filename,
        .data.file_path = input_filename};
    wchar_t *source_code = baa_preprocess(&pp_source, NULL, &pp_error_message);

    if (!source_code)
    {
        if (pp_error_message)
        {
            fwprintf(stderr, L"Preprocessor Error: %ls\n", pp_error_message);
            free(pp_error_message);
        }
        else
        {
            fwprintf(stderr, L"Preprocessor Error: Failed for file %hs (unknown error).\n", input_filename);
        }
        free(w_input_filename);
        return 1;
    }
    wprintf(L"--- Preprocessing OK ---\n");
    // wprintf(L"%ls\n", source_code); // Optionally print preprocessed code

    // 2. Lex
    BaaLexer lexer;
    // Use w_input_filename for context if needed by lexer/parser errors
    baa_init_lexer(&lexer, source_code, w_input_filename);
    wprintf(L"--- Lexing Initialized ---\n");
    // We don't need to loop through tokens here, the parser will consume them.

    // 3. Parse
    BaaParser parser;
    baa_init_parser(&parser, &lexer); // Initialize the parser with the lexer
    wprintf(L"--- Parsing Started ---\n");
    BaaProgram *program = baa_parse_program(&parser);

    if (!program)
    {
        const wchar_t *parser_error = baa_get_parser_error(&parser);
        if (parser_error)
        {
            fwprintf(stderr, L"Parser Error: %ls\n", parser_error);
        }
        else
        {
            fprintf(stderr, "Parser Error: Parsing failed (unknown parser error).\n");
        }
        free(source_code);
        free(w_input_filename);
        return 1;
    }
    wprintf(L"--- Parsing OK ---\n");

    // 4. Print AST
    wprintf(L"\n--- AST Output ---\n");
    baa_print_ast(stdout, program);
    wprintf(L"\n--- End AST Output ---\n");

    // 5. Cleanup
    baa_free_program(program);
    free(source_code);
    free(w_input_filename);

    wprintf(L"--- Parser Test Completed Successfully ---\n");
    return 0;
}
