/**
 * @file parser.h
 * @brief ????? ?????? ????????.
 */

#ifndef BAA_FRONTEND_PARSER_H
#define BAA_FRONTEND_PARSER_H

#include "lexer.h"
#include "ast.h"

/**
 * @struct Parser
 * @brief يمثل حالة عملية التحليل القواعدي مع تخزين مؤقت للوحدة الحالية والتالية (Lookahead).
 */
typedef struct {
    Lexer* lexer;
    Token current;
    Token next;
    bool panic_mode; // وضع الذعر للتعافي من الأخطاء
    bool had_error;  // هل حدث خطأ أثناء التحليل؟
} Parser;

/**
 * @brief بدء عملية التحليل القواعدي وبناء الشجرة (AST).
 */
Node* parse(Lexer* lexer);

#endif
