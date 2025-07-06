// src/parser/expression_parser.c
#include "expression_parser.h"
#include "parser_internal.h"
#include "parser_utils.h"
#include "baa/ast/ast.h"
#include "baa/lexer/lexer.h"
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

        // Use NULL type for now - this should be determined properly later
        BaaNode *node = baa_ast_new_literal_int_node(span, value, NULL);
        baa_parser_advance(parser); // Consume the token
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

        // Use NULL type for now - this should be determined properly later
        BaaNode *node = baa_ast_new_literal_string_node(span, string_value, NULL);
        baa_parser_advance(parser); // Consume the token
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
        baa_parser_advance(parser); // Consume the token
        return node;
    }

    if (parser->current_token.type == BAA_TOKEN_LPAREN)
    {
        baa_parser_advance(parser);               // Consume '('
        BaaNode *expr = parse_expression(parser); // Recursively parse the expression inside
        if (!expr)
        {
            return NULL; // Error in parsing the inner expression
        }

        baa_parser_consume_token(parser, BAA_TOKEN_RPAREN, L"توقع ')' بعد التعبير");
        return expr; // Return the inner expression (grouping doesn't create a new node)
    }

    // If we reach here, we have an unexpected token
    baa_parser_error_at_token(parser, &parser->current_token,
                              L"توقع تعبير أساسي (رقم، نص، معرف، أو '(')");
    return NULL;
}

// Helper function to get precedence of a token type
static int get_token_precedence(BaaTokenType token_type)
{
    switch (token_type)
    {
    // Logical OR (lowest precedence)
    case BAA_TOKEN_OR:
        return 10;

    // Logical AND
    case BAA_TOKEN_AND:
        return 20;

    // Equality operators
    case BAA_TOKEN_EQUAL_EQUAL:
    case BAA_TOKEN_BANG_EQUAL:
        return 30;

    // Comparison operators
    case BAA_TOKEN_LESS:
    case BAA_TOKEN_LESS_EQUAL:
    case BAA_TOKEN_GREATER:
    case BAA_TOKEN_GREATER_EQUAL:
        return 40;

    // Additive operators
    case BAA_TOKEN_PLUS:
    case BAA_TOKEN_MINUS:
        return 50;

    // Multiplicative operators (highest precedence)
    case BAA_TOKEN_STAR:
    case BAA_TOKEN_SLASH:
    case BAA_TOKEN_PERCENT:
        return 60;

    default:
        return -1; // Not a binary operator
    }
}

// Helper function to convert token type to binary operator kind
static BaaBinaryOperatorKind token_to_binary_op(BaaTokenType token_type)
{
    switch (token_type)
    {
    case BAA_TOKEN_PLUS:
        return BAA_BINARY_OP_ADD;
    case BAA_TOKEN_MINUS:
        return BAA_BINARY_OP_SUBTRACT;
    case BAA_TOKEN_STAR:
        return BAA_BINARY_OP_MULTIPLY;
    case BAA_TOKEN_SLASH:
        return BAA_BINARY_OP_DIVIDE;
    case BAA_TOKEN_PERCENT:
        return BAA_BINARY_OP_MODULO;
    case BAA_TOKEN_EQUAL_EQUAL:
        return BAA_BINARY_OP_EQUAL;
    case BAA_TOKEN_BANG_EQUAL:
        return BAA_BINARY_OP_NOT_EQUAL;
    case BAA_TOKEN_LESS:
        return BAA_BINARY_OP_LESS_THAN;
    case BAA_TOKEN_LESS_EQUAL:
        return BAA_BINARY_OP_LESS_EQUAL;
    case BAA_TOKEN_GREATER:
        return BAA_BINARY_OP_GREATER_THAN;
    case BAA_TOKEN_GREATER_EQUAL:
        return BAA_BINARY_OP_GREATER_EQUAL;
    case BAA_TOKEN_AND:
        return BAA_BINARY_OP_LOGICAL_AND;
    case BAA_TOKEN_OR:
        return BAA_BINARY_OP_LOGICAL_OR;
    default:
        // This should not happen if get_token_precedence returned a valid precedence
        return BAA_BINARY_OP_ADD; // Default fallback
    }
}

