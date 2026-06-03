/**
 * @file lexer.c
 * @brief جسر المحلل اللفظي المكتوب بباء مع واجهة الواجهة الأمامية المكتوبة بـ C.
 * @version 0.9.1.5
 */

#include "frontend_internal.h"

#include <ctype.h>
#include <stdarg.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define LEX_PATH_BUFFER_SIZE PATH_MAX

void lex_baa_bridge_init(Lexer* l, char* src, const char* filename);
void lex_baa_bridge_cleanup(Lexer* l);

static int is_utf8_cont_byte(unsigned char b)
{
    return ((b & 0xC0u) == 0x80u);
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
    t.length = 1;
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
        if (cp < 0x80u) return false;
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
        if (cp < 0x800u) return false;
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false;
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
 * @brief هل المسار مطلق على المنصة الحالية؟
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
 * @brief استخراج مسار مجلد الملف الحالي.
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
 * @brief توحيد فواصل المسار وتمثيل حرف القرص على ويندوز.
 */
static void lex_normalize_path_text(char* path)
{
    if (!path) return;

    for (size_t i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }

#ifdef _WIN32
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        path[0] = (char)tolower((unsigned char)path[0]);
    }
#endif
}

/**
 * @brief تطبيع مسار موجود فعلياً إلى مسار مطلق ثابت قدر الإمكان.
 */
static char* lex_normalize_existing_path(Lexer* l, const char* path)
{
    if (!l || !path || !path[0]) return NULL;

    char resolved[LEX_PATH_BUFFER_SIZE];
    char* out = NULL;

#ifdef _WIN32
    if (_fullpath(resolved, path, sizeof(resolved)) != NULL) {
        out = lex_strdup_heap(l, resolved);
    }
#else
    if (realpath(path, resolved) != NULL) {
        out = lex_strdup_heap(l, resolved);
    }
#endif

    if (!out) {
        out = lex_strdup_heap(l, path);
    }

    lex_normalize_path_text(out);
    return out;
}

/**
 * @brief تسجيل تبعية بناء مطبعة بدون تكرار.
 */
static void lex_record_dependency(Lexer* l, const char* resolved_path)
{
    if (!l || !resolved_path || !resolved_path[0]) return;

    for (size_t i = 0; i < l->dependency_count; ++i) {
        if (l->dependency_paths[i] && strcmp(l->dependency_paths[i], resolved_path) == 0) {
            return;
        }
    }

    if (l->dependency_count >= l->dependency_capacity) {
        size_t new_cap = (l->dependency_capacity == 0) ? 8u : (l->dependency_capacity * 2u);
        char** new_paths = (char**)realloc(l->dependency_paths, new_cap * sizeof(char*));
        if (!new_paths) {
            lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء تسجيل تبعيات البناء.");
        }
        l->dependency_paths = new_paths;
        l->dependency_capacity = new_cap;
    }

    l->dependency_paths[l->dependency_count] = lex_strdup_heap(l, resolved_path);
    l->dependency_count++;
}

/**
 * @brief مقارنة مسارين بعد التطبيع حسب قواعد المنصة.
 */
static bool lex_paths_equivalent(Lexer* l, const char* lhs, const char* rhs)
{
    if (!l || !lhs || !rhs) return false;

    char* lhs_norm = lex_normalize_existing_path(l, lhs);
    char* rhs_norm = lex_normalize_existing_path(l, rhs);
    if (!lhs_norm || !rhs_norm) {
        free(lhs_norm);
        free(rhs_norm);
        return false;
    }

#ifdef _WIN32
    bool same = (_stricmp(lhs_norm, rhs_norm) == 0);
#else
    bool same = (strcmp(lhs_norm, rhs_norm) == 0);
#endif

    free(lhs_norm);
    free(rhs_norm);
    return same;
}

/**
 * @brief بناء رسالة تشخيص عربية لسلسلة دورة التضمين.
 */
static void lex_fatal_include_cycle(Lexer* l, const char* requested_path, const char* resolved_path)
{
    if (!l || !resolved_path) {
        lex_fatal(l, "خطأ قبلي: تم اكتشاف دورة تضمين.");
        return;
    }

    char chain[2048];
    int n = snprintf(chain, sizeof(chain),
                     "خطأ قبلي: تم اكتشاف دورة تضمين عند '%s'. سلسلة التضمين: ",
                     requested_path ? requested_path : resolved_path);
    if (n < 0) {
        lex_fatal(l, "خطأ قبلي: تم اكتشاف دورة تضمين.");
        return;
    }

    size_t used = (size_t)n;
    for (int i = 0; i < l->stack_depth; ++i) {
        const char* filename = l->stack[i].filename ? l->stack[i].filename : "<unknown>";
        n = snprintf(chain + used, (used < sizeof(chain)) ? sizeof(chain) - used : 0u,
                     "%s -> ", filename);
        if (n < 0) break;
        used += (size_t)n;
        if (used >= sizeof(chain)) break;
    }

    if (used < sizeof(chain)) {
        const char* current = l->state.filename ? l->state.filename : "<unknown>";
        n = snprintf(chain + used, sizeof(chain) - used, "%s -> %s", current, resolved_path);
        if (n > 0) {
            used += (size_t)n;
        }
    }

    lex_fatal(l, "%s", chain);
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

    *out_resolved_path = lex_normalize_existing_path(l, candidate);
    return source;
}

/**
 * @brief حل مسار #تضمين وقراءة الملف من أقرب مسار مدعوم.
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
                size_t include_dir_count)
{
    if (!l) return;

    l->stack_depth = 0;
    l->macro_count = 0;
    l->skipping = false;
    l->if_depth = 0;
    l->include_dirs = include_dirs;
    l->include_dir_count = include_dir_count;
    l->dependency_paths = NULL;
    l->dependency_count = 0;
    l->dependency_capacity = 0;
    l->baa_source_count = 0;

    l->state.source = src;
    l->state.filename = filename;
    l->state.line = 1;
    l->state.col = 1;
    l->state.cur_char = src;

    lex_skip_utf8_bom(&l->state);

    error_register_source(filename, src);

    char* root_dep = lex_normalize_existing_path(l, filename ? filename : "<unknown>");
    if (root_dep) {
        lex_record_dependency(l, root_dep);
        free(root_dep);
    }

    lex_baa_bridge_init(l, src, filename);
}

const char* const* lexer_get_dependencies(const Lexer* lexer, size_t* out_count)
{
    if (out_count) {
        *out_count = lexer ? lexer->dependency_count : 0u;
    }
    return lexer ? (const char* const*)lexer->dependency_paths : NULL;
}

void lexer_free_dependencies(Lexer* lexer)
{
    if (!lexer) return;
    lex_baa_bridge_cleanup(lexer);
    for (size_t i = 0; i < lexer->dependency_count; ++i) {
        free(lexer->dependency_paths[i]);
    }
    free(lexer->dependency_paths);
    lexer->dependency_paths = NULL;
    lexer->dependency_count = 0;
    lexer->dependency_capacity = 0;
}

/**
 * @brief تثبيت طول الوحدة قبل إرجاعها من المحلل اللفظي.
 */
static Token lex_finish_token(Lexer* l, Token token)
{
    if (!l) {
        if (token.length <= 0) token.length = 1;
        return token;
    }

    if (token.length <= 0 &&
        token.filename == l->state.filename &&
        token.line == l->state.line) {
        int len = l->state.col - token.col;
        token.length = (len > 0) ? len : 1;
    }

    if (token.length <= 0) token.length = 1;
    return token;
}

#include "lexer_baa_bridge.c"
#include "lexer_debug.c"
