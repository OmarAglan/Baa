#include "declaration_parser.h"
#include "type_parser.h"
#include "expression_parser.h"
#include "parser_internal.h"
#include "parser_utils.h"
#include "baa/ast/ast.h"
#include "baa/lexer/lexer.h"
#include <stdio.h>

/**
 * @file declaration_parser.c
 * @brief Implementation of declaration parsing functions.
 * 
 * This file implements functions for parsing various types of declarations
 * including variable declarations and function declarations.
 */

BaaNode *parse_variable_declaration_statement(BaaParser *parser, BaaAstNodeModifiers initial_modifiers)
{
    BaaAstNodeModifiers modifiers = initial_modifiers;
    
    // Parse additional modifiers if present
    while (parser->current_token.type == BAA_TOKEN_CONST ||
           parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE ||
           parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT)
    {
        if (parser->current_token.type == BAA_TOKEN_CONST)
        {
            modifiers |= BAA_MOD_CONST;
        }
        else if (parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE)
        {
            // Note: inline is typically for functions, but we'll handle it here for completeness
            // modifiers |= BAA_MOD_INLINE; // This would need to be defined in ast_types.h
        }
        else if (parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT)
        {
            // Note: restrict is typically for pointers, but we'll handle it here for completeness
            // modifiers |= BAA_MOD_RESTRICT; // This would need to be defined in ast_types.h
        }
        
        baa_parser_advance(parser); // Consume the modifier token
    }
    
    // Parse the type specifier
    BaaNode *type_node = parse_type_specifier(parser);
    if (!type_node)
    {
        return NULL; // Error in parsing type specifier
    }
    
    // Parse the variable name (identifier)
    if (parser->current_token.type != BAA_TOKEN_IDENTIFIER)
    {
        baa_parser_error_at_token(parser, &parser->current_token,
                                  L"توقع اسم متغير بعد مُحدد النوع");
        baa_ast_free_node(type_node);
        return NULL;
    }
    
    // Get the variable name
    const wchar_t *var_name = parser->current_token.lexeme ? parser->current_token.lexeme : L"";
    
    // Create source span starting from the type specifier
    BaaAstSourceSpan span = {
        .start = type_node->span.start,
        .end = {
            .filename = parser->source_filename,
            .line = parser->current_token.line,
            .column = parser->current_token.column + parser->current_token.length}};
    
    baa_parser_advance(parser); // Consume the identifier
    
    // Check for optional initializer
    BaaNode *initializer_expr = NULL;
    if (parser->current_token.type == BAA_TOKEN_EQUAL)
    {
        baa_parser_advance(parser); // Consume '='
        
        // Parse the initializer expression
        initializer_expr = parse_expression(parser);
        if (!initializer_expr)
        {
            baa_ast_free_node(type_node);
            return NULL;
        }
        
        // Update span to include the initializer
        span.end = initializer_expr->span.end;
    }
    
    // Consume the statement terminator '.'
    baa_parser_consume_token(parser, BAA_TOKEN_DOT, L"توقع '.' في نهاية إعلان المتغير");
    
    // Update span to include the dot
    span.end.filename = parser->source_filename;
    span.end.line = parser->current_token.line;
    span.end.column = parser->current_token.column;
    
    // Create the variable declaration node
    BaaNode *var_decl_node = baa_ast_new_var_decl_node(span, var_name, modifiers, type_node, initializer_expr);
    if (!var_decl_node)
    {
        baa_ast_free_node(type_node);
        if (initializer_expr)
        {
            baa_ast_free_node(initializer_expr);
        }
        return NULL;
    }
    
    return var_decl_node;
}