BaaNode *parse_expression(BaaParser *parser)
{
    // Start with unary expression parsing
    BaaNode *left_expr = parse_unary_expression(parser);
    if (!left_expr)
    {
        return NULL;
    }

    // Then handle binary operators with precedence climbing
    return parse_binary_expression_rhs(parser, 0, left_expr);
}

BaaNode *parse_unary_expression(BaaParser *parser)
{
    // Check for unary operators
    if (parser->current_token.type == BAA_TOKEN_MINUS)
    {
        // Create source span for the unary operator
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        baa_parser_advance(parser); // Consume the '-' token

        // Parse the operand (recursively handle unary expressions)
        BaaNode *operand = parse_unary_expression(parser);
        if (!operand)
        {
            return NULL;
        }

        // Update span to include the operand
        span.end = operand->span.end;

        // Create unary expression node
        return baa_ast_new_unary_expr_node(span, operand, BAA_UNARY_OP_MINUS);
    }
    else if (parser->current_token.type == BAA_TOKEN_PLUS)
    {
        // Create source span for the unary operator
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        baa_parser_advance(parser); // Consume the '+' token

        // Parse the operand (recursively handle unary expressions)
        BaaNode *operand = parse_unary_expression(parser);
        if (!operand)
        {
            return NULL;
        }

        // Update span to include the operand
        span.end = operand->span.end;

        // Create unary expression node
        return baa_ast_new_unary_expr_node(span, operand, BAA_UNARY_OP_PLUS);
    }
    else if (parser->current_token.type == BAA_TOKEN_BANG)
    {
        // Create source span for the unary operator
        BaaAstSourceSpan span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        baa_parser_advance(parser); // Consume the '!' token

        // Parse the operand (recursively handle unary expressions)
        BaaNode *operand = parse_unary_expression(parser);
        if (!operand)
        {
            return NULL;
        }

        // Update span to include the operand
        span.end = operand->span.end;

        // Create unary expression node
        return baa_ast_new_unary_expr_node(span, operand, BAA_UNARY_OP_LOGICAL_NOT);
    }

    // No unary operator, parse primary expression and then handle postfix operations
    BaaNode *primary_expr = parse_primary_expression(parser);
    if (!primary_expr)
    {
        return NULL;
    }

    // Handle postfix operations (function calls, array access, etc.)
    return parse_postfix_expression(parser, primary_expr);
}

BaaNode *parse_binary_expression_rhs(BaaParser *parser, int min_precedence, BaaNode *left_expr)
{
    if (!left_expr)
    {
        return NULL;
    }

    while (true)
    {
        // Check if current token is a binary operator
        int token_precedence = get_token_precedence(parser->current_token.type);

        // If it's not a binary operator or precedence is too low, we're done
        if (token_precedence < min_precedence)
        {
            return left_expr;
        }

        // Remember the operator token and advance
        BaaTokenType operator_token = parser->current_token.type;
        BaaAstSourceSpan operator_span = {
            .start = {
                .filename = parser->source_filename,
                .line = parser->current_token.line,
                .column = parser->current_token.column},
            .end = {.filename = parser->source_filename, .line = parser->current_token.line, .column = parser->current_token.column + parser->current_token.length}};

        baa_parser_advance(parser); // Consume the operator

        // Parse the right operand
        BaaNode *right_expr = parse_unary_expression(parser);
        if (!right_expr)
        {
            baa_ast_free_node(left_expr);
            return NULL;
        }

        // Check if the next operator has higher precedence
        int next_precedence = get_token_precedence(parser->current_token.type);
        if (next_precedence > token_precedence)
        {
            // Parse the right side with higher precedence first
            right_expr = parse_binary_expression_rhs(parser, token_precedence + 1, right_expr);
            if (!right_expr)
            {
                baa_ast_free_node(left_expr);
                return NULL;
            }
        }

        // Create the binary expression node
        BaaAstSourceSpan expr_span = {
            .start = left_expr->span.start,
            .end = right_expr->span.end};

        BaaBinaryOperatorKind op_kind = token_to_binary_op(operator_token);
        BaaNode *binary_expr = baa_ast_new_binary_expr_node(expr_span, left_expr, right_expr, op_kind);
        if (!binary_expr)
        {
            baa_ast_free_node(left_expr);
            baa_ast_free_node(right_expr);
            return NULL;
        }

        // The binary expression becomes the new left operand for the next iteration
        left_expr = binary_expr;
    }
}

