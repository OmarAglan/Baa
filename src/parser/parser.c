// src/parser/parser.c
#include "baa/parser/parser.h"
#include "parser_internal.h" // For BaaParser struct definition
#include "parser_utils.h"    // For parser utility function declarations
#include "statement_parser.h" // For parse_statement
#include "declaration_parser.h" // For parse_declaration_or_statement
#include "baa/utils/utils.h"  // For baa_malloc, baa_free
#include "baa/lexer/lexer.h"  // For baa_lexer_next_token, baa_free_token
#include "baa/ast/ast.h"     // For AST node creation functions
#include <stdio.h>            // For error printing (temporary), fwprintf, vfwprintf
#include <stdarg.h>           // For va_list, va_start, va_end
#include <stdlib.h>           // For NULL

// Forward declarations are now in parser_utils.h (baa_parser_*)

/**
 * @brief Attempts to recover from a syntax error by discarding tokens.
 *
 * After an error is reported and the parser enters panic mode, this function
 * advances through tokens until it finds a token that likely marks the
 * beginning of a new statement or declaration, or until EOF is reached.
 * This helps prevent cascading error messages.
 *
 * Current synchronization points:
 * - Statement terminator (`.`)
 * - Keywords that typically start a new declaration or statement.
 * - End of file (`BAA_TOKEN_EOF`).
 *
 * Once a synchronization point is found (or EOF), panic mode is cleared.
 *
 * @param parser Pointer to the BaaParser instance.
 */
void baa_parser_synchronize(BaaParser *parser)
{
    if (!parser)
        return;

    // parser->panic_mode = false; // Original: Clear panic mode *before* consuming
    // Let's try clearing it *after* finding a recovery point or at the start
    // IF an error just occurred.
    // For this function, it's assumed an error *has* occurred and we are *already* in panic_mode.
    // The goal is to find a safe place and then clear it.

    while (parser->current_token.type != BAA_TOKEN_EOF)
    {
        // 1. Check if the *previous* token was a statement terminator.
        //    If so, the current token is likely the start of a new statement.
        if (parser->previous_token.type == BAA_TOKEN_DOT)
        {
            parser->panic_mode = false; // Cleared on recovery
            return;
        }

        // 2. Check if the *current* token is a keyword that likely starts a new declaration or major statement.
        switch (parser->current_token.type)
        {
        // Declaration-starting keywords (type keywords)
        case BAA_TOKEN_TYPE_INT:
        case BAA_TOKEN_TYPE_FLOAT:
        case BAA_TOKEN_TYPE_CHAR:
        case BAA_TOKEN_TYPE_VOID:
        case BAA_TOKEN_TYPE_BOOL:
        // Statement-starting keywords
        case BAA_TOKEN_CONST: // `ثابت` can start a declaration
        case BAA_TOKEN_IF:
        case BAA_TOKEN_WHILE:
        case BAA_TOKEN_FOR:
        case BAA_TOKEN_RETURN:
        case BAA_TOKEN_SWITCH:          // If/when switch is added
                                        // case BAA_TOKEN_FUNC: // If `دالة` keyword was kept
            parser->panic_mode = false; // Cleared on recovery
            return;
        default:
            // Not a recovery point based on current token type
            break;
        }
        baa_parser_advance(parser); // Consume the token and try the next one
    }
    // If EOF is reached, panic mode is implicitly over.
    parser->panic_mode = false; // Cleared at EOF
}

/**
 * @brief Reports a parser error at the location of a specific token.
 * Sets the parser's had_error flag and enters panic mode if not already in it.
 * Avoids printing duplicate errors if already in panic mode.
 *
 * @param parser Pointer to the BaaParser instance.
 * @param token Pointer to the BaaToken at which the error occurred.
 * @param message_format A printf-style format string for the error message.
 * @param ... Additional arguments for the format string.
 */
