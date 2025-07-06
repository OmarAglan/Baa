#include "declaration_parser.h"
#include "type_parser.h"
#include "expression_parser.h"
#include "statement_parser.h"
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

// --- Function Parameter Parsing ---

BaaNode *parse_parameter(BaaParser *parser)
{
    // Convert lexer span to AST span for start position
    BaaAstSourceSpan start_span = {
        {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
        {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
    };

    // Parse the parameter type
    BaaNode *type_node = parse_type_specifier(parser);
    if (!type_node)
    {
        return NULL; // Error in parsing type specifier
    }

    // Expect parameter name (identifier)
    if (parser->current_token.type != BAA_TOKEN_IDENTIFIER)
    {
        baa_parser_error(parser, L"Expected parameter name after type specifier");
        baa_ast_free_node(type_node);
        return NULL;
    }

    // Extract parameter name from lexeme
    wchar_t *param_name = baa_strdup(parser->current_token.lexeme);
    if (!param_name)
    {
        baa_parser_error(parser, L"Memory allocation failed for parameter name");
        baa_ast_free_node(type_node);
        return NULL;
    }

    // Convert lexer span to AST span for end position
    BaaAstSourceSpan end_span = {
        {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
        {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
    };
    baa_parser_advance(parser); // Consume the parameter name

    // Create the parameter node with combined span
    BaaAstSourceSpan param_span = {
        start_span.start,
        end_span.end
    };
    BaaNode *param_node = baa_ast_new_parameter_node(param_span, param_name, type_node);

    baa_free(param_name); // Free the duplicated name (baa_ast_new_parameter_node duplicates it again)

    if (!param_node)
    {
        baa_parser_error(parser, L"Failed to create parameter node");
        baa_ast_free_node(type_node);
        return NULL;
    }

    return param_node;
}

bool parse_parameter_list(BaaParser *parser, BaaNode ***parameters, size_t *parameter_count)
{
    if (!parameters || !parameter_count)
    {
        return false; // Invalid output parameters
    }

    *parameters = NULL;
    *parameter_count = 0;

    // Expect opening parenthesis
    if (parser->current_token.type != BAA_TOKEN_LPAREN)
    {
        baa_parser_error(parser, L"Expected '(' to start parameter list");
        return false;
    }

    baa_parser_advance(parser); // Consume '('

    // Handle empty parameter list
    if (parser->current_token.type == BAA_TOKEN_RPAREN)
    {
        baa_parser_advance(parser); // Consume ')'
        return true; // Empty parameter list is valid
    }

    // Parse parameters
    size_t capacity = 4; // Initial capacity
    BaaNode **param_array = (BaaNode **)baa_malloc(capacity * sizeof(BaaNode *));
    if (!param_array)
    {
        baa_parser_error(parser, L"Memory allocation failed for parameter array");
        return false;
    }

    size_t count = 0;

    do {
        // Parse a single parameter
        BaaNode *param = parse_parameter(parser);
        if (!param)
        {
            // Clean up already parsed parameters
            for (size_t i = 0; i < count; i++)
            {
                baa_ast_free_node(param_array[i]);
            }
            baa_free(param_array);
            return false;
        }

        // Resize array if needed
        if (count >= capacity)
        {
            capacity *= 2;
            BaaNode **new_array = (BaaNode **)baa_realloc(param_array, capacity * sizeof(BaaNode *));
            if (!new_array)
            {
                baa_parser_error(parser, L"Memory allocation failed for parameter array resize");
                baa_ast_free_node(param);
                for (size_t i = 0; i < count; i++)
                {
                    baa_ast_free_node(param_array[i]);
                }
                baa_free(param_array);
                return false;
            }
            param_array = new_array;
        }

        param_array[count++] = param;

        // Check for comma (more parameters) or closing parenthesis
        if (parser->current_token.type == BAA_TOKEN_COMMA)
        {
            baa_parser_advance(parser); // Consume ','
            // Continue to parse next parameter
        }
        else if (parser->current_token.type == BAA_TOKEN_RPAREN)
        {
            break; // End of parameter list
        }
        else
        {
            baa_parser_error(parser, L"Expected ',' or ')' in parameter list");
            for (size_t i = 0; i < count; i++)
            {
                baa_ast_free_node(param_array[i]);
            }
            baa_free(param_array);
            return false;
        }

    } while (parser->current_token.type != BAA_TOKEN_RPAREN &&
             parser->current_token.type != BAA_TOKEN_EOF);

    // Expect closing parenthesis
    if (parser->current_token.type != BAA_TOKEN_RPAREN)
    {
        baa_parser_error(parser, L"Expected ')' to close parameter list");
        for (size_t i = 0; i < count; i++)
        {
            baa_ast_free_node(param_array[i]);
        }
        baa_free(param_array);
        return false;
    }

    baa_parser_advance(parser); // Consume ')'

    // Return results
    *parameters = param_array;
    *parameter_count = count;
    return true;
}

// --- Function Definition Parsing ---

BaaNode *parse_function_definition(BaaParser *parser, BaaAstNodeModifiers initial_modifiers)
{
    BaaAstNodeModifiers modifiers = initial_modifiers;

    // Convert lexer span to AST span for start position
    BaaAstSourceSpan start_span = {
        {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
        {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
    };

    // Parse additional modifiers if present (e.g., inline)
    while (parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE)
    {
        if (parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE)
        {
            // modifiers |= BAA_MOD_INLINE; // This would need to be defined in ast_types.h
        }

        baa_parser_advance(parser); // Consume the modifier token
    }

    // Parse the return type specifier
    BaaNode *return_type_node = parse_type_specifier(parser);
    if (!return_type_node)
    {
        return NULL; // Error in parsing return type specifier
    }

    // Expect function name (identifier)
    if (parser->current_token.type != BAA_TOKEN_IDENTIFIER)
    {
        baa_parser_error(parser, L"Expected function name after return type");
        baa_ast_free_node(return_type_node);
        return NULL;
    }

    // Extract function name from lexeme
    wchar_t *function_name = baa_strdup(parser->current_token.lexeme);
    if (!function_name)
    {
        baa_parser_error(parser, L"Memory allocation failed for function name");
        baa_ast_free_node(return_type_node);
        return NULL;
    }

    baa_parser_advance(parser); // Consume the function name

    // Parse parameter list
    BaaNode **parameters = NULL;
    size_t parameter_count = 0;
    if (!parse_parameter_list(parser, &parameters, &parameter_count))
    {
        baa_parser_error(parser, L"Failed to parse function parameter list");
        baa_free(function_name);
        baa_ast_free_node(return_type_node);
        return NULL;
    }

    // Parse function body (block statement)
    BaaNode *body = parse_block_statement(parser);
    if (!body)
    {
        baa_parser_error(parser, L"Expected function body (block statement)");
        baa_free(function_name);
        baa_ast_free_node(return_type_node);
        // Free parameters array
        if (parameters)
        {
            for (size_t i = 0; i < parameter_count; i++)
            {
                baa_ast_free_node(parameters[i]);
            }
            baa_free(parameters);
        }
        return NULL;
    }

    // Convert lexer span to AST span for end position
    BaaAstSourceSpan end_span = {
        {NULL, parser->current_token.span.start_line, parser->current_token.span.start_column},
        {NULL, parser->current_token.span.end_line, parser->current_token.span.end_column}
    };

    // Create the function definition node with combined span
    BaaAstSourceSpan function_span = {
        start_span.start,
        end_span.end
    };

    BaaNode *function_def_node = baa_ast_new_function_def_node(function_span, function_name,
                                                               modifiers, return_type_node,
                                                               body, false); // is_variadic = false for now

    baa_free(function_name); // Free the duplicated name (baa_ast_new_function_def_node duplicates it again)

    if (!function_def_node)
    {
        baa_parser_error(parser, L"Failed to create function definition node");
        baa_ast_free_node(return_type_node);
        baa_ast_free_node(body);
        // Free parameters array
        if (parameters)
        {
            for (size_t i = 0; i < parameter_count; i++)
            {
                baa_ast_free_node(parameters[i]);
            }
            baa_free(parameters);
        }
        return NULL;
    }

    // Add parameters to the function definition
    if (parameters)
    {
        for (size_t i = 0; i < parameter_count; i++)
        {
            if (!baa_ast_add_function_parameter(function_def_node, parameters[i]))
            {
                baa_parser_error(parser, L"Failed to add parameter to function definition");
                baa_ast_free_node(function_def_node);
                // Free remaining parameters
                for (size_t j = i; j < parameter_count; j++)
                {
                    baa_ast_free_node(parameters[j]);
                }
                baa_free(parameters);
                return NULL;
            }
        }
        baa_free(parameters); // Free the parameters array (nodes are now owned by function_def_node)
    }

    return function_def_node;
}

// --- Declaration/Statement Dispatcher ---

/**
 * @brief Helper function to check if the current token sequence looks like a function definition.
 * This function performs lookahead to distinguish between function definitions and variable declarations.
 *
 * @param parser Pointer to the parser state.
 * @return true if the sequence looks like a function definition, false otherwise.
 */
static bool looks_like_function_definition(BaaParser *parser)
{
    // We need to look ahead to see if after [modifiers] type_specifier identifier we have '('
    // This is a simplified heuristic - a more robust implementation would use proper lookahead

    // Save current parser state
    BaaToken saved_current = parser->current_token;
    BaaToken saved_previous = parser->previous_token;

    // Skip modifiers
    while (parser->current_token.type == BAA_TOKEN_CONST ||
           parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE ||
           parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT)
    {
        baa_parser_advance(parser);
    }

    // Skip type specifier (simplified - assumes single token type)
    if (baa_token_is_type(parser->current_token.type))
    {
        baa_parser_advance(parser);
    }
    else
    {
        // Restore parser state and return false
        parser->current_token = saved_current;
        parser->previous_token = saved_previous;
        return false;
    }

    // Skip identifier (function/variable name)
    if (parser->current_token.type == BAA_TOKEN_IDENTIFIER)
    {
        baa_parser_advance(parser);
    }
    else
    {
        // Restore parser state and return false
        parser->current_token = saved_current;
        parser->previous_token = saved_previous;
        return false;
    }

    // Check if next token is '(' (function definition) or something else (variable declaration)
    bool is_function = (parser->current_token.type == BAA_TOKEN_LPAREN);

    // Restore parser state
    parser->current_token = saved_current;
    parser->previous_token = saved_previous;

    return is_function;
}

BaaNode *parse_declaration_or_statement(BaaParser *parser)
{
    // Check if this could be a declaration first
    if (parser->current_token.type == BAA_TOKEN_CONST ||
        parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE ||
        parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT ||
        baa_token_is_type(parser->current_token.type))
    {
        // Parse modifiers first
        BaaAstNodeModifiers modifiers = BAA_MOD_NONE;

        // Handle const modifier
        if (parser->current_token.type == BAA_TOKEN_CONST)
        {
            modifiers |= BAA_MOD_CONST;
            baa_parser_advance(parser);
        }

        // Handle other modifiers
        while (parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE ||
               parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT)
        {
            if (parser->current_token.type == BAA_TOKEN_KEYWORD_INLINE)
            {
                // modifiers |= BAA_MOD_INLINE; // This would need to be defined in ast_types.h
            }
            else if (parser->current_token.type == BAA_TOKEN_KEYWORD_RESTRICT)
            {
                // modifiers |= BAA_MOD_RESTRICT; // This would need to be defined in ast_types.h
            }

            baa_parser_advance(parser);
        }

        // Now determine if this is a function definition or variable declaration
        if (looks_like_function_definition(parser))
        {
            return parse_function_definition(parser, modifiers);
        }
        else
        {
            return parse_variable_declaration_statement(parser, modifiers);
        }
    }

    // Not a declaration, delegate to statement parser
    return parse_statement(parser);
}