// --- Postfix Expression Parsing ---

BaaNode *parse_postfix_expression(BaaParser *parser, BaaNode *base_expr)
{
    if (!base_expr)
    {
        return NULL;
    }

    BaaNode *current_expr = base_expr;

    // Handle postfix operations in a loop
    while (true)
    {
        if (parser->current_token.type == BAA_TOKEN_LPAREN)
        {
            // Function call
            current_expr = parse_call_expression(parser, current_expr);
            if (!current_expr)
            {
                return NULL;
            }
        }
        // Future: Add array access [index], member access .member, etc.
        else
        {
            // No more postfix operations
            break;
        }
    }

    return current_expr;
}

BaaNode *parse_call_expression(BaaParser *parser, BaaNode *callee_expr)
{
    if (!callee_expr)
    {
        return NULL;
    }

    // Convert lexer span to AST span for start position
    BaaAstSourceSpan start_span = {
        {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
        {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
    };

    // Expect opening parenthesis
    if (parser->current_token.type != BAA_TOKEN_LPAREN)
    {
        baa_parser_error(parser, L"Expected '(' to start function call");
        baa_ast_free_node(callee_expr);
        return NULL;
    }

    baa_parser_advance(parser); // Consume '('

    // Create the call expression node
    BaaNode *call_expr = baa_ast_new_call_expr_node(start_span, callee_expr);
    if (!call_expr)
    {
        baa_parser_error(parser, L"Failed to create call expression node");
        baa_ast_free_node(callee_expr);
        return NULL;
    }

    // Handle empty argument list
    if (parser->current_token.type == BAA_TOKEN_RPAREN)
    {
        baa_parser_advance(parser); // Consume ')'

        // Update span to include closing parenthesis
        BaaAstSourceSpan end_span = {
            {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
            {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
        };
        call_expr->span.end = end_span.end;

        return call_expr; // Empty argument list is valid
    }

    // Parse arguments
    do {
        // Parse a single argument expression
        BaaNode *arg_expr = parse_expression(parser);
        if (!arg_expr)
        {
            baa_parser_error(parser, L"Failed to parse function call argument");
            baa_ast_free_node(call_expr);
            return NULL;
        }

        // Add the argument to the call expression
        if (!baa_ast_add_call_argument(call_expr, arg_expr))
        {
            baa_parser_error(parser, L"Failed to add argument to function call");
            baa_ast_free_node(arg_expr);
            baa_ast_free_node(call_expr);
            return NULL;
        }

        // Check for comma (more arguments) or closing parenthesis
        if (parser->current_token.type == BAA_TOKEN_COMMA)
        {
            baa_parser_advance(parser); // Consume ','
            // Continue to parse next argument
        }
        else if (parser->current_token.type == BAA_TOKEN_RPAREN)
        {
            break; // End of argument list
        }
        else
        {
            baa_parser_error(parser, L"Expected ',' or ')' in function call argument list");
            baa_ast_free_node(call_expr);
            return NULL;
        }

    } while (parser->current_token.type != BAA_TOKEN_RPAREN &&
             parser->current_token.type != BAA_TOKEN_EOF);

    // Expect closing parenthesis
    if (parser->current_token.type != BAA_TOKEN_RPAREN)
    {
        baa_parser_error(parser, L"Expected ')' to close function call argument list");
        baa_ast_free_node(call_expr);
        return NULL;
    }

    // Update span to include closing parenthesis
    BaaAstSourceSpan end_span = {
        {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
        {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
    };
    call_expr->span.end = end_span.end;

    baa_parser_advance(parser); // Consume ')'

    return call_expr;
}