void baa_parser_error_at_token(BaaParser *parser, const BaaToken *token, const wchar_t *message_format, ...)
{
    if (!parser || parser->panic_mode)
    { // Don't report if already panicking
        return;
    }
    parser->panic_mode = true; // Enter panic mode
    parser->had_error = true;

    // Construct the error message prefix with location
    // Using source_filename stored in parser struct
    fwprintf(stderr, L"%ls:%zu:%zu: خطأ: ",
             parser->source_filename ? parser->source_filename : L"<unknown_source>",
             token->line,
             token->column);

    va_list args;
    va_start(args, message_format);
    vfwprintf(stderr, message_format, args);
    va_end(args);

    fwprintf(stderr, L"\n");
}

/**
 * @brief Checks if the current token's type matches the expected type.
 * Does not consume the token.
 *
 * @param parser Pointer to the BaaParser instance.
 * @param type The expected BaaTokenType.
 * @return True if the current token's type matches, false otherwise.
 */
bool baa_parser_check_token(BaaParser *parser, BaaTokenType type)
{
    if (!parser)
        return false;
    return parser->current_token.type == type;
}

/**
 * @brief If the current token's type matches the expected type, consumes it
 * and returns true. Otherwise, returns false and does not consume the token.
 *
 * @param parser Pointer to the BaaParser instance.
 * @param type The expected BaaTokenType to match and consume.
 * @return True if the token was matched and consumed, false otherwise.
 */
bool baa_parser_match_token(BaaParser *parser, BaaTokenType type)
{
    if (!parser)
        return false;
    if (!baa_parser_check_token(parser, type))
    {
        return false;
    }
    baa_parser_advance(parser);
    return true;
}

/**
 * @brief Consumes the current token if its type matches the expected type.
 * If the type does not match, an error is reported, and error recovery (synchronization)
 * might be attempted by the caller or a higher-level parsing function.
 *
 * @param parser Pointer to the BaaParser instance.
 * @param expected_type The BaaTokenType that is expected.
 * @param error_message_format A printf-style format string for the error message if the token does not match.
 * @param ... Additional arguments for the error message format string.
 */
void baa_parser_consume_token(BaaParser *parser, BaaTokenType expected_type, const wchar_t *error_message_format, ...)
{
    if (!parser)
        return;

    if (parser->current_token.type == expected_type)
    {
        baa_parser_advance(parser);
        return;
    }

    // Token doesn't match, report error using varargs for the message
    va_list args;
    va_start(args, error_message_format);
    // Create a temporary buffer for the formatted message body
    wchar_t message_body[256]; // Adjust size as needed or use dynamic allocation
    vswprintf(message_body, sizeof(message_body) / sizeof(wchar_t), error_message_format, args);
    va_end(args);

    // Report the error using the already formatted message_body
    baa_parser_error_at_token(parser, &parser->current_token, L"%ls", message_body);
    // Note: synchronize() is typically called by the parsing rule that detects an unrecoverable state,
    // not directly by consume_token itself, to allow the rule to decide if it can recover differently.
}

BaaParser *baa_parser_create(BaaLexer *lexer, const wchar_t *source_filename)
{
    if (!lexer)
    {
        // Consider logging this error more formally later
        fprintf(stderr, "Parser Error: Cannot create parser with NULL lexer.\n");
        return NULL;
    }

    BaaParser *parser = (BaaParser *)baa_malloc(sizeof(BaaParser));
    if (!parser)
    {
        fprintf(stderr, "Parser Error: Failed to allocate memory for parser.\n");
        return NULL;
    }

    parser->lexer = lexer;
    parser->source_filename = source_filename; // Store filename
    parser->had_error = false;
    parser->panic_mode = false;

    // Initialize tokens to a known state (e.g., EOF or an UNKNOWN type)
    // The lexemes should be NULL initially so 'advance' doesn't try to free garbage.
    parser->current_token.type = BAA_TOKEN_UNKNOWN; // Or some initial sentinel
    parser->current_token.lexeme = NULL;
    parser->current_token.length = 0;
    parser->current_token.line = 0;
    parser->current_token.column = 0;

    parser->previous_token.type = BAA_TOKEN_UNKNOWN;
    parser->previous_token.lexeme = NULL;
    parser->previous_token.length = 0;
    parser->previous_token.line = 0;
    parser->previous_token.column = 0;

    // Prime the pump: Fetch the first token to be current_token.
    // previous_token will remain in its initial state after this first advance.
    baa_parser_advance(parser);

    // If the very first token is an error from the lexer, report it.
    // (The advance function itself will loop to get a non-error token or EOF)
    // This initial 'advance' might set parser->had_error if lexer immediately fails hard.

    return parser;
}

