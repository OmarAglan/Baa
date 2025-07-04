// src/parser/expression_parser.c
#include "expression_parser.h"
#include "parser_internal.h"
#include "parser_utils.h"
#include "baa/ast/ast.h"
#include "baa/lexer/lexer.h"
#include "baa/types/types.h"
#include <stdio.h>

BaaNode *parse_primary_expression(BaaParser *parser)
{
    // Handle literals
    if (parser->current_token.type == BAA_TOKEN_INT_LIT)
    {
        // Extract the integer value from the token
        // For now, we'll use a simple conversion - this should be improved later
        long long value = 0;
        if (parser->current_token.lexeme)
        {
            // Simple conversion - this should use proper number parsing later
            value = wcstoll(parser->current_token.lexeme, NULL, 10);
        }

        // Create source span from current token
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        // Use a default integer type for now - this should be determined properly later
        BaaNode *node = baa_ast_new_literal_int_node(span, value, baa_type_int);
        advance(parser); // Consume the token
        return node;
    }

    if (parser->current_token.type == BAA_TOKEN_STRING_LIT)
    {
        // Create source span from current token
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        // Use the token's lexeme as the string value
        const wchar_t *string_value = parser->current_token.lexeme ? parser->current_token.lexeme : L"";

        // Use a default string type for now - this should be determined properly later
        BaaNode *node = baa_ast_new_literal_string_node(span, string_value, baa_type_string);
        advance(parser); // Consume the token
        return node;
    }

    if (parser->current_token.type == BAA_TOKEN_IDENTIFIER)
    {
        // Create source span from current token
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        // Use the token's lexeme as the identifier name
        const wchar_t *identifier_name = parser->current_token.lexeme ? parser->current_token.lexeme : L"";

        BaaNode *node = baa_ast_new_identifier_expr_node(span, identifier_name);
        advance(parser); // Consume the token
        return node;
    }

    if (parser->current_token.type == BAA_TOKEN_LPAREN)
    {
        advance(parser);                          // Consume '('
        BaaNode *expr = parse_expression(parser); // Recursively parse the expression inside
        if (!expr)
        {
            return NULL; // Error in parsing the inner expression
        }

        consume_token(parser, BAA_TOKEN_RPAREN, L"توقع ')' بعد التعبير");
        return expr; // Return the inner expression (grouping doesn't create a new node)
    }

    // If we reach here, we have an unexpected token
    parser_error_at_token(parser, &parser->current_token,
                          L"توقع تعبير أساسي (رقم، نص، معرف، أو '(')");
    return NULL;
}

BaaNode *parse_expression(BaaParser *parser)
{
    // For now, just delegate to primary expression parsing
    // This will be expanded to handle operator precedence levels
    return parse_primary_expression(parser);
}
