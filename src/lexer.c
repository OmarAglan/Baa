/**
 * @file lexer.c
 * @brief يقوم بتحويل الكود المصدري المكتوب باللغة العربية (UTF-8) إلى وحدات لفظية (Tokens).
 * @version 0.2.6 (Preprocessor Implementation + Undefine)
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
    l->macro_count = 0;
    l->skipping = false;
    
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

/**
 * @brief إضافة تعريف ماكرو جديد.
 */
void add_macro(Lexer* l, char* name, char* value) {
    if (l->macro_count >= 100) {
        printf("Preprocessor Error: Too many macros defined (Max 100).\n");
        exit(1);
    }
    l->macros[l->macro_count].name = strdup(name);
    l->macros[l->macro_count].value = strdup(value);
    l->macro_count++;
}

/**
 * @brief إزالة تعريف ماكرو.
 */
static void remove_macro(Lexer* l, const char* name) {
    for (int i = 0; i < l->macro_count; i++) {
        if (strcmp(l->macros[i].name, name) == 0) {
            free(l->macros[i].name);
            free(l->macros[i].value);
            // إزاحة العناصر المتبقية
            for (int j = i; j < l->macro_count - 1; j++) {
                l->macros[j] = l->macros[j+1];
            }
            l->macro_count--;
            return;
        }
    }
}

/**
 * @brief البحث عن ماكرو بالاسم.
 */
char* get_macro_value(Lexer* l, const char* name) {
    for (int i = 0; i < l->macro_count; i++) {
        if (strcmp(l->macros[i].name, name) == 0) {
            return l->macros[i].value;
        }
    }
    return NULL;
}


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

        // ====================================================================
        // معالجة الموجهات (Preprocessor Directives)
        // ====================================================================
        if (peek(l) == '#') {
            advance_pos(l); // consume '#'
            
            // قراءة الموجه
            if (is_arabic_start_byte(peek(l))) {
                 char* dir_start = l->state.cur_char;
                 while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                 
                 size_t len = l->state.cur_char - dir_start;
                 char* directive = malloc(len + 1);
                 strncpy(directive, dir_start, len);
                 directive[len] = '\0';
                 
                 // 1. #تضمين (Include)
                 if (strcmp(directive, "تضمين") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     if (peek(l) != '"') {
                         printf("Preprocessor Error: Expected filename string after #تضمين\n"); exit(1);
                     }
                     advance_pos(l); // skip "
                     char* path_start = l->state.cur_char;
                     while (peek(l) != '"' && peek(l) != '\0') advance_pos(l);
                     size_t path_len = l->state.cur_char - path_start;
                     char* path = malloc(path_len + 1);
                     strncpy(path, path_start, path_len);
                     path[path_len] = '\0';
                     advance_pos(l); // skip "

                     char* new_src = read_file(path);
                     if (!new_src) {
                         printf("Preprocessor Error: Could not include file '%s'\n", path);
                         exit(1);
                     }
                     
                     if (l->stack_depth >= 10) { printf("Preprocessor Error: Max include depth.\n"); exit(1); }
                     l->stack[l->stack_depth++] = l->state;
                     
                     l->state.source = new_src;
                     l->state.cur_char = new_src;
                     l->state.filename = strdup(path);
                     l->state.line = 1;
                     l->state.col = 1;
                     
                     free(directive);
                     free(path);
                     continue;
                 }
                 // 2. #تعريف (Define)
                 else if (strcmp(directive, "تعريف") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     // قراءة الاسم
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     char* name_start = l->state.cur_char;
                     while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                     size_t name_len = l->state.cur_char - name_start;
                     char* name = malloc(name_len + 1);
                     strncpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     // قراءة القيمة (باقي السطر)
                     while (peek(l) != '\0' && isspace(peek(l)) && peek(l) != '\n') advance_pos(l);
                     char* val_start = l->state.cur_char;
                     while (peek(l) != '\n' && peek(l) != '\0' && peek(l) != '\r') advance_pos(l); // لنهاية السطر
                     size_t val_len = l->state.cur_char - val_start;
                     char* val = malloc(val_len + 1);
                     strncpy(val, val_start, val_len);
                     val[val_len] = '\0';

                     // تنظيف القيمة من المسافات الزائدة في النهاية
                     while(val_len > 0 && isspace(val[val_len-1])) val[--val_len] = '\0';

                     add_macro(l, name, val);
                     free(name);
                     free(val);
                     free(directive);
                     continue;
                 }
                 // 3. #إذا_عرف (If Defined)
                 else if (strcmp(directive, "إذا_عرف") == 0) {
                     // قراءة الاسم
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     char* name_start = l->state.cur_char;
                     while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                     size_t name_len = l->state.cur_char - name_start;
                     char* name = malloc(name_len + 1);
                     strncpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     bool defined = (get_macro_value(l, name) != NULL);
                     // إذا كنا نتخطى أصلاً، نستمر في التخطي
                     if (!l->skipping) {
                         l->skipping = !defined;
                     }
                     
                     free(name);
                     free(directive);
                     continue;
                 }
                 // 4. #وإلا (Else)
                 else if (strcmp(directive, "وإلا") == 0) {
                     l->skipping = !l->skipping;
                     free(directive);
                     continue;
                 }
                 // 5. #نهاية (End)
                 else if (strcmp(directive, "نهاية") == 0) {
                     l->skipping = false;
                     free(directive);
                     continue;
                 }
                 // 6. #الغاء_تعريف (Undefine)
                 else if (strcmp(directive, "الغاء_تعريف") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     // قراءة الاسم
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     char* name_start = l->state.cur_char;
                     while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                     size_t name_len = l->state.cur_char - name_start;
                     char* name = malloc(name_len + 1);
                     strncpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     remove_macro(l, name);
                     free(name);
                     free(directive);
                     continue;
                 }
                 
                 free(directive);
            }
             printf("Preprocessor Error: Unknown directive at %s:%d\n", l->state.filename, l->state.line);
             exit(1);
        }

        // إذا كنا في وضع التخطي، نتجاهل كل شيء حتى نجد #
        if (l->skipping) {
            advance_pos(l);
            continue;
        }

        // نهاية الملف EOF
        if (peek(l) == '\0') {
            // إذا كنا داخل ملف مضمن، نعود للملف السابق (Pop)
            if (l->stack_depth > 0) {
                free(l->state.source);
                l->state = l->stack[--l->stack_depth];
                continue; 
            }
            token.type = TOKEN_EOF;
            token.filename = l->state.filename;
            token.line = l->state.line;
            token.col = l->state.col;
            return token;
        }

        break; // وجدنا بداية رمز صالح ونحن لسن في وضع التخطي
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

        // 1. Check for Macro substitution
        char* macro_val = get_macro_value(l, word);
        if (macro_val != NULL) {
            // استبدال الرمز بقيمة الماكرو
            if (macro_val[0] == '"') {
                token.type = TOKEN_STRING;
                // إزالة علامات التنصيص
                size_t vlen = strlen(macro_val);
                if (vlen >= 2) {
                    char* val = malloc(vlen - 1);
                    strncpy(val, macro_val + 1, vlen - 2);
                    val[vlen - 2] = '\0';
                    token.value = val;
                } else {
                    token.value = strdup("");
                }
            } 
            else if (isdigit((unsigned char)macro_val[0]) || is_arabic_digit(macro_val)) {
                token.type = TOKEN_INT;
                token.value = strdup(macro_val);
            }
            else {
                token.type = TOKEN_IDENTIFIER;
                token.value = strdup(macro_val);
            }
            free(word);
            return token;
        }

        // 2. الكلمات المفتاحية المحجوزة
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