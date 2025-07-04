// src/parser/statement_parser.c
#include "statement_parser.h"
#include "expression_parser.h"
#include "parser_internal.h"
#include "parser_utils.h"
#include "baa/ast/ast.h"
#include "baa/lexer/lexer.h"
#include <stdio.h>

BaaNode *parse_expression_statement(BaaParser *parser)
{
    // Parse the expression
    BaaNode *expr = parse_expression(parser);
    if (!expr)
    {
        return NULL; // Error in parsing the expression
    }

    // Create source span from the start of the expression to the current position
    BaaAstSourceSpan span = {
        .start = expr->span.start,
        .end = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column + parser->current_token.length}};

    // Expect and consume the dot terminator
    baa_parser_consume_token(parser, BAA_TOKEN_DOT, L"توقع '.' بعد التعبير في الجملة");

    // Create the expression statement node
    BaaNode *stmt_node = baa_ast_new_expr_stmt_node(span, expr);
    if (!stmt_node)
    {
        // If creating the statement node failed, we need to free the expression
        baa_ast_free_node(expr);
        return NULL;
    }

    return stmt_node;
}

BaaNode *parse_block_statement(BaaParser *parser)
{
    // Create source span starting from the opening brace
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column},
        .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + 1}};

    // Consume the opening brace
    baa_parser_consume_token(parser, BAA_TOKEN_LBRACE, L"توقع '{' لبداية الكتلة");

    // Create the block statement node
    BaaNode *block_node = baa_ast_new_block_stmt_node(span);
    if (!block_node)
    {
        return NULL;
    }

    // Parse statements until we hit the closing brace or EOF
    while (!baa_parser_check_token(parser, BAA_TOKEN_RBRACE) && !baa_parser_check_token(parser, BAA_TOKEN_EOF))
    {
        BaaNode *stmt = parse_statement(parser);
        if (stmt)
        {
            if (!baa_ast_add_stmt_to_block(block_node, stmt))
            {
                // Failed to add statement to block
                baa_ast_free_node(stmt);
                baa_ast_free_node(block_node);
                baa_parser_error_at_token(parser, &parser->current_token,
                                          L"فشل في إضافة الجملة إلى الكتلة");
                return NULL;
            }
        }
        else
        {
            // Error parsing statement - try to recover
            // For now, just break out of the loop
            break;
        }
    }

    // Update the end position of the span
    span.end.line = parser->current_token.line;
    span.end.column = parser->current_token.column + parser->current_token.length;
    block_node->span = span;

    // Consume the closing brace
    baa_parser_consume_token(parser, BAA_TOKEN_RBRACE, L"توقع '}' لنهاية الكتلة");

    return block_node;
}

BaaNode *parse_statement(BaaParser *parser)
{
    // Dispatch based on the current token type
    switch (parser->current_token.type)
    {
    case BAA_TOKEN_LBRACE:
        return parse_block_statement(parser);

        // Add more statement types here as they are implemented
        // case BAA_TOKEN_IF:
        //     return parse_if_statement(parser);
        // case BAA_TOKEN_WHILE:
        //     return parse_while_statement(parser);
        // case BAA_TOKEN_FOR:
        //     return parse_for_statement(parser);
        // case BAA_TOKEN_RETURN:
        //     return parse_return_statement(parser);

    default:
        // Default to expression statement
        return parse_expression_statement(parser);
    }
}