/**
 * @brief Consumes the current token and fetches the next one from the lexer.
 *
 * Skips over lexical error tokens, reporting them and continuing to the next
 * valid token or EOF. Manages lexeme ownership for current_token and previous_token.
 */
void baa_parser_advance(BaaParser *parser)
{
    // 1. Free the lexeme of the *old* previous_token (the one about to be overwritten)
    if (parser->previous_token.lexeme != NULL)
    {
        baa_free((void *)parser->previous_token.lexeme); // Cast to void* for baa_free
        parser->previous_token.lexeme = NULL;
    }

    // 2. Shift current_token to previous_token
    parser->previous_token = parser->current_token; // Struct copy. previous_token now "owns" the lexeme
                                                    // that current_token previously owned.

    // 3. Fetch the next token from the lexer until it's not a BAA_TOKEN_ERROR or we hit EOF.
    for (;;)
    {
        BaaToken *lexer_next_token_ptr = baa_lexer_next_token(parser->lexer);
        if (!lexer_next_token_ptr)
        {
            // This indicates a critical failure in the lexer (e.g., malloc failed for token)
            // Report a parser-level error and set current_token to EOF to stop.
            baa_parser_error_at_token(parser, &parser->current_token, L"فشل معجمي حرج: لم يتم إرجاع رمز مميز.");
            parser->had_error = true; // Signal a general error
            parser->current_token.type = BAA_TOKEN_EOF;
            parser->current_token.lexeme = NULL; // No lexeme for this synthetic EOF
            parser->current_token.length = 0;
            parser->current_token.line = parser->previous_token.line; // Approximate location
            parser->current_token.column = parser->previous_token.column;
            // No need to free lexer_next_token_ptr as it's NULL
            return;
        }
        // --- DEBUG PRINT: Token received from lexer IN THIS ITERATION ---
        if (lexer_next_token_ptr)
        {
            fwprintf(stderr, L"DEBUG PARSER advance - Lexer Returned: Type=%d (%ls), Lexeme='", lexer_next_token_ptr->type, baa_token_type_to_string(lexer_next_token_ptr->type));
            for (size_t i = 0; i < lexer_next_token_ptr->length; ++i)
            {
                putwc(lexer_next_token_ptr->lexeme[i], stderr);
            }
            fwprintf(stderr, L"', Line=%zu, Col=%zu\n", lexer_next_token_ptr->line, lexer_next_token_ptr->column);
            fflush(stderr);
        }
        // --- END DEBUG PRINT ---

        // --- DEBUG PRINT before assignment ---
        if (lexer_next_token_ptr)
        {
            fwprintf(stderr, L"DEBUG PARSER advance - Lexer Token Before Assign: Type=%d, Lexeme='", lexer_next_token_ptr->type);
            for (size_t i = 0; i < lexer_next_token_ptr->length; ++i)
            {
                putwc(lexer_next_token_ptr->lexeme[i], stderr);
            }
            fwprintf(stderr, L"', Len=%zu, Line=%zu, Col=%zu\n",
                     lexer_next_token_ptr->length, lexer_next_token_ptr->line, lexer_next_token_ptr->column);
        }
        // --- END DEBUG PRINT ---

        // 4. Copy the new token's data to parser->current_token
        parser->current_token = *lexer_next_token_ptr; // Struct copy. current_token now "owns" the lexeme.

        // 5. Sever ownership from the temporary token struct returned by the lexer
        lexer_next_token_ptr->lexeme = NULL;

        // 6. Free the shell of the token returned by the lexer
        baa_free_token(lexer_next_token_ptr);

        // -- -DEBUG PRINT after assignment-- -
        fwprintf(stderr, L"DEBUG PARSER advance - Parser Current Token After Assign: Type=%d, Lexeme='", parser->current_token.type);
        for (size_t i = 0; i < parser->current_token.length; ++i)
        {
            putwc(parser->current_token.lexeme[i], stderr);
        }
        fwprintf(stderr, L"', Len=%zu, Line=%zu, Col=%zu\n",
                 parser->current_token.length, parser->current_token.line, parser->current_token.column);
        // --- END DEBUG PRINT ---

        if (parser->current_token.type != BAA_TOKEN_ERROR)
        {
            break; // Got a valid token or EOF
        }

        // Lexical error encountered. We report it here because 'advance' is responsible for
        // dealing with tokens from the lexer.
        baa_parser_error_at_token(parser, &parser->current_token, L"خطأ معجمي: %ls",
                                  parser->current_token.lexeme ? parser->current_token.lexeme : L"Unknown lexical error");
        // parser->had_error is already set by parser_error_at_token
        // The loop continues to fetch the next token after a lexical error.
        // The erroneous token's lexeme in current_token will be freed on the next advance.
    }
}

