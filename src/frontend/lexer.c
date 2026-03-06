/**
 * @file lexer.c
 * @brief يقوم بتحويل الكود المصدري المكتوب باللغة العربية (UTF-8) إلى وحدات لفظية (Tokens).
 * @version 0.2.9 (Input & UX Polish)
 */

#include "frontend_internal.h"
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

#include "lexer_tokens.c"
#include "lexer_debug.c"
