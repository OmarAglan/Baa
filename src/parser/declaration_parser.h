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

#endif // BAA_DECLARATION_PARSER_INTERNAL_H