BaaNode *baa_parse_program(BaaParser *parser)
{
    if (!parser)
    {
        return NULL;
    }

    // Create source span for the entire program
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column},
        .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column}};

    // Create the program node
    BaaNode *program_node = baa_ast_new_program_node(span);
    if (!program_node)
    {
        baa_parser_error_at_token(parser, &parser->current_token,
                                  L"فشل في إنشاء عقدة البرنامج");
        return NULL;
    }

    // Parse top-level constructs until EOF
    while (!baa_parser_check_token(parser, BAA_TOKEN_EOF))
    {
        // Parse declarations (functions, variables) and statements
        BaaNode *declaration = parse_declaration_or_statement(parser);

        if (declaration)
        {
            if (!baa_ast_add_declaration_to_program(program_node, declaration))
            {
                // Failed to add declaration to program
                baa_ast_free_node(declaration);
                baa_ast_free_node(program_node);
                baa_parser_error_at_token(parser, &parser->current_token,
                                          L"فشل في إضافة الإعلان إلى البرنامج");
                return NULL;
            }
        }
        else
        {
            // Error parsing declaration - try to recover
            if (parser->panic_mode)
            {
                baa_parser_synchronize(parser);
            }
            else
            {
                // If we're not in panic mode but got a NULL declaration,
                // something went wrong - break to avoid infinite loop
                break;
            }
        }
    }

    // Update the end position of the program span
    span.end.line = parser->current_token.line;
    span.end.column = parser->current_token.column;
    program_node->span = span;

    return program_node;
}

/**
 * @brief Frees the resources associated with the parser.
 *
 * This includes freeing the lexemes of any currently held tokens (`current_token`
 * and `previous_token`) and then freeing the BaaParser structure itself.
 * It does NOT free the lexer instance that was passed during parser creation.
 *
 * @param parser A pointer to the BaaParser instance to be freed. If NULL,
 *               the function does nothing.
 */
void baa_parser_free(BaaParser *parser)
{
    if (!parser)
    {
        return;
    }

    // Free the lexeme of the current_token if it exists
    if (parser->current_token.lexeme != NULL)
    {
        baa_free((void *)parser->current_token.lexeme);
    }
    // Free the lexeme of the previous_token if it exists
    if (parser->previous_token.lexeme != NULL)
    {
        baa_free((void *)parser->previous_token.lexeme);
    }
    baa_free(parser);
}

// Implementation for baa_parser_had_error
bool baa_parser_had_error(const BaaParser *parser)
{
    if (!parser)
        return true; // Or false, depending on desired behavior for NULL parser
    return parser->had_error;
}
