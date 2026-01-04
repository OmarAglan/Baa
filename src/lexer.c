/**
 * @file lexer.c
 * @brief يقوم بتحويل الكود المصدري المكتوب باللغة العربية (UTF-8) إلى وحدات لفظية (Tokens).
 * @version 0.2.2 (Diagnostic Engine - Location Tracking & String Helpers)
 */

#include "baa.h"
#include <ctype.h>

// قراءة ملف (من main.c، يجب أن تكون متاحة للجميع الآن)
char* read_file(const char* path);

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر وتخطي الـ BOM إذا وجد.
 */
void lexer_init(Lexer* l, char* src, const char* filename) {
    l->stack_depth = 0;
    
    // تهيئة الحالة الحالية
    l->state.source = src;
    l->state.filename = filename; // تخزين اسم الملف
    l->state.line = 1;
    l->state.col = 1;
    l->state.cur_char = src;

    // تخطي علامة ترتيب البايت (BOM) لملفات UTF-8 إذا كانت موجودة
    size_t len = strlen(src);
    if (len >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) {
        l->state.cur_char += 3;
    }
}

/**
 * @brief دالة مساعدة لتقديم المؤشر وتحديث السطر والعمود.
 */
/**
 * @brief دالة مساعدة لتقديم المؤشر وتحديث السطر والعمود.
 */
