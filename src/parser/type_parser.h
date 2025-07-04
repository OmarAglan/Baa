#ifndef BAA_TYPE_PARSER_INTERNAL_H
#define BAA_TYPE_PARSER_INTERNAL_H

#include "parser_internal.h" // For BaaParser
#include "baa/ast/ast.h"     // For BaaNode

/**
 * @file type_parser.h
 * @brief Internal header for type specifier parsing functions.
 * 
 * This file contains function declarations for parsing type specifications
 * including primitive types and array types.
 */

/**
 * @brief Parses a type specifier (primitive types and array types).
 * This function handles parsing of type annotations used in variable declarations
 * and function parameters.
 *
 * @param parser Pointer to the parser state.
 * @return A BaaNode* of kind BAA_NODE_KIND_TYPE representing the type specifier, or NULL on error.
 */
BaaNode *parse_type_specifier(BaaParser *parser);

#endif // BAA_TYPE_PARSER_INTERNAL_H
