/**
 * @file lexer.c
 * @brief يقوم بتحويل الكود المصدري المكتوب باللغة العربية (UTF-8) إلى وحدات لفظية (Tokens).
 */

#include "baa.h"
#include <ctype.h>

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر وتخطي الـ BOM إذا وجد.
 */
void lexer_init(Lexer* l, const char* src) {
    l->src = src;
    l->pos = 0;
    l->len = strlen(src);
    l->line = 1;
    // تخطي علامة ترتيب البايت (BOM) لملفات UTF-8 إذا كانت موجودة
    if (l->len >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) {
        l->pos = 3;
    }
}

/**
 * @brief التحقق مما إذا كان البايت الحالي بداية لمحرف عربي في ترميز UTF-8.
 */
int is_arabic_start_byte(char c) {
    unsigned char ub = (unsigned char)c;
    return (ub >= 0xD8 && ub <= 0xDB);
}

/**
 * @brief التحقق مما إذا كان الجزء الحالي من النص يمثل رقماً عربياً (٠-٩).
 */
int is_arabic_digit(const char* c) {
    unsigned char b1 = (unsigned char)c[0];
    unsigned char b2 = (unsigned char)c[1];
    return (b1 == 0xD9 && b2 >= 0xA0 && b2 <= 0xA9);
}

/**
 * @brief استخراج الوحدة اللفظية التالية.
 */
Token lexer_next_token(Lexer* l) {
    Token token = {0};
    
    // تخطي المسافات البيضاء والأسطر الجديدة
    while (l->pos < l->len && isspace(l->src[l->pos])) {
        if (l->src[l->pos] == '\n') l->line++;
        l->pos++;
    }

    // تخطي التعليقات التي تبدأ بـ //
    if (l->pos + 1 < l->len && l->src[l->pos] == '/' && l->src[l->pos+1] == '/') {
        while (l->pos < l->len && l->src[l->pos] != '\n') l->pos++;
        return lexer_next_token(l);
    }

    // التحقق من نهاية الملف
    if (l->pos >= l->len) { token.type = TOKEN_EOF; return token; }

    const char* current = l->src + l->pos;

    // --- معالجة النصوص ("...") ---
    if (*current == '"') {
        l->pos++;
        size_t start = l->pos;
        while (l->pos < l->len && l->src[l->pos] != '"') {
            if (l->src[l->pos] == '\n') l->line++;
            l->pos++;
        }
        if (l->pos >= l->len) { printf("Lexer Error: Unterminated string\n"); exit(1); }
        size_t len = l->pos - start;
        char* str = malloc(len + 1);
        strncpy(str, l->src + start, len);
        str[len] = '\0';
        l->pos++;
        token.type = TOKEN_STRING;
        token.value = str;
        return token;
    }

    // --- معالجة الحروف ('...') ---
    if (*current == '\'') {
        l->pos++;
        char c = l->src[l->pos];
        l->pos++;
        if (l->src[l->pos] != '\'') { printf("Lexer Error: Expected '\n"); exit(1); }
        l->pos++;
        token.type = TOKEN_CHAR;
        char* val = malloc(2); val[0] = c; val[1] = '\0';
        token.value = val;
        return token;
    }

    // معالجة الرموز والعمليات (نقطة، فاصلة، جمع، طرح، إلخ)
    if (*current == '.') { token.type = TOKEN_DOT; l->pos++; return token; }
    if (*current == ',') { token.type = TOKEN_COMMA; l->pos++; return token; }
    if (*current == '+') { token.type = TOKEN_PLUS; l->pos++; return token; }
    if (*current == '-') { token.type = TOKEN_MINUS; l->pos++; return token; }
    if (*current == '*') { token.type = TOKEN_STAR; l->pos++; return token; }
    if (*current == '/') { token.type = TOKEN_SLASH; l->pos++; return token; }
    if (*current == '%') { token.type = TOKEN_PERCENT; l->pos++; return token; }
    if (*current == '(') { token.type = TOKEN_LPAREN; l->pos++; return token; }
    if (*current == ')') { token.type = TOKEN_RPAREN; l->pos++; return token; }
    if (*current == '{') { token.type = TOKEN_LBRACE; l->pos++; return token; }
    if (*current == '}') { token.type = TOKEN_RBRACE; l->pos++; return token; }

    // معالجة العمليات المنطقية (&&، ||، !)
    if (*current == '&') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '&') {
            token.type = TOKEN_AND; l->pos += 2; return token;
        }
    }
    if (*current == '|') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '|') {
            token.type = TOKEN_OR; l->pos += 2; return token;
        }
    }
    if (*current == '!') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_NEQ; l->pos += 2; return token;
        }
        token.type = TOKEN_NOT; l->pos++; return token; // نفي أحادي
    }

    // معالجة عمليات المقارنة والتعيين (=، ==، <، <=، >، >=)
    if (*current == '=') { 
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_EQ; l->pos += 2; return token;
        }
        token.type = TOKEN_ASSIGN; l->pos++; return token; 
    }
    if (*current == '<') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_LTE; l->pos += 2; return token;
        }
        token.type = TOKEN_LT; l->pos++; return token;
    }
    if (*current == '>') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_GTE; l->pos += 2; return token;
        }
        token.type = TOKEN_GT; l->pos++; return token;
    }

    // معالجة الأرقام (تدعم الأرقام العربية ٠-٩ والأرقام الغربية 0-9)
    if (isdigit(*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        char buffer[64] = {0}; 
        int buf_idx = 0;
        while (l->pos < l->len) {
            const char* c = l->src + l->pos;
            if (isdigit(*c)) { buffer[buf_idx++] = *c; l->pos++; } 
            else if (is_arabic_digit(c)) {
                // تحويل الرقم العربي (UTF-8) إلى مكافئه في ASCII
                buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                l->pos += 2;
            } else { break; }
        }
        token.value = strdup(buffer);
        return token;
    }

    // معالجة المعرفات والكلمات المفتاحية المكتوبة بالعربية
    if (is_arabic_start_byte(*current)) {
        size_t start = l->pos;
        while (l->pos < l->len && !isspace(l->src[l->pos]) && 
               strchr(".+-,=(){}!<>*/%&|\"'", l->src[l->pos]) == NULL) { 
            l->pos++;
        }
        
        size_t len = l->pos - start;
        char* word = malloc(len + 1);
        strncpy(word, l->src + start, len);
        word[len] = '\0';

        // التحقق مما إذا كانت الكلمة كلمة مفتاحية محجوزة
        if (strcmp(word, "إرجع") == 0) token.type = TOKEN_RETURN;
        else if (strcmp(word, "اطبع") == 0) token.type = TOKEN_PRINT;
        else if (strcmp(word, "صحيح") == 0) token.type = TOKEN_KEYWORD_INT;
        else if (strcmp(word, "إذا") == 0) token.type = TOKEN_IF;
        else if (strcmp(word, "طالما") == 0) token.type = TOKEN_WHILE;
        else {
            // إذا لم تكن كلمة مفتاحية، فهي معرف (اسم متغير أو دالة)
            token.type = TOKEN_IDENTIFIER;
            token.value = word;
            return token;
        }
        free(word);
        return token;
    }

    // إذا وصلنا لهنا، فهذا يعني وجود محرف غير معروف
    token.type = TOKEN_INVALID;
    printf("Lexer Error: Unknown byte 0x%02X at line %d\n", (unsigned char)*current, l->line);
    exit(1);
    return token;
}