void advance_pos(Lexer* l) {
    if (*l->state.cur_char == '\n') {
        l->state.line++;
        l->state.col = 1;
    } else {
        l->state.col++;
    }
    l->state.cur_char++;
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
// Helper to peek current char
char peek(Lexer* l) { return *l->state.cur_char; }
// Helper to peek next char
char peek_next(Lexer* l) { return *(l->state.cur_char + 1); }


Token lexer_next_token(Lexer* l) {
    Token token = {0};
    
    // 0. حلقة لتجاوز المسافات ومعالجة التضمين
    while (1) {
        // تخطي المسافات البيضاء والأسطر الجديدة
        while (peek(l) != '\0' && isspace(peek(l))) {
            advance_pos(l);
        }

        // تخطي التعليقات //
        if (peek(l) == '/' && peek_next(l) == '/') {
            while (peek(l) != '\0' && peek(l) != '\n') {
                l->state.cur_char++; // تخطي سريع
            }
            continue; // إعادة المحاولة (لمعالجة السطر الجديد)
        }

        // معالجة الموجه # (Preprocessor)
        if (peek(l) == '#') {
            advance_pos(l); // consume '#'
            
            // قراءة الموجه
            if (is_arabic_start_byte(peek(l))) {
                 // نحتاج لقراءة الكلمة "تضمين"
                 // بايتات "تضمين" UTF-8: d8 aa d8 b6 d9 85 d9 8a d9 86
                 // للتبسيط، سنقرأ الكلمة ونتحقق منها
                 char* directive_start = l->state.cur_char;
                 while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                 
                 // مقارنة "تضمين"
                 // نقوم بنسخ مؤقت
                 size_t len = l->state.cur_char - directive_start;
                 if (len == 10 && strncmp(directive_start, "تضمين", 10) == 0) {
                     // 1. تخطي المسافات
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     
                     // 2. توقع اسم الملف كنص "..."
                     if (peek(l) != '"') {
                         printf("Preprocessor Error: Expected filename string after #تضمين at %s:%d\n", l->state.filename, l->state.line);
                         exit(1);
                     }
                     advance_pos(l); // skip "

                     char* path_start = l->state.cur_char;
                     while (peek(l) != '"' && peek(l) != '\0') advance_pos(l);
                     
                     size_t path_len = l->state.cur_char - path_start;
                     char* path = malloc(path_len + 1);
                     strncpy(path, path_start, path_len);
                     path[path_len] = '\0';
                     
                     advance_pos(l); // skip "
                     
                     // 3. قراءة الملف الجديد
                     char* new_src = read_file(path);
                     if (!new_src) {
                         printf("Preprocessor Error: Could not include file '%s'\n", path);
                         exit(1);
                     }
                     
                     // 4. وضع الحالة الحالية في المكدس (Push)
                     if (l->stack_depth >= 10) {
                          printf("Preprocessor Error: Too many nested includes (Max 10)\n");
                          exit(1);
                     }
                     l->stack[l->stack_depth++] = l->state;
                     
                     // 5. تهيئة الحالة الجديدة
                     l->state.source = new_src;
                     l->state.cur_char = new_src;
                     l->state.filename = strdup(path); // يجب تحريره لاحقاً
                     l->state.line = 1;
                     l->state.col = 1;
                     
                     // free(path); // path is now owned by filename (if we duplicate wisely, but here simple strdup)
                     
                     continue; // ابدأ تحليل الملف الجديد فوراً
                 }
            }
            // إذا لم يكن #تضمين، نعتبره خطأ أو نعالجه كرمز عادي (حالياً لا يوجد غير التضمين)
             printf("Preprocessor Error: Unknown directive at %s:%d\n", l->state.filename, l->state.line);
             exit(1);
        }

        // نهاية الملف EOF
        if (peek(l) == '\0') {
            // إذا كنا داخل ملف مضمن، نعود للملف السابق (Pop)
            if (l->stack_depth > 0) {
                // cleanup current
                free(l->state.source);
                // free((void*)l->state.filename); // if duplicated

                // pop
                l->state = l->stack[--l->stack_depth];
                continue; // أكمل من حيث توقفنا في الملف السابق
            }
            token.type = TOKEN_EOF;
            token.filename = l->state.filename;
            token.line = l->state.line;
            token.col = l->state.col;
            return token;
        }

        break; // وجدنا بداية رمز صالح
    }

    // تسجيل الموقع
    token.line = l->state.line;
    token.col = l->state.col;
    token.filename = l->state.filename;

    char* current = l->state.cur_char;

    // --- معالجة النصوص ("...") ---
    if (*current == '"') {
        advance_pos(l); // تخطي " البداية
        char* start = l->state.cur_char;
        while (peek(l) != '"' && peek(l) != '\0') {
            advance_pos(l);
        }
        if (peek(l) == '\0') { 
            printf("Lexer Error: Unterminated string at %s:%d:%d\n", l->state.filename, l->state.line, l->state.col); 
            exit(1); 
        }
        size_t len = l->state.cur_char - start;
        char* str = malloc(len + 1);
        strncpy(str, start, len);
        str[len] = '\0';
        advance_pos(l); // تخطي " النهاية
        token.type = TOKEN_STRING;
        token.value = str;
        return token;
    }

    // --- معالجة الحروف ('...') ---
    if (*current == '\'') {
        advance_pos(l);
        char c = *l->state.cur_char;
        advance_pos(l);
        if (*l->state.cur_char != '\'') { 
            printf("Lexer Error: Expected ' at %s:%d:%d\n", l->state.filename, l->state.line, l->state.col); 
            exit(1); 
        }
        advance_pos(l);
        token.type = TOKEN_CHAR;
        char* val = malloc(2); val[0] = c; val[1] = '\0';
        token.value = val;
        return token;
    }

    // معالجة الفاصلة المنقوطة العربية (؛)
    if ((unsigned char)*current == 0xD8 && (unsigned char)*(l->state.cur_char+1) == 0x9B) {
        token.type = TOKEN_SEMICOLON;
        l->state.cur_char += 2; l->state.col += 2; 
        return token;
    }

    // معالجة الرموز والعمليات
    if (*current == '.') { token.type = TOKEN_DOT; advance_pos(l); return token; }
    if (*current == ',') { token.type = TOKEN_COMMA; advance_pos(l); return token; }
    if (*current == ':') { token.type = TOKEN_COLON; advance_pos(l); return token; }
    
    // الجمع والزيادة (++ و +)
    if (*current == '+') { 
        if (*(l->state.cur_char+1) == '+') {
            token.type = TOKEN_INC; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_PLUS; advance_pos(l); return token; 
    }
    
    // الطرح والنقصان (-- و -)
    if (*current == '-') { 
        if (*(l->state.cur_char+1) == '-') {
            token.type = TOKEN_DEC; l->state.cur_char += 2; l->state.col += 2; return token;
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
        if (*(l->state.cur_char+1) == '&') {
            token.type = TOKEN_AND; l->state.cur_char += 2; l->state.col += 2; return token;
        }
    }
    if (*current == '|') {
        if (*(l->state.cur_char+1) == '|') {
            token.type = TOKEN_OR; l->state.cur_char += 2; l->state.col += 2; return token;
        }
    }
    if (*current == '!') {
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_NEQ; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_NOT; advance_pos(l); return token;
    }

    // معالجة عمليات المقارنة والتعيين (=، ==، <، <=، >، >=)
    if (*current == '=') { 
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_EQ; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_ASSIGN; advance_pos(l); return token; 
    }
    if (*current == '<') {
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_LTE; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_LT; advance_pos(l); return token;
    }
    if (*current == '>') {
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_GTE; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_GT; advance_pos(l); return token;
    }

    // معالجة الأرقام
    // if (isdigit(*current) || is_arabic_digit(current)) ...
    // Note: is_arabic_digit expects char*, so we pass current. Check bounds?
    // current is safe pointer.
    
    if (isdigit((unsigned char)*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        char buffer[64] = {0}; 
        int buf_idx = 0;
        
        while (1) {
            char* c = l->state.cur_char;
            if (isdigit((unsigned char)*c)) { 
                buffer[buf_idx++] = *c; 
                advance_pos(l); 
            } 
            else if (is_arabic_digit(c)) {
                buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                l->state.cur_char += 2; l->state.col += 2;
            } else { break; }
        }
        token.value = strdup(buffer);
        return token;
    }

    // معالجة المعرفات والكلمات المفتاحية
    if (is_arabic_start_byte(*current)) {
        char* start = l->state.cur_char;
        while (!isspace(peek(l)) && peek(l) != '\0' && 
               strchr(".+-,=:(){}[]!<>*/%&|\"'", peek(l)) == NULL) {
            // Check arabic semicolon
            if ((unsigned char)*l->state.cur_char == 0xD8 && (unsigned char)*(l->state.cur_char+1) == 0x9B) {
                break;
            }
            advance_pos(l);
        }
        
        size_t len = l->state.cur_char - start;
        char* word = malloc(len + 1);
        strncpy(word, start, len);
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
            token.type = TOKEN_IDENTIFIER;
            token.value = word;
            return token;
        }
        free(word);
        return token;
    }

    // إذا وصلنا لهنا، فهذا يعني وجود محرف غير معروف
    token.type = TOKEN_INVALID;
    printf("Lexer Error: Unknown byte 0x%02X at %s:%d:%d\n", (unsigned char)*current, l->state.filename, l->state.line, l->state.col);
    exit(1);
    return token;
}

/**
 * @brief تحويل نوع الوحدة إلى نص مقروء (لأغراض التنقيح ورسائل الخطأ).
 */
const char* token_type_to_str(BaaTokenType type) {
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