// src/parser/statement_parser.c
#include "statement_parser.h"
#include "expression_parser.h"
#include "declaration_parser.h"
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

// Helper function to check if current token could start a declaration
static bool could_start_declaration(BaaParser *parser)
{
    // Check for modifiers or type keywords
    return parser->current_token.type == BAA_TOKEN_CONST ||
           parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE ||
           parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT ||
           baa_token_is_type(parser->current_token.type);
}

BaaNode *parse_statement(BaaParser *parser)
{
    // Check if this could be a declaration first
    if (could_start_declaration(parser))
    {
        return parse_variable_declaration_statement(parser, BAA_MOD_NONE);
    }

    // Dispatch based on the current token type for statements
    switch (parser->current_token.type)
    {
    case BAA_TOKEN_LBRACE:
        return parse_block_statement(parser);

    case BAA_TOKEN_IF:
        return parse_if_statement(parser);

    case BAA_TOKEN_WHILE:
        return parse_while_statement(parser);

    case BAA_TOKEN_FOR:
        return parse_for_statement(parser);

    case BAA_TOKEN_RETURN:
        return parse_return_statement(parser);

    case BAA_TOKEN_BREAK:
        return parse_break_statement(parser);

    case BAA_TOKEN_CONTINUE:
        return parse_continue_statement(parser);

    default:
        // Default to expression statement
        return parse_expression_statement(parser);
    }
}

// --- Control Flow Statement Parsing Functions ---

// --- If Statement Parsing ---

BaaNode *parse_if_statement(BaaParser *parser)
{
    // Create source span starting from the 'إذا' keyword
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column
        }
    };

    // Consume the 'إذا' keyword
    baa_parser_consume_token(parser, BAA_TOKEN_IF, L"Expected 'إذا' keyword");

    // Consume the opening parenthesis
    baa_parser_consume_token(parser, BAA_TOKEN_LPAREN, L"Expected '(' after 'إذا'");

    // Parse the condition expression
    BaaNode *condition_expr = parse_expression(parser);
    if (!condition_expr)
    {
        return NULL; // Error in parsing the condition
    }

    // Consume the closing parenthesis
    baa_parser_consume_token(parser, BAA_TOKEN_RPAREN, L"Expected ')' after condition");

    // Parse the then statement
    BaaNode *then_stmt = parse_statement(parser);
    if (!then_stmt)
    {
        baa_ast_free_node(condition_expr);
        return NULL; // Error in parsing the then statement
    }

    // Check for optional else clause
    BaaNode *else_stmt = NULL;
    if (baa_parser_match_token(parser, BAA_TOKEN_ELSE))
    {
        else_stmt = parse_statement(parser);
        if (!else_stmt)
        {
            baa_ast_free_node(condition_expr);
            baa_ast_free_node(then_stmt);
            return NULL; // Error in parsing the else statement
        }
    }

    // Update the span end location
    span.end.filename = parser->source_filename;
    span.end.line = parser->previous_token.line;
    span.end.column = parser->previous_token.column + parser->previous_token.length;

    // Create the if statement node
    return baa_ast_new_if_stmt_node(span, condition_expr, then_stmt, else_stmt);
}

// --- While Statement Parsing ---

BaaNode *parse_while_statement(BaaParser *parser)
{
    // Create source span starting from the 'طالما' keyword
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column
        }
    };

    // Consume the 'طالما' keyword
    baa_parser_consume_token(parser, BAA_TOKEN_WHILE, L"Expected 'طالما' keyword");

    // Consume the opening parenthesis
    baa_parser_consume_token(parser, BAA_TOKEN_LPAREN, L"Expected '(' after 'طالما'");

    // Parse the condition expression
    BaaNode *condition_expr = parse_expression(parser);
    if (!condition_expr)
    {
        return NULL; // Error in parsing the condition
    }

    // Consume the closing parenthesis
    baa_parser_consume_token(parser, BAA_TOKEN_RPAREN, L"Expected ')' after condition");

    // Parse the body statement
    BaaNode *body_stmt = parse_statement(parser);
    if (!body_stmt)
    {
        baa_ast_free_node(condition_expr);
        return NULL; // Error in parsing the body statement
    }

    // Update the span end location
    span.end.filename = parser->source_filename;
    span.end.line = parser->previous_token.line;
    span.end.column = parser->previous_token.column + parser->previous_token.length;

    // Create the while statement node
    return baa_ast_new_while_stmt_node(span, condition_expr, body_stmt);
}

// --- For Statement Parsing ---

