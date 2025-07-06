#ifndef BAA_DECLARATION_PARSER_INTERNAL_H
#define BAA_DECLARATION_PARSER_INTERNAL_H

#include "parser_internal.h" // For BaaParser
#include "baa/ast/ast.h"     // For BaaNode
#include "baa/ast/ast_types.h" // For BaaAstNodeModifiers

/**
 * @file declaration_parser.h
 * @brief Internal header for declaration parsing functions.
 * 
 * This file contains function declarations for parsing various types of
 * declarations including variable declarations and function declarations.
 */

/**
 * @brief Parses a variable declaration statement.
 * This function handles parsing of variable declarations with optional modifiers,
 * type specifiers, and initializers.
 *
 * @param parser Pointer to the parser state.
 * @param initial_modifiers Initial modifiers that have already been parsed (e.g., const).
 * @return A BaaNode* of kind BAA_NODE_KIND_VAR_DECL_STMT representing the variable declaration, or NULL on error.
 */
BaaNode *parse_variable_declaration_statement(BaaParser *parser, BaaAstNodeModifiers initial_modifiers);

/**
 * @brief Parses a single function parameter.
 * This function handles parsing of a parameter with its type and name.
 * Expected syntax: type_specifier identifier
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* of kind BAA_NODE_KIND_PARAMETER representing the parameter, or NULL on error.
 */
BaaNode *parse_parameter(BaaParser *parser);

/**
 * @brief Parses a function parameter list enclosed in parentheses.
 * This function handles parsing of parameter lists with proper comma separation.
 * Expected syntax: '(' [parameter (',' parameter)*] ')'
 *
 * @param parser Pointer to the parser state.
 * @param parameters Pointer to an array that will be allocated and filled with parameter nodes.
 * @param parameter_count Pointer to store the number of parameters parsed.
 * @return true on success, false on error. On success, *parameters contains the parameter nodes.
 */
bool parse_parameter_list(BaaParser *parser, BaaNode ***parameters, size_t *parameter_count);

/**
 * @brief Parses a complete function definition.
 * This function handles parsing of function definitions with return type, name, parameters, and body.
 * Expected syntax: [modifiers] return_type function_name '(' parameter_list ')' block_statement
 *
 * @param parser Pointer to the parser state.
 * @param initial_modifiers Initial modifiers that have already been parsed (e.g., static, inline).
 * @return A BaaNode* of kind BAA_NODE_KIND_FUNCTION_DEF representing the function definition, or NULL on error.
 */
BaaNode *parse_function_definition(BaaParser *parser, BaaAstNodeModifiers initial_modifiers);

/**
 * @brief Parses a declaration or statement (top-level dispatcher).
 * This function determines whether to parse a declaration (variable or function) or a statement
 * based on the current token sequence.
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* representing the declaration or statement, or NULL on error.
 */
BaaNode *parse_declaration_or_statement(BaaParser *parser);

#endif // BAA_DECLARATION_PARSER_INTERNAL_H
