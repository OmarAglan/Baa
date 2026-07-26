/**
 * @file driver_symbols.h
 * @brief symbols-json-v1 tooling output for Qalam and language servers.
 */

#ifndef BAA_DRIVER_SYMBOLS_H
#define BAA_DRIVER_SYMBOLS_H

#include <stdbool.h>
#include <stdio.h>

#include "../frontend/ast.h"

bool driver_symbols_json_write(FILE* out,
                               const char* compiler_version,
                               const char* logical_file,
                               const char* source,
                               const Node* program);

#endif