BaaNode *parse_for_statement(BaaParser *parser)
{
    // Create source span starting from the 'لكل' keyword
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column
        }
    };

    // Consume the 'لكل' keyword
    baa_parser_consume_token(parser, BAA_TOKEN_FOR, L"Expected 'لكل' keyword");

    // Consume the opening parenthesis
    baa_parser_consume_token(parser, BAA_TOKEN_LPAREN, L"Expected '(' after 'لكل'");

    // Parse the initializer (can be a variable declaration or expression statement)
    BaaNode *initializer_stmt = NULL;
    if (!baa_parser_check_token(parser, BAA_TOKEN_SEMICOLON))
    {
        if (could_start_declaration(parser))
        {
            initializer_stmt = parse_variable_declaration_statement(parser, BAA_MOD_NONE);
        }
        else
        {
            // Parse as expression statement but don't expect the dot terminator
            BaaNode *expr = parse_expression(parser);
            if (expr)
            {
                BaaAstSourceSpan expr_span = {
                    .start = {
                        .filename = parser->source_filename,
                        .line = expr->span.start.line,
                        .column = expr->span.start.column
                    },
                    .end = {
                        .filename = parser->source_filename,
                        .line = parser->previous_token.line,
                        .column = parser->previous_token.column + parser->previous_token.length
                    }
                };
                initializer_stmt = baa_ast_new_expr_stmt_node(expr_span, expr);
            }
        }
    }

    // Consume the first semicolon
    baa_parser_consume_token(parser, BAA_TOKEN_SEMICOLON, L"Expected ';' after for loop initializer");

    // Parse the condition expression (optional)
    BaaNode *condition_expr = NULL;
    if (!baa_parser_check_token(parser, BAA_TOKEN_SEMICOLON))
    {
        condition_expr = parse_expression(parser);
        if (!condition_expr && initializer_stmt)
        {
            baa_ast_free_node(initializer_stmt);
            return NULL;
        }
    }

    // Consume the second semicolon
    baa_parser_consume_token(parser, BAA_TOKEN_SEMICOLON, L"Expected ';' after for loop condition");

    // Parse the increment expression (optional)
    BaaNode *increment_expr = NULL;
    if (!baa_parser_check_token(parser, BAA_TOKEN_RPAREN))
    {
        increment_expr = parse_expression(parser);
        if (!increment_expr)
        {
            if (initializer_stmt) baa_ast_free_node(initializer_stmt);
            if (condition_expr) baa_ast_free_node(condition_expr);
            return NULL;
        }
    }

    // Consume the closing parenthesis
    baa_parser_consume_token(parser, BAA_TOKEN_RPAREN, L"Expected ')' after for loop header");

    // Parse the body statement
    BaaNode *body_stmt = parse_statement(parser);
    if (!body_stmt)
    {
        if (initializer_stmt) baa_ast_free_node(initializer_stmt);
        if (condition_expr) baa_ast_free_node(condition_expr);
        if (increment_expr) baa_ast_free_node(increment_expr);
        return NULL;
    }

    // Update the span end location
    span.end.filename = parser->source_filename;
    span.end.line = parser->previous_token.line;
    span.end.column = parser->previous_token.column + parser->previous_token.length;

    // Create the for statement node
    return baa_ast_new_for_stmt_node(span, initializer_stmt, condition_expr, increment_expr, body_stmt);
}

// --- Return Statement Parsing ---

BaaNode *parse_return_statement(BaaParser *parser)
{
    // Create source span starting from the 'إرجع' keyword
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column
        }
    };

    // Consume the 'إرجع' keyword
    baa_parser_consume_token(parser, BAA_TOKEN_RETURN, L"Expected 'إرجع' keyword");

    // Parse optional return value expression
    BaaNode *value_expr = NULL;
    if (!baa_parser_check_token(parser, BAA_TOKEN_DOT))
    {
        value_expr = parse_expression(parser);
        if (!value_expr)
        {
            return NULL; // Error in parsing the return value
        }
    }

    // Consume the dot terminator
    baa_parser_consume_token(parser, BAA_TOKEN_DOT, L"Expected '.' after return statement");

    // Update the span end location
    span.end.filename = parser->source_filename;
    span.end.line = parser->previous_token.line;
    span.end.column = parser->previous_token.column + parser->previous_token.length;

    // Create the return statement node
    return baa_ast_new_return_stmt_node(span, value_expr);
}

// --- Break Statement Parsing ---

BaaNode *parse_break_statement(BaaParser *parser)
{
    // Create source span starting from the 'توقف' keyword
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column
        }
    };

    // Consume the 'توقف' keyword
    baa_parser_consume_token(parser, BAA_TOKEN_BREAK, L"Expected 'توقف' keyword");

    // Consume the dot terminator
    baa_parser_consume_token(parser, BAA_TOKEN_DOT, L"Expected '.' after break statement");

    // Update the span end location
    span.end.filename = parser->source_filename;
    span.end.line = parser->previous_token.line;
    span.end.column = parser->previous_token.column + parser->previous_token.length;

    // Create the break statement node
    return baa_ast_new_break_stmt_node(span);
}

// --- Continue Statement Parsing ---

BaaNode *parse_continue_statement(BaaParser *parser)
{
    // Create source span starting from the 'استمر' keyword
    BaaAstSourceSpan span = {
        .start = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column
        }
    };

    // Consume the 'استمر' keyword
    baa_parser_consume_token(parser, BAA_TOKEN_CONTINUE, L"Expected 'استمر' keyword");

    // Consume the dot terminator
    baa_parser_consume_token(parser, BAA_TOKEN_DOT, L"Expected '.' after continue statement");

    // Update the span end location
    span.end.filename = parser->source_filename;
    span.end.line = parser->previous_token.line;
    span.end.column = parser->previous_token.column + parser->previous_token.length;

    // Create the continue statement node
    return baa_ast_new_continue_stmt_node(span);
}
