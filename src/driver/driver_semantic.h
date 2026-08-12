/**
 * @file driver_semantic.h
 * @brief semantic-query-json-v1 for cursor-sensitive editor intelligence.
 */

#ifndef BAA_DRIVER_SEMANTIC_H
#define BAA_DRIVER_SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../frontend/ast.h"

bool driver_semantic_query_json_write(FILE* out,
                                      const char* compiler_version,
                                      const char* logical_file,
                                      const char* source,
                                      const Node* program,
                                      size_t position_byte);

bool driver_semantic_index_json_write(FILE* out,
                                      const char* compiler_version,
                                      const char* logical_file,
                                      const char* source,
                                      const Node* program);

bool driver_inlay_hints_json_write(FILE* out,
                                   const char* compiler_version,
                                   const char* logical_file,
                                   const char* source,
                                   const Node* program,
                                   bool complete);

#endif
