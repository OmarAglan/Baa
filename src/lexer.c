/**
 * @file lexer.c
 * @brief يقوم بتحويل الكود المصدري المكتوب باللغة العربية (UTF-8) إلى وحدات لفظية (Tokens).
 * @version 0.2.9 (Input & UX Polish)
 */

#include "baa.h"
#include <ctype.h>
#include <stdarg.h>

static int is_utf8_cont_byte(unsigned char b)
{
    return ((b & 0xC0u) == 0x80u);
}

static bool lex_decode_arabic_escape(Lexer* l, unsigned char* out_byte)
{
    if (!l || !out_byte) return false;

    const unsigned char b0 = (unsigned char)l->state.cur_char[0];
    const unsigned char b1 = (unsigned char)l->state.cur_char[1];

    // \س => سطر جديد (LF)
    if (b0 == 0xD8 && b1 == 0xB3) {
        *out_byte = (unsigned char)'\n';
        l->state.cur_char += 2;
        l->state.col += 2;
        return true;
    }
    // \ت => Tab
    if (b0 == 0xD8 && b1 == 0xAA) {
        *out_byte = (unsigned char)'\t';
        l->state.cur_char += 2;
        l->state.col += 2;
        return true;
    }
    // \ر => Carriage Return
    if (b0 == 0xD8 && b1 == 0xB1) {
        *out_byte = (unsigned char)'\r';
        l->state.cur_char += 2;
        l->state.col += 2;
        return true;
    }
    // \٠ => NULL
    if (b0 == 0xD9 && b1 == 0xA0) {
        *out_byte = 0;
        l->state.cur_char += 2;
        l->state.col += 2;
        return true;
    }

    return false;
}

static bool lex_append_byte(char** buf, size_t* len, size_t* cap, unsigned char byte)
{
    if (!buf || !len || !cap || !*buf) return false;

    if (*len + 1 >= *cap) {
        size_t new_cap = (*cap < 32) ? 32 : (*cap * 2);
        char* new_buf = (char*)realloc(*buf, new_cap);
        if (!new_buf) return false;
        *buf = new_buf;
        *cap = new_cap;
    }

    (*buf)[(*len)++] = (char)byte;
    return true;
}

/**
 * @brief إنشاء Token لموقع محدد داخل المحلل اللفظي.
 */
static Token lex_make_token_at(const Lexer* l, int line, int col)
{
    Token t;
    memset(&t, 0, sizeof(t));
    t.type = TOKEN_INVALID;
    t.filename = (l && l->state.filename) ? l->state.filename : "unknown";
    t.line = (line > 0) ? line : 1;
    t.col = (col > 0) ? col : 1;
    return t;
}

static void lex_fatal(Lexer* l, const char* fmt, ...)
{
    int line = (l ? l->state.line : 1);
    int col = (l ? l->state.col : 1);

    char msg[512];
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    error_report(lex_make_token_at(l, line, col), "%s", msg);
    exit(1);
}

/**
 * @brief التحقق من صحة تسلسل UTF-8 بدءاً من `s`.
 *
 * عند النجاح:
 * - تُخزّن طول التسلسل في out_len (1..4)
 * - تُرجع true
 */
