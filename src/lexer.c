/**
 * @file lexer.c
 * @brief يقوم بتحويل الكود المصدري المكتوب باللغة العربية (UTF-8) إلى وحدات لفظية (Tokens).
 * @version 0.2.2 (Diagnostic Engine - Location Tracking & String Helpers)
 */

#include "baa.h"
#include <ctype.h>

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر وتخطي الـ BOM إذا وجد.
 */
void lexer_init(Lexer* l, const char* src, const char* filename) {
    l->src = src;
    l->pos = 0;
    l->len = strlen(src);
    l->line = 1;
    l->col = 1;
    l->filename = filename; // تخزين اسم الملف

    // تخطي علامة ترتيب البايت (BOM) لملفات UTF-8 إذا كانت موجودة
    if (l->len >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) {
        l->pos = 3;
    }
}

/**
 * @brief دالة مساعدة لتقديم المؤشر وتحديث السطر والعمود.
 */
void advance_pos(Lexer* l) {
    if (l->src[l->pos] == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    l->pos++;
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
        advance_pos(l);
    }

    // تخطي التعليقات التي تبدأ بـ //
    if (l->pos + 1 < l->len && l->src[l->pos] == '/' && l->src[l->pos+1] == '/') {
        while (l->pos < l->len && l->src[l->pos] != '\n') {
            l->pos++; // لا نحتاج لتحديث العمود بدقة داخل التعليق، فقط السطر عند النهاية
        }
        // عند الوصول للسطر الجديد، الحلقة التالية ستعالجه وتحدث السطر
        return lexer_next_token(l);
    }

    // تسجيل موقع بداية الوحدة الحالية
    token.line = l->line;
    token.col = l->col;
    token.filename = l->filename;

    // التحقق من نهاية الملف
    if (l->pos >= l->len) { token.type = TOKEN_EOF; return token; }

    const char* current = l->src + l->pos;

    // --- معالجة النصوص ("...") ---
    if (*current == '"') {
        advance_pos(l); // تخطي " البداية
        size_t start = l->pos;
        while (l->pos < l->len && l->src[l->pos] != '"') {
            advance_pos(l);
        }
        if (l->pos >= l->len) { 
            // استخدام نظام الأخطاء الجديد (سيتم تنفيذه لاحقاً، حالياً طباعة مباشرة)
            printf("Lexer Error: Unterminated string at %s:%d:%d\n", l->filename, l->line, l->col); 
            exit(1); 
        }
        size_t len = l->pos - start;
        char* str = malloc(len + 1);
        strncpy(str, l->src + start, len);
        str[len] = '\0';
        advance_pos(l); // تخطي " النهاية
        token.type = TOKEN_STRING;
        token.value = str;
        return token;
    }

    // --- معالجة الحروف ('...') ---
    if (*current == '\'') {
        advance_pos(l);
        char c = l->src[l->pos];
        advance_pos(l);
        if (l->src[l->pos] != '\'') { 
            printf("Lexer Error: Expected ' at %s:%d:%d\n", l->filename, l->line, l->col); 
            exit(1); 
        }
        advance_pos(l);
        token.type = TOKEN_CHAR;
        char* val = malloc(2); val[0] = c; val[1] = '\0';
        token.value = val;
        return token;
    }

    // معالجة الفاصلة المنقوطة العربية (؛)
    if ((unsigned char)*current == 0xD8 && l->pos + 1 < l->len && (unsigned char)l->src[l->pos+1] == 0x9B) {
        token.type = TOKEN_SEMICOLON;
        l->pos += 2; l->col += 2; // تحديث يدوي لأننا قفزنا بايتين
        return token;
    }

    // معالجة الرموز والعمليات
    if (*current == '.') { token.type = TOKEN_DOT; advance_pos(l); return token; }
    if (*current == ',') { token.type = TOKEN_COMMA; advance_pos(l); return token; }
    if (*current == ':') { token.type = TOKEN_COLON; advance_pos(l); return token; }
    
    // الجمع والزيادة (++ و +)
    if (*current == '+') { 
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '+') {
            token.type = TOKEN_INC; l->pos += 2; l->col += 2; return token;
        }
        token.type = TOKEN_PLUS; advance_pos(l); return token; 
    }
    
    // الطرح والنقصان (-- و -)
    if (*current == '-') { 
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '-') {
            token.type = TOKEN_DEC; l->pos += 2; l->col += 2; return token;
        }
        token.type = TOKEN_MINUS; advance_pos(l); return token; 
    }

    if (*current == '*') { token.type = TOKEN_STAR; advance_pos(l); return token; }
    if (*current == '/') { token.type = TOKEN_SLASH; advance_pos(l); return token; }
    if (*current == '%') { token.type = TOKEN_PERCENT; advance_pos(l); return token; }
    if (*current == '(') { token.type = TOKEN_LPAREN; advance_pos(l); return token; }
    if (*current == ')') { token.type = TOKEN_RPAREN; advance_pos(l); return token; }
    if (*current == '{') { token.type = TOKEN_LBRACE; advance_pos(l); return token; }
    if (*current == '}') { token.type = TOKEN_RBRACE; advance_pos(l); return token; }
    if (*current == '[') { token.type = TOKEN_LBRACKET; advance_pos(l); return token; }
    if (*current == ']') { token.type = TOKEN_RBRACKET; advance_pos(l); return token; }

    // معالجة العمليات المنطقية (&&، ||، !)
    if (*current == '&') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '&') {
            token.type = TOKEN_AND; l->pos += 2; l->col += 2; return token;
        }
    }
    if (*current == '|') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '|') {
            token.type = TOKEN_OR; l->pos += 2; l->col += 2; return token;
        }
    }
    if (*current == '!') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_NEQ; l->pos += 2; l->col += 2; return token;
        }
        token.type = TOKEN_NOT; advance_pos(l); return token;
    }

    // معالجة عمليات المقارنة والتعيين (=، ==، <، <=، >، >=)
    if (*current == '=') { 
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_EQ; l->pos += 2; l->col += 2; return token;
        }
        token.type = TOKEN_ASSIGN; advance_pos(l); return token; 
    }
    if (*current == '<') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_LTE; l->pos += 2; l->col += 2; return token;
        }
        token.type = TOKEN_LT; advance_pos(l); return token;
    }
    if (*current == '>') {
        if (l->pos + 1 < l->len && l->src[l->pos+1] == '=') {
            token.type = TOKEN_GTE; l->pos += 2; l->col += 2; return token;
        }
        token.type = TOKEN_GT; advance_pos(l); return token;
    }

    // معالجة الأرقام (تدعم الأرقام العربية ٠-٩ والأرقام الغربية 0-9)
    if (isdigit(*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        char buffer[64] = {0}; 
        int buf_idx = 0;
        while (l->pos < l->len) {
            const char* c = l->src + l->pos;
            if (isdigit(*c)) { 
                buffer[buf_idx++] = *c; 
                advance_pos(l); 
            } 
            else if (is_arabic_digit(c)) {
                // تحويل الرقم العربي (UTF-8) إلى مكافئه في ASCII
                buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                l->pos += 2; l->col += 2;
            } else { break; }
        }
        token.value = strdup(buffer);
        return token;
    }

    // معالجة المعرفات والكلمات المفتاحية
    if (is_arabic_start_byte(*current)) {
        size_t start = l->pos;
        while (l->pos < l->len && !isspace(l->src[l->pos]) && 
               strchr(".+-,=:(){}[]!<>*/%&|\"'", l->src[l->pos]) == NULL) {
            // ملاحظة: الفاصلة المنقوطة العربية (؛) تبدأ بـ 0xD8 وهي ضمن النطاق العربي
            // لذلك يجب التحقق منها وإيقاف قراءة المعرف إذا وجدناها
            if ((unsigned char)l->src[l->pos] == 0xD8 && (unsigned char)l->src[l->pos+1] == 0x9B) {
                break;
            }
            advance_pos(l);
        }
        
        size_t len = l->pos - start;
        char* word = malloc(len + 1);
        strncpy(word, l->src + start, len);
        word[len] = '\0';

        // التحقق مما إذا كانت الكلمة كلمة مفتاحية محجوزة
        if (strcmp(word, "إرجع") == 0) token.type = TOKEN_RETURN;
        else if (strcmp(word, "اطبع") == 0) token.type = TOKEN_PRINT;
        else if (strcmp(word, "صحيح") == 0) token.type = TOKEN_KEYWORD_INT;
        else if (strcmp(word, "نص") == 0) token.type = TOKEN_KEYWORD_STRING;
        else if (strcmp(word, "إذا") == 0) token.type = TOKEN_IF;
        else if (strcmp(word, "وإلا") == 0) token.type = TOKEN_ELSE;
        else if (strcmp(word, "طالما") == 0) token.type = TOKEN_WHILE;
        else if (strcmp(word, "لكل") == 0) token.type = TOKEN_FOR;
        else if (strcmp(word, "توقف") == 0) token.type = TOKEN_BREAK;
        else if (strcmp(word, "استمر") == 0) token.type = TOKEN_CONTINUE;
        else if (strcmp(word, "اختر") == 0) token.type = TOKEN_SWITCH;
        else if (strcmp(word, "حالة") == 0) token.type = TOKEN_CASE;
        else if (strcmp(word, "افتراضي") == 0) token.type = TOKEN_DEFAULT;
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
    printf("Lexer Error: Unknown byte 0x%02X at %s:%d:%d\n", (unsigned char)*current, l->filename, l->line, l->col);
    exit(1);
    return token;
}

/**
 * @brief تحويل نوع الوحدة إلى نص مقروء (لأغراض التنقيح ورسائل الخطأ).
 */
const char* token_type_to_str(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_INT: return "INTEGER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        
        // Keywords
        case TOKEN_KEYWORD_INT: return "صحيح";
        case TOKEN_KEYWORD_STRING: return "نص";
        case TOKEN_RETURN: return "إرجع";
        case TOKEN_PRINT: return "اطبع";
        case TOKEN_IF: return "إذا";
        case TOKEN_ELSE: return "وإلا";
        case TOKEN_WHILE: return "طالما";
        case TOKEN_FOR: return "لكل";
        case TOKEN_BREAK: return "توقف";
        case TOKEN_CONTINUE: return "استمر";
        case TOKEN_SWITCH: return "اختر";
        case TOKEN_CASE: return "حالة";
        case TOKEN_DEFAULT: return "افتراضي";
        
        // Symbols
        case TOKEN_ASSIGN: return "=";
        case TOKEN_DOT: return ".";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_SEMICOLON: return "؛";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_INC: return "++";
        case TOKEN_DEC: return "--";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_GT: return ">";
        case TOKEN_LTE: return "<=";
        case TOKEN_GTE: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        
        default: return "UNKNOWN";
    }
}