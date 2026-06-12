/**
 * @file ast_dump.h
 * @brief طباعة تمثيل ثابت لشجرة الإعراب المجردة لأغراض الاختبار.
 */

#ifndef BAA_FRONTEND_AST_DUMP_H
#define BAA_FRONTEND_AST_DUMP_H

#include "ast.h"

#include <stdio.h>

/**
 * @brief طباعة بنية AST بشكل حتمي لا يعتمد على العناوين أو نتائج التحليل الدلالي.
 */
void ast_dump_print(const Node* root, FILE* out);

#endif