static bool lex_utf8_validate_at(const char* s, int* out_len)
{
    if (out_len) *out_len = 0;
    if (!s || !*s) return false;

    const unsigned char b0 = (unsigned char)s[0];
    if ((b0 & 0x80u) == 0x00u) {
        if (out_len) *out_len = 1;
        return true;
    }

    if ((b0 & 0xE0u) == 0xC0u) {
        const unsigned char b1 = (unsigned char)s[1];
        if (b1 == 0 || !is_utf8_cont_byte(b1)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        if (cp < 0x80u) return false; // overlong
        if (out_len) *out_len = 2;
        return true;
    }

    if ((b0 & 0xF0u) == 0xE0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        if (b1 == 0 || b2 == 0) return false;
        if (!is_utf8_cont_byte(b1) || !is_utf8_cont_byte(b2)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                      ((uint32_t)(b1 & 0x3Fu) << 6) |
                      (uint32_t)(b2 & 0x3Fu);
        if (cp < 0x800u) return false; // overlong
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false; // surrogate
        if (out_len) *out_len = 3;
        return true;
    }

    if ((b0 & 0xF8u) == 0xF0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        const unsigned char b3 = (unsigned char)s[3];
        if (b1 == 0 || b2 == 0 || b3 == 0) return false;
        if (!is_utf8_cont_byte(b1) || !is_utf8_cont_byte(b2) || !is_utf8_cont_byte(b3)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) |
                      ((uint32_t)(b1 & 0x3Fu) << 12) |
                      ((uint32_t)(b2 & 0x3Fu) << 6) |
                      (uint32_t)(b3 & 0x3Fu);
        if (cp < 0x10000u || cp > 0x10FFFFu) return false;
        if (out_len) *out_len = 4;
        return true;
    }

    return false;
}

// قراءة ملف (من main.c، يجب أن تكون متاحة للجميع الآن)
char* read_file(const char* path);

#define LEX_STDLIB_DIR "stdlib"
#define LEX_ENV_BAA_STDLIB "BAA_STDLIB"
#define LEX_ENV_BAA_HOME "BAA_HOME"

/**
 * @brief هل المسار يحتوي فواصل مجلدات؟
 */
static bool lex_path_has_separator(const char* path)
{
    if (!path) return false;
    for (size_t i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '/' || path[i] == '\\') return true;
    }
    return false;
}

/**
 * @brief هل المسار مطلق (Windows/Linux)؟
 */
static bool lex_path_is_absolute(const char* path)
{
    if (!path || !path[0]) return false;
    if (path[0] == '/' || path[0] == '\\') return true;
    if (isalpha((unsigned char)path[0]) && path[1] == ':') return true;
    return false;
}

/**
 * @brief نسخ نص على heap مع إنهاء NUL.
 */
static char* lex_strdup_heap(Lexer* l, const char* text)
{
    if (!l || !text) return NULL;
    size_t n = strlen(text);
    char* out = (char*)malloc(n + 1);
    if (!out) {
        lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء نسخ مسار التضمين.");
    }
    if (n) memcpy(out, text, n);
    out[n] = '\0';
    return out;
}

/**
 * @brief دمج مسارين مع فاصل مناسب.
 */
static char* lex_join_paths(Lexer* l, const char* base, const char* leaf)
{
    if (!l || !base || !leaf || !base[0] || !leaf[0]) return NULL;
    size_t base_len = strlen(base);
    size_t leaf_len = strlen(leaf);
    bool need_sep = (base[base_len - 1] != '/' && base[base_len - 1] != '\\');
    size_t total = base_len + (need_sep ? 1u : 0u) + leaf_len + 1u;

    char* out = (char*)malloc(total);
    if (!out) {
        lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء تركيب مسار التضمين.");
    }

    memcpy(out, base, base_len);
    size_t pos = base_len;
    if (need_sep) out[pos++] = '/';
    memcpy(out + pos, leaf, leaf_len);
    out[pos + leaf_len] = '\0';
    return out;
}

/**
 * @brief استخراج مسار مجلد الملف الحالي (مع الفاصل الأخير).
 */
static char* lex_dirname_from_path(Lexer* l, const char* path)
{
    if (!l || !path || !path[0]) return NULL;

    const char* sep_slash = strrchr(path, '/');
    const char* sep_backslash = strrchr(path, '\\');
    const char* sep = sep_slash;
    if (!sep || (sep_backslash && sep_backslash > sep)) {
        sep = sep_backslash;
    }
    if (!sep) return NULL;

    size_t dir_len = (size_t)(sep - path) + 1u;
    char* dir = (char*)malloc(dir_len + 1u);
    if (!dir) {
        lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء اشتقاق مجلد ملف التضمين.");
    }
    memcpy(dir, path, dir_len);
    dir[dir_len] = '\0';
    return dir;
}

/**
 * @brief محاولة قراءة ملف مرشح للتضمين.
 */
static char* lex_try_read_include_candidate(Lexer* l, const char* candidate, char** out_resolved_path)
{
    if (!l || !candidate || !candidate[0] || !out_resolved_path) return NULL;

    FILE* probe = fopen(candidate, "rb");
    if (!probe) return NULL;
    fclose(probe);

    char* source = read_file(candidate);
    if (!source) return NULL;

    *out_resolved_path = lex_strdup_heap(l, candidate);
    return source;
}

/**
 * @brief حل مسار #تضمين وقراءة الملف من أقرب مسار مدعوم.
 *
 * ترتيب البحث:
 * 1) المسار النسبي من مجلد الملف الحالي (مثل C local include).
 * 2) كما كُتب في الكود.
 * 3) إن كان نسبياً: تحت BAA_HOME.
 * 4) مسارات -I من سطر الأوامر بالترتيب.
 * 5) إن كان اسماً مجرداً (بدون / أو \\):
 *    stdlib/ من مجلد الملف الحالي، ثم stdlib/ محلي، ثم BAA_STDLIB، ثم BAA_HOME/stdlib.
 */
static char* lex_resolve_and_read_include(Lexer* l, const char* requested_path, char** out_resolved_path)
{
    if (!l || !requested_path || !requested_path[0] || !out_resolved_path) return NULL;
    *out_resolved_path = NULL;

    char* source_dir = lex_dirname_from_path(l, l->state.filename);
    if (source_dir && !lex_path_is_absolute(requested_path)) {
        char* source_rel_candidate = lex_join_paths(l, source_dir, requested_path);
        char* source = lex_try_read_include_candidate(l, source_rel_candidate, out_resolved_path);
        free(source_rel_candidate);
        if (source) {
            free(source_dir);
            return source;
        }
    }

    char* source = lex_try_read_include_candidate(l, requested_path, out_resolved_path);
    if (source) {
        free(source_dir);
        return source;
    }

    const char* baa_home = getenv(LEX_ENV_BAA_HOME);
    if (!lex_path_is_absolute(requested_path) && baa_home && baa_home[0]) {
        char* home_candidate = lex_join_paths(l, baa_home, requested_path);
        source = lex_try_read_include_candidate(l, home_candidate, out_resolved_path);
        free(home_candidate);
        if (source) {
            free(source_dir);
            return source;
        }
    }

    if (!lex_path_is_absolute(requested_path) && l->include_dirs && l->include_dir_count > 0) {
        for (size_t i = 0; i < l->include_dir_count; ++i) {
            const char* include_dir = l->include_dirs[i];
            if (!include_dir || !include_dir[0]) continue;
            char* include_candidate = lex_join_paths(l, include_dir, requested_path);
            source = lex_try_read_include_candidate(l, include_candidate, out_resolved_path);
            free(include_candidate);
            if (source) {
                free(source_dir);
                return source;
            }
        }
    }

    if (lex_path_has_separator(requested_path)) {
        free(source_dir);
        return NULL;
    }

    if (source_dir) {
        char* source_stdlib = lex_join_paths(l, source_dir, LEX_STDLIB_DIR);
        char* source_stdlib_candidate = lex_join_paths(l, source_stdlib, requested_path);
        source = lex_try_read_include_candidate(l, source_stdlib_candidate, out_resolved_path);
        free(source_stdlib_candidate);
        free(source_stdlib);
        if (source) {
            free(source_dir);
            return source;
        }
    }

    char* cwd_stdlib = lex_join_paths(l, LEX_STDLIB_DIR, requested_path);
    source = lex_try_read_include_candidate(l, cwd_stdlib, out_resolved_path);
    free(cwd_stdlib);
    if (source) {
        free(source_dir);
        return source;
    }

    const char* baa_stdlib = getenv(LEX_ENV_BAA_STDLIB);
    if (baa_stdlib && baa_stdlib[0]) {
        char* stdlib_candidate = lex_join_paths(l, baa_stdlib, requested_path);
        source = lex_try_read_include_candidate(l, stdlib_candidate, out_resolved_path);
        free(stdlib_candidate);
        if (source) {
            free(source_dir);
            return source;
        }
    }

    if (baa_home && baa_home[0]) {
        char* home_stdlib = lex_join_paths(l, baa_home, LEX_STDLIB_DIR);
        char* home_stdlib_candidate = lex_join_paths(l, home_stdlib, requested_path);
        source = lex_try_read_include_candidate(l, home_stdlib_candidate, out_resolved_path);
        free(home_stdlib_candidate);
        free(home_stdlib);
        if (source) {
            free(source_dir);
            return source;
        }
    }

    free(source_dir);
    return NULL;
}

static void lex_skip_utf8_bom(LexerState* state)
{
    if (!state || !state->cur_char) return;

    char* s = state->cur_char;
    if ((unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF) {
        state->cur_char += 3;
    }
}

/**
 * @brief تهيئة المحلل اللفظي بنص المصدر وتخطي الـ BOM إذا وجد.
 */
void lexer_init(Lexer* l,
                char* src,
                const char* filename,
                const char* const* include_dirs,
                size_t include_dir_count) {
    l->stack_depth = 0;
    l->macro_count = 0;
    l->skipping = false;
    l->if_depth = 0;
    l->include_dirs = include_dirs;
    l->include_dir_count = include_dir_count;
    
    // تهيئة الحالة الحالية
    l->state.source = src;
    l->state.filename = filename; // تخزين اسم الملف
    l->state.line = 1;
    l->state.col = 1;
    l->state.cur_char = src;

    // تخطي علامة ترتيب البايت (BOM) لملفات UTF-8 إذا كانت موجودة
    lex_skip_utf8_bom(&l->state);

    error_register_source(filename, src);
}

static bool pp_active(const Lexer* l)
{
    if (!l) return true;
    if (l->if_depth <= 0) return true;
    const int i = l->if_depth - 1;
    const unsigned char parent_active = l->if_stack[i].parent_active;
    const unsigned char cond_true = l->if_stack[i].cond_true;
    const unsigned char in_else = l->if_stack[i].in_else;
    if (!parent_active) return false;
    return in_else ? (!cond_true) : (cond_true != 0);
}

static void pp_recompute_skipping(Lexer* l)
{
    if (!l) return;
    l->skipping = pp_active(l) ? false : true;
}

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
// مساعد: إرجاع المحرف الحالي دون تحريك المؤشر
char peek(Lexer* l) { return *l->state.cur_char; }
// مساعد: إرجاع المحرف التالي دون تحريك المؤشر
char peek_next(Lexer* l) {
    if (!l) return '\0';
    if (!l->state.cur_char) return '\0';
    if (*l->state.cur_char == '\0') return '\0';
    return *(l->state.cur_char + 1);
}

/**
 * @brief إضافة تعريف ماكرو جديد.
 */
void add_macro(Lexer* l, char* name, char* value) {
    if (l->macro_count >= 100) {
        lex_fatal(l, "خطأ قبلي: عدد تعريفات الماكرو تجاوز الحد الأقصى (100).");
    }
    l->macros[l->macro_count].name = strdup(name);
    l->macros[l->macro_count].value = strdup(value);
    if (!l->macros[l->macro_count].name || !l->macros[l->macro_count].value) {
        lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء إضافة ماكرو.");
    }
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
                 if (!directive) {
                     lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة التوجيه.");
                 }
                 if (len) memcpy(directive, dir_start, len);
                 directive[len] = '\0';
                 
                 // 1. #تضمين (Include)
                 if (strcmp(directive, "تضمين") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     if (peek(l) != '"') {
                         lex_fatal(l, "خطأ قبلي: متوقع اسم ملف نصي بعد #تضمين.");
                     }
                     advance_pos(l); // skip "
                     char* path_start = l->state.cur_char;
                     while (peek(l) != '"' && peek(l) != '\0') advance_pos(l);
                     size_t path_len = l->state.cur_char - path_start;
                     char* path = malloc(path_len + 1);
                     if (!path) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة مسار #تضمين.");
                     }
                     if (path_len) memcpy(path, path_start, path_len);
                     path[path_len] = '\0';
                     advance_pos(l); // skip "

                     char* resolved_include_path = NULL;
                     char* new_src = lex_resolve_and_read_include(l, path, &resolved_include_path);
                     if (!new_src || !resolved_include_path) {
                         lex_fatal(l,
                                   "خطأ قبلي: تعذر تضمين الملف '%s'. "
                                   "المسارات المدعومة: مسار الملف الحالي، المسار المباشر، "
                                   "BAA_HOME، مسارات -I، stdlib/، BAA_STDLIB.",
                                   path);
                     }
                      
                     size_t max_include_depth = sizeof(l->stack) / sizeof(l->stack[0]);
                     if ((size_t)l->stack_depth >= max_include_depth) {
                         free(new_src);
                         free(resolved_include_path);
                         lex_fatal(l, "خطأ قبلي: تم تجاوز عمق التضمين الأقصى (%zu).", max_include_depth);
                     }
                     l->stack[l->stack_depth++] = l->state;
                      
                     l->state.source = new_src;
                     l->state.cur_char = new_src;
                     l->state.filename = resolved_include_path;
                     l->state.line = 1;
                     l->state.col = 1;
                     lex_skip_utf8_bom(&l->state);
                      error_register_source(l->state.filename, new_src);
                     
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
                     if (!name) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة اسم ماكرو.");
                     }
                     if (name_len) memcpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     // قراءة القيمة (باقي السطر)
                     while (peek(l) != '\0' && isspace(peek(l)) && peek(l) != '\n') advance_pos(l);
                     char* val_start = l->state.cur_char;
                     while (peek(l) != '\n' && peek(l) != '\0' && peek(l) != '\r') advance_pos(l); // لنهاية السطر
                     size_t val_len = l->state.cur_char - val_start;
                     char* val = malloc(val_len + 1);
                     if (!val) {
                         free(name);
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة قيمة ماكرو.");
                     }
                     if (val_len) memcpy(val, val_start, val_len);
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
                     if (!name) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة اسم #إذا_عرف.");
                     }
                     if (name_len) memcpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     bool defined = (get_macro_value(l, name) != NULL);

                     if (l->if_depth >= (int)(sizeof(l->if_stack) / sizeof(l->if_stack[0]))) {
                         free(name);
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: تجاوزت الشروط المتداخلة الحد الأقصى.");
                     }

                     bool parent = pp_active(l);
                     l->if_stack[l->if_depth].parent_active = parent ? 1 : 0;
                     l->if_stack[l->if_depth].cond_true = (parent && defined) ? 1 : 0;
                     l->if_stack[l->if_depth].in_else = 0;
                     l->if_depth++;
                     pp_recompute_skipping(l);
                     
                     free(name);
                     free(directive);
                     continue;
                 }
                 // 4. #وإلا (Else)
                 else if (strcmp(directive, "وإلا") == 0) {
                     if (l->if_depth <= 0) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: #وإلا بدون #إذا_عرف مطابق.");
                     }
                     int i = l->if_depth - 1;
                     if (l->if_stack[i].in_else) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: تكرار #وإلا داخل نفس الكتلة.");
                     }
                     l->if_stack[i].in_else = 1;
                     pp_recompute_skipping(l);
                     free(directive);
                     continue;
                 }
                 // 5. #نهاية (End)
                 else if (strcmp(directive, "نهاية") == 0) {
                     if (l->if_depth <= 0) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: #نهاية بدون #إذا_عرف مطابق.");
                     }
                     l->if_depth--;
                     pp_recompute_skipping(l);
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
                     if (!name) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة اسم #الغاء_تعريف.");
                     }
                     if (name_len) memcpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     remove_macro(l, name);
                     free(name);
                     free(directive);
                     continue;
                 }
                 
                 free(directive);
             }
             lex_fatal(l, "خطأ قبلي: توجيه غير معروف.");
        }

        // نهاية الملف EOF
        if (peek(l) == '\0') {
            if (l->if_depth > 0 && l->stack_depth == 0) {
                lex_fatal(l, "خطأ قبلي: نهاية الملف قبل إغلاق #إذا_عرف (مفقود #نهاية).");
            }

            // إذا كنا داخل ملف مضمن، نعود للملف السابق (Pop)
            if (l->stack_depth > 0) {
                free(l->state.source);
                free((void*)l->state.filename);
                l->state = l->stack[--l->stack_depth];
                continue; 
            }
            token.type = TOKEN_EOF;
            token.filename = l->state.filename;
            token.line = l->state.line;
            token.col = l->state.col;
            return token;
        }

        // إذا كنا في وضع التخطي، نتجاهل كل شيء حتى نجد #
        if (l->skipping) {
            advance_pos(l);
            continue;
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

        size_t cap = 32;
        size_t len = 0;
        char* str = (char*)malloc(cap);
        if (!str) {
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء قراءة النص.");
        }

        while (peek(l) != '"' && peek(l) != '\0') {
            if (peek(l) == '\\') {
                advance_pos(l); // تخطي '\'
                if (peek(l) == '\0') {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: تسلسل هروب غير مكتمل داخل النص.");
                }

                unsigned char out = 0;
                if (peek(l) == '\\') { out = (unsigned char)'\\'; advance_pos(l); }
                else if (peek(l) == '"') { out = (unsigned char)'"'; advance_pos(l); }
                else if (peek(l) == '\'') { out = (unsigned char)'\''; advance_pos(l); }
                else if (lex_decode_arabic_escape(l, &out)) { /* done */ }
                else {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: تسلسل هروب غير مدعوم داخل النص.");
                }

                if (!lex_append_byte(&str, &len, &cap, out)) {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء النص.");
                }
                continue;
            }

            unsigned char b0 = (unsigned char)peek(l);
            if (b0 >= 0x80u) {
                int ulen = 0;
                if (!lex_utf8_validate_at(l->state.cur_char, &ulen)) {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: تسلسل UTF-8 غير صالح داخل النص.");
                }
                for (int i = 0; i < ulen; i++) {
                    if (!lex_append_byte(&str, &len, &cap, (unsigned char)*l->state.cur_char)) {
                        free(str);
                        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء النص.");
                    }
                    advance_pos(l);
                }
                continue;
            }

            if (!lex_append_byte(&str, &len, &cap, b0)) {
                free(str);
                lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء النص.");
            }
            advance_pos(l);
        }

        if (peek(l) == '\0') {
            free(str);
            lex_fatal(l, "خطأ لفظي: النص غير مُغلق.");
        }

        if (!lex_append_byte(&str, &len, &cap, 0)) {
            free(str);
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء إنهاء النص.");
        }

        advance_pos(l); // تخطي " النهاية
        token.type = TOKEN_STRING;
        token.value = str;
        return token;
    }

    // --- معالجة الحروف ('...') ---
    if (*current == '\'') {
        advance_pos(l);

        unsigned char bytes[4];
        int blen = 0;

        if (peek(l) == '\\')
        {
            // تسلسلات الهروب (عربية + بنيوية)
            advance_pos(l);
            unsigned char esc0 = (unsigned char)peek(l);
            if (esc0 == '\0') {
                lex_fatal(l, "خطأ لفظي: الحرف الحرفي غير مُغلق.");
            }

            if (esc0 == '\\') { bytes[0] = '\\'; blen = 1; advance_pos(l); }
            else if (esc0 == '\'') { bytes[0] = '\''; blen = 1; advance_pos(l); }
            else if (esc0 == '"') { bytes[0] = '"'; blen = 1; advance_pos(l); }
            else if (lex_decode_arabic_escape(l, &bytes[0])) { blen = 1; }
            else
            {
                lex_fatal(l, "خطأ لفظي: تسلسل هروب غير معروف داخل الحرف الحرفي.");
            }
        }
        else
        {
            int ulen = 0;
            if (!lex_utf8_validate_at(l->state.cur_char, &ulen)) {
                lex_fatal(l, "خطأ لفظي: UTF-8 غير صالح داخل الحرف الحرفي.");
            }
            for (int i = 0; i < ulen; i++) {
                bytes[i] = (unsigned char)*l->state.cur_char;
                advance_pos(l);
            }
            blen = ulen;
        }

        if (peek(l) != '\'')
        {
            lex_fatal(l, "خطأ لفظي: متوقع إغلاق الحرف الحرفي بعلامة ' .");
        }
        advance_pos(l);

        token.type = TOKEN_CHAR;
        char* val = malloc((size_t)blen + 1);
        if (!val) { lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء إنشاء الحرف الحرفي."); }
        if (blen) memcpy(val, bytes, (size_t)blen);
        val[blen] = '\0';
        token.value = val;
        return token;
    }

    // معالجة الفاصلة المنقوطة العربية (؛)
    if ((unsigned char)*current == 0xD8 && (unsigned char)*(l->state.cur_char+1) == 0x9B) {
        token.type = TOKEN_SEMICOLON;
        l->state.cur_char += 2; l->state.col += 2; 
        return token;
    }

    // معالجة الفاصلة العربية (،)
    if ((unsigned char)*current == 0xD8 && (unsigned char)*(l->state.cur_char + 1) == 0x8C) {
        token.type = TOKEN_COMMA;
        l->state.cur_char += 2;
        l->state.col += 2;
        return token;
    }

    // معالجة الرموز والعمليات
    if (*current == '.') {
        if (*(l->state.cur_char + 1) == '.' && *(l->state.cur_char + 2) == '.') {
            token.type = TOKEN_ELLIPSIS;
            l->state.cur_char += 3;
            l->state.col += 3;
            return token;
        }
        token.type = TOKEN_DOT;
        advance_pos(l);
        return token;
    }
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

    // معالجة العمليات المنطقية/البتية (&&، ||، !، &، |، ^، ~)
    if (*current == '&') {
        if (*(l->state.cur_char+1) == '&') {
            token.type = TOKEN_AND; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_AMP; advance_pos(l); return token;
    }
    if (*current == '|') {
        if (*(l->state.cur_char+1) == '|') {
            token.type = TOKEN_OR; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_PIPE; advance_pos(l); return token;
    }
    if (*current == '^') {
        token.type = TOKEN_CARET; advance_pos(l); return token;
    }
    if (*current == '~') {
        token.type = TOKEN_TILDE; advance_pos(l); return token;
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
        if (*(l->state.cur_char+1) == '<') {
            token.type = TOKEN_SHL; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_LTE; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_LT; advance_pos(l); return token;
    }
    if (*current == '>') {
        if (*(l->state.cur_char+1) == '>') {
            token.type = TOKEN_SHR; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_GTE; l->state.cur_char += 2; l->state.col += 2; return token;
        }
        token.type = TOKEN_GT; advance_pos(l); return token;
    }

    // معالجة الأرقام
    // ملاحظة: is_arabic_digit تتوقع مؤشر char*، لذا نمرر current.
    // ملاحظة: current هنا مؤشر آمن ضمن المصدر الحالي.
    
    if (isdigit((unsigned char)*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        char buffer[64] = {0}; 
        int buf_idx = 0;
        
        while (1) {
            char* c = l->state.cur_char;
            if (isdigit((unsigned char)*c)) { 
                if (buf_idx >= (int)sizeof(buffer) - 1) {
                    lex_fatal(l, "خطأ لفظي: العدد الصحيح أطول من الحد المسموح.");
                }
                buffer[buf_idx++] = *c;
                advance_pos(l); 
            } 
            else if (is_arabic_digit(c)) {
                if (buf_idx >= (int)sizeof(buffer) - 1) {
                    lex_fatal(l, "خطأ لفظي: العدد الصحيح أطول من الحد المسموح.");
                }
                buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                l->state.cur_char += 2; l->state.col += 2;
            } else { break; }
        }

        // رقم عشري: جزء كسري بعد '.' متبوعاً برقم
        if (peek(l) == '.' && (isdigit((unsigned char)peek_next(l)) || is_arabic_digit(l->state.cur_char + 1)))
        {
            if (buf_idx >= (int)sizeof(buffer) - 1) {
                lex_fatal(l, "خطأ لفظي: العدد العشري أطول من الحد المسموح.");
            }
            buffer[buf_idx++] = '.';
            advance_pos(l); // تخطي '.'

            while (1) {
                char* c = l->state.cur_char;
                if (isdigit((unsigned char)*c)) {
                    if (buf_idx >= (int)sizeof(buffer) - 1) {
                        lex_fatal(l, "خطأ لفظي: العدد العشري أطول من الحد المسموح.");
                    }
                    buffer[buf_idx++] = *c;
                    advance_pos(l);
                }
                else if (is_arabic_digit(c)) {
                    if (buf_idx >= (int)sizeof(buffer) - 1) {
                        lex_fatal(l, "خطأ لفظي: العدد العشري أطول من الحد المسموح.");
                    }
                    buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                    l->state.cur_char += 2; l->state.col += 2;
                } else { break; }
            }

            token.type = TOKEN_FLOAT;
            token.value = strdup(buffer);
            return token;
        }

        token.value = strdup(buffer);
        return token;
    }

    // معالجة المعرفات والكلمات المفتاحية
    if (is_arabic_start_byte(*current)) {
        char* start = l->state.cur_char;
        while (!isspace(peek(l)) && peek(l) != '\0' && 
               strchr(".+-,=:(){}[]!<>*/%&|^~\"'", peek(l)) == NULL) {
            // تحقق من الفاصلة المنقوطة العربية (؛)
            if ((unsigned char)*l->state.cur_char == 0xD8 && (unsigned char)*(l->state.cur_char+1) == 0x9B) {
                break;
            }
            // تحقق من الفاصلة العربية (،)
            if ((unsigned char)*l->state.cur_char == 0xD8 && (unsigned char)*(l->state.cur_char + 1) == 0x8C) {
                break;
            }
            unsigned char b0 = (unsigned char)*l->state.cur_char;
            if (b0 >= 0x80u) {
                int ulen = 0;
                if (!lex_utf8_validate_at(l->state.cur_char, &ulen)) {
                    lex_fatal(l, "خطأ لفظي: UTF-8 غير صالح داخل المعرّف.");
                }
                for (int i = 0; i < ulen; i++) {
                    advance_pos(l);
                }
            } else {
                advance_pos(l);
            }
        }
        
        size_t len = l->state.cur_char - start;
        char* word = malloc(len + 1);
        if (!word) {
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ المعرّف.");
        }
        if (len) memcpy(word, start, len);
        word[len] = '\0';

        // 1. التحقق من استبدال الماكرو
        char* macro_val = get_macro_value(l, word);
        if (macro_val != NULL) {
            // استبدال الرمز بقيمة الماكرو
            if (macro_val[0] == '"') {
                token.type = TOKEN_STRING;
                // إزالة علامات التنصيص
                size_t vlen = strlen(macro_val);
                if (vlen >= 2) {
                    char* val = malloc(vlen - 1);
                    if (!val) {
                        free(word);
                        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                    }
                    if (vlen > 2) memcpy(val, macro_val + 1, vlen - 2);
                    val[vlen - 2] = '\0';
                    token.value = val;
                } else {
                    token.value = strdup("");
                    if (!token.value) {
                        free(word);
                        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                    }
                }
            } 
            else if (isdigit((unsigned char)macro_val[0]) || is_arabic_digit(macro_val)) {
                token.type = TOKEN_INT;
                token.value = strdup(macro_val);
                if (!token.value) {
                    free(word);
                    lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                }
            }
            else {
                token.type = TOKEN_IDENTIFIER;
                token.value = strdup(macro_val);
                if (!token.value) {
                    free(word);
                    lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                }
            }
            free(word);
            return token;
        }

        // 2. الكلمات المفتاحية المحجوزة
        if (strcmp(word, "إرجع") == 0) token.type = TOKEN_RETURN;
        else if (strcmp(word, "اطبع") == 0) token.type = TOKEN_PRINT;
        else if (strcmp(word, "اقرأ") == 0) token.type = TOKEN_READ;
        else if (strcmp(word, "مجمع") == 0) token.type = TOKEN_ASM;
        else if (strcmp(word, "صحيح") == 0) token.type = TOKEN_KEYWORD_INT;
        else if (strcmp(word, "ص٨") == 0) token.type = TOKEN_KEYWORD_I8;
        else if (strcmp(word, "ص١٦") == 0) token.type = TOKEN_KEYWORD_I16;
        else if (strcmp(word, "ص٣٢") == 0) token.type = TOKEN_KEYWORD_I32;
        else if (strcmp(word, "ص٦٤") == 0) token.type = TOKEN_KEYWORD_I64;
        else if (strcmp(word, "ط٨") == 0) token.type = TOKEN_KEYWORD_U8;
        else if (strcmp(word, "ط١٦") == 0) token.type = TOKEN_KEYWORD_U16;
        else if (strcmp(word, "ط٣٢") == 0) token.type = TOKEN_KEYWORD_U32;
        else if (strcmp(word, "ط٦٤") == 0) token.type = TOKEN_KEYWORD_U64;
        else if (strcmp(word, "نص") == 0) token.type = TOKEN_KEYWORD_STRING;
        else if (strcmp(word, "منطقي") == 0) token.type = TOKEN_KEYWORD_BOOL;
        else if (strcmp(word, "حرف") == 0) token.type = TOKEN_KEYWORD_CHAR;
        else if (strcmp(word, "عشري") == 0) token.type = TOKEN_KEYWORD_FLOAT;
        else if (strcmp(word, "عشري٣٢") == 0) token.type = TOKEN_KEYWORD_FLOAT32;
        else if (strcmp(word, "عدم") == 0) token.type = TOKEN_KEYWORD_VOID;
        else if (strcmp(word, "كـ") == 0) token.type = TOKEN_CAST;
        else if (strcmp(word, "حجم") == 0) token.type = TOKEN_SIZEOF;
        else if (strcmp(word, "ثابت") == 0) token.type = TOKEN_CONST;
        else if (strcmp(word, "ساكن") == 0) token.type = TOKEN_STATIC;
        else if (strcmp(word, "إذا") == 0) token.type = TOKEN_IF;
        else if (strcmp(word, "وإلا") == 0) token.type = TOKEN_ELSE;
        else if (strcmp(word, "طالما") == 0) token.type = TOKEN_WHILE;
        else if (strcmp(word, "لكل") == 0) token.type = TOKEN_FOR;
        else if (strcmp(word, "توقف") == 0) token.type = TOKEN_BREAK;
        else if (strcmp(word, "استمر") == 0) token.type = TOKEN_CONTINUE;
        else if (strcmp(word, "اختر") == 0) token.type = TOKEN_SWITCH;
        else if (strcmp(word, "حالة") == 0) token.type = TOKEN_CASE;
        else if (strcmp(word, "افتراضي") == 0) token.type = TOKEN_DEFAULT;
        else if (strcmp(word, "صواب") == 0) token.type = TOKEN_TRUE;
        else if (strcmp(word, "خطأ") == 0) token.type = TOKEN_FALSE;
        else if (strcmp(word, "تعداد") == 0) token.type = TOKEN_ENUM;
        else if (strcmp(word, "هيكل") == 0) token.type = TOKEN_STRUCT;
        else if (strcmp(word, "اتحاد") == 0) token.type = TOKEN_UNION;
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
    lex_fatal(l, "خطأ لفظي: بايت غير معروف 0x%02X.", (unsigned char)*current);
    return token;
}

/**
 * @brief تحويل نوع الوحدة إلى نص مقروء (لأغراض التنقيح ورسائل الخطأ).
 */
const char* token_type_to_str(BaaTokenType type) {
    switch (type) {
        case TOKEN_EOF: return "نهاية_الملف";
        case TOKEN_INT: return "عدد_صحيح";
        case TOKEN_FLOAT: return "عدد_عشري";
        case TOKEN_STRING: return "نص";
        case TOKEN_CHAR: return "حرف";
        case TOKEN_IDENTIFIER: return "معرّف";
        
        // كلمات مفتاحية
        case TOKEN_KEYWORD_INT: return "صحيح";
        case TOKEN_KEYWORD_I8: return "ص٨";
        case TOKEN_KEYWORD_I16: return "ص١٦";
        case TOKEN_KEYWORD_I32: return "ص٣٢";
        case TOKEN_KEYWORD_I64: return "ص٦٤";
        case TOKEN_KEYWORD_U8: return "ط٨";
        case TOKEN_KEYWORD_U16: return "ط١٦";
        case TOKEN_KEYWORD_U32: return "ط٣٢";
        case TOKEN_KEYWORD_U64: return "ط٦٤";
        case TOKEN_KEYWORD_STRING: return "نص";
        case TOKEN_KEYWORD_BOOL: return "منطقي";
        case TOKEN_KEYWORD_CHAR: return "حرف";
        case TOKEN_KEYWORD_FLOAT: return "عشري";
        case TOKEN_KEYWORD_FLOAT32: return "عشري٣٢";
        case TOKEN_KEYWORD_VOID: return "عدم";
        case TOKEN_CAST: return "كـ";
        case TOKEN_SIZEOF: return "حجم";
        case TOKEN_TYPE_ALIAS: return "نوع";
        case TOKEN_CONST: return "ثابت";
        case TOKEN_STATIC: return "ساكن";
        case TOKEN_RETURN: return "إرجع";
        case TOKEN_PRINT: return "اطبع";
        case TOKEN_READ: return "اقرأ";
        case TOKEN_ASM: return "مجمع";
        case TOKEN_IF: return "إذا";
        case TOKEN_ELSE: return "وإلا";
        case TOKEN_WHILE: return "طالما";
        case TOKEN_FOR: return "لكل";
        case TOKEN_BREAK: return "توقف";
        case TOKEN_CONTINUE: return "استمر";
        case TOKEN_SWITCH: return "اختر";
        case TOKEN_CASE: return "حالة";
        case TOKEN_DEFAULT: return "افتراضي";
        case TOKEN_TRUE: return "صواب";
        case TOKEN_FALSE: return "خطأ";
        case TOKEN_ENUM: return "تعداد";
        case TOKEN_STRUCT: return "هيكل";
        case TOKEN_UNION: return "اتحاد";
        
        // رموز
        case TOKEN_ASSIGN: return "=";
        case TOKEN_DOT: return ".";
        case TOKEN_ELLIPSIS: return "...";
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
        case TOKEN_AMP: return "&";
        case TOKEN_PIPE: return "|";
        case TOKEN_CARET: return "^";
        case TOKEN_TILDE: return "~";
        case TOKEN_SHL: return "<<";
        case TOKEN_SHR: return ">>";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        
        default: return "UNKNOWN";
    }
}
