#include "type_parser.h"
#include "expression_parser.h"
#include "parser_internal.h"
#include "parser_utils.h"
#include "baa/ast/ast.h"
#include "baa/lexer/lexer.h"
#include <stdio.h>

/**
 * @file type_parser.c
 * @brief Implementation of type specifier parsing functions.
 *
 * This file implements functions for parsing type specifications including
 * primitive types and array types.
 */

// Helper function to convert type token to type name string
static const wchar_t *token_to_type_name(BaaTokenType token_type)
{
    switch (token_type)
    {
    case BAA_TOKEN_TYPE_INT:
        return L"عدد_صحيح";
    case BAA_TOKEN_TYPE_FLOAT:
        return L"عدد_حقيقي";
    case BAA_TOKEN_TYPE_CHAR:
        return L"حرف";
    case BAA_TOKEN_TYPE_VOID:
        return L"فراغ";
    case BAA_TOKEN_TYPE_BOOL:
        return L"منطقي";
    default:
        return L""; // Should not happen for valid type tokens
    }
}

BaaNode *parse_type_specifier(BaaParser *parser)
{
    // Check if current token is a primitive type
    if (baa_token_is_type(parser->current_token.type))
    {
        // Create source span for the type token
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        // Get the type name
        const wchar_t *type_name = token_to_type_name(parser->current_token.type);

        // Advance past the type token
        baa_parser_advance(parser);

        // Create primitive type node
        BaaNode *type_node = baa_ast_new_primitive_type_node(span, type_name);
        if (!type_node)
        {
            return NULL;
        }

        // Check for array syntax: type[size] or type[]
        if (parser->current_token.type == BAA_TOKEN_LBRACKET)
        {
            baa_parser_advance(parser); // Consume '['

            BaaNode *size_expr = NULL;

            // Check if there's a size expression
            if (parser->current_token.type != BAA_TOKEN_RBRACKET)
            {
                // Parse the size expression
                size_expr = parse_expression(parser);
                if (!size_expr)
                {
                    baa_ast_free_node(type_node);
                    return NULL;
                }
            }

            // Consume the closing ']'
            baa_parser_consume_token(parser, BAA_TOKEN_RBRACKET, L"توقع ']' بعد حجم المصفوفة");

            // Update span to include the array brackets
            BaaAstSourceSpan array_span = {
                .start = span.start,
                .end = {
                    .filename = parser->source_filename,
                    .line = parser->current_token.line,
                    .column = parser->current_token.column}};

            // Create array type node
            BaaNode *array_type_node = baa_ast_new_array_type_node(array_span, type_node, size_expr);
            if (!array_type_node)
            {
                baa_ast_free_node(type_node);
                if (size_expr)
                {
                    baa_ast_free_node(size_expr);
                }
                return NULL;
            }

            return array_type_node;
        }

        // Just a primitive type
        return type_node;
    }

    // Not a valid type specifier
    baa_parser_error_at_token(parser, &parser->current_token,
                              L"توقع مُحدد نوع (عدد_صحيح، عدد_حقيقي، حرف، فراغ، أو منطقي)");
    return NULL;
}
