/**
 * @file error.c
 * @brief نظام التشخيص (Diagnostic Engine) - الأخطاء والتحذيرات.
 * @version 0.2.8
 */

#include "support_internal.h"
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

// ============================================================================
// ألوان ANSI (ANSI Colors)
// ============================================================================

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[31m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_BOLD_RED     "\033[1;31m"
#define ANSI_BOLD_YELLOW  "\033[1;33m"
#define ANSI_BOLD_CYAN    "\033[1;36m"

#define DIAGNOSTIC_MIN_CARET_WIDTH 1
#define DIAGNOSTIC_TAB_WIDTH 4

// ============================================================================
// حالة النظام (System State)
// ============================================================================

static const char* current_source = NULL;
static bool had_error = false;
static bool had_warning = false;
static int warning_count = 0;

typedef struct {
    char* filename;
    char* source;
} ErrorSourceEntry;

static ErrorSourceEntry* g_error_sources = NULL;
static int g_error_sources_count = 0;
static int g_error_sources_cap = 0;

// إعدادات التحذيرات العامة
WarningConfig g_warning_config;

// ============================================================================
// دوال مساعدة (Helper Functions)
// ============================================================================

/**
 * @brief التحقق من دعم الألوان في الطرفية.
 */
static bool supports_color(void) {
#ifdef _WIN32
    // تفعيل دعم ANSI في Windows 10+
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) return false;
    
    return true;
#else
    return isatty(fileno(stderr));
#endif
}

/**
 * @brief الحصول على اسم التحذير بالإنجليزية.
 */
static const char* warning_type_name(WarningType type) {
    switch (type) {
        case WARN_UNUSED_VARIABLE: return "unused-variable";
        case WARN_DEAD_CODE:       return "dead-code";
        case WARN_IMPLICIT_RETURN: return "implicit-return";
        case WARN_SHADOW_VARIABLE: return "shadow-variable";
        case WARN_IMPLICIT_NARROWING: return "implicit-narrowing";
        case WARN_SIGNED_UNSIGNED_COMPARE: return "signed-unsigned-compare";
        default:                   return "unknown";
    }
}

static void error_sources_clear(void) {
    if (g_error_sources) {
        for (int i = 0; i < g_error_sources_count; i++) {
            free(g_error_sources[i].filename);
            free(g_error_sources[i].source);
            g_error_sources[i].filename = NULL;
            g_error_sources[i].source = NULL;
        }
        free(g_error_sources);
    }

    g_error_sources = NULL;
    g_error_sources_count = 0;
    g_error_sources_cap = 0;
}

static const char* error_sources_lookup(const char* filename) {
    if (!filename || !filename[0]) return current_source;

    for (int i = 0; i < g_error_sources_count; i++) {
        if (g_error_sources[i].filename &&
            strcmp(g_error_sources[i].filename, filename) == 0) {
            return g_error_sources[i].source;
        }
    }

    return current_source;
}

static DiagnosticSpan diagnostic_span_normalize(DiagnosticSpan span)
{
    if (span.line <= 0) span.line = 1;
    if (span.col <= 0) span.col = 1;
    if (span.end_col <= span.col) span.end_col = span.col + DIAGNOSTIC_MIN_CARET_WIDTH;
    return span;
}

static DiagnosticSpan diagnostic_span_from_loc(const char* filename, int line, int col)
{
    DiagnosticSpan span;
    span.filename = filename;
    span.line = line;
    span.col = col;
    span.end_col = col + DIAGNOSTIC_MIN_CARET_WIDTH;
    return diagnostic_span_normalize(span);
}

static int diagnostic_utf8_len(const char* s)
{
    if (!s || !*s) return 0;
    const unsigned char b0 = (unsigned char)s[0];
    if ((b0 & 0x80u) == 0x00u) return 1;
    if ((b0 & 0xE0u) == 0xC0u) return (s[1] != '\0') ? 2 : 1;
    if ((b0 & 0xF0u) == 0xE0u) return (s[1] && s[2]) ? 3 : 1;
    if ((b0 & 0xF8u) == 0xF0u) return (s[1] && s[2] && s[3]) ? 4 : 1;
    return 1;
}

static int diagnostic_display_width_bytes(const char* start, int byte_count)
{
    if (!start || byte_count <= 0) return 0;

    int width = 0;
    int consumed = 0;
    while (consumed < byte_count && start[consumed] != '\0' && start[consumed] != '\n') {
        if (start[consumed] == '\t') {
            width += DIAGNOSTIC_TAB_WIDTH;
            consumed++;
            continue;
        }

        int len = diagnostic_utf8_len(start + consumed);
        if (len <= 0 || consumed + len > byte_count) len = 1;
        consumed += len;
        width++;
    }

    return width;
}

static void diagnostic_print_spaces(int count)
{
    for (int i = 0; i < count; i++) {
        fputc(' ', stderr);
    }
}

// ============================================================================
// تهيئة النظام (Initialization)
// ============================================================================

void error_init(const char* source) {
    error_sources_clear();
    current_source = source;
    had_error = false;
}

void error_register_source(const char* filename, const char* source)
{
    if (!filename || !filename[0] || !source) return;

    for (int i = 0; i < g_error_sources_count; i++) {
        if (g_error_sources[i].filename &&
            strcmp(g_error_sources[i].filename, filename) == 0) {
            char* src_copy = strdup(source);
            if (!src_copy) return;
            free(g_error_sources[i].source);
            g_error_sources[i].source = src_copy;
            return;
        }
    }

    if (g_error_sources_count >= g_error_sources_cap) {
        int new_cap = (g_error_sources_cap == 0) ? 8 : g_error_sources_cap * 2;
        ErrorSourceEntry* new_arr = (ErrorSourceEntry*)realloc(g_error_sources,
            (size_t)new_cap * sizeof(ErrorSourceEntry));
        if (!new_arr) return;
        g_error_sources = new_arr;
        g_error_sources_cap = new_cap;
    }

    char* name_copy = strdup(filename);
    if (!name_copy) return;
    char* src_copy = strdup(source);
    if (!src_copy) {
        free(name_copy);
        return;
    }

    g_error_sources[g_error_sources_count].filename = name_copy;
    g_error_sources[g_error_sources_count].source = src_copy;
    g_error_sources_count++;
}

void warning_init(void) {
    // تعطيل جميع التحذيرات افتراضياً
    for (int i = 0; i < WARN_COUNT; i++) {
        g_warning_config.enabled[i] = false;
    }
    g_warning_config.warnings_as_errors = false;
    g_warning_config.all_warnings = false;
    g_warning_config.colored_output = supports_color();
    
    had_warning = false;
    warning_count = 0;
}

// ============================================================================
// استعلام الحالة (State Queries)
// ============================================================================

bool error_has_occurred() {
    return had_error;
}

bool warning_has_occurred() {
    return had_warning;
}

int warning_get_count() {
    return warning_count;
}

void error_reset() {
    had_error = false;
}

void warning_reset() {
    had_warning = false;
    warning_count = 0;
}

// ============================================================================
// طباعة سياق الكود (Source Context Printing)
// ============================================================================

/**
 * @brief طباعة سطر الكود مع مؤشر نطاق.
 */
static void print_source_line(DiagnosticSpan span,
                              bool use_color,
                              const char* pointer_color) {
    span = diagnostic_span_normalize(span);

    const char* src = error_sources_lookup(span.filename);
    if (!src) return;

    // 1. البحث عن بداية السطر
    const char* start = src;
    int current_line = 1;
    while (current_line < span.line && *start != '\0') {
        if (*start == '\n') current_line++;
        start++;
    }

    // 2. البحث عن نهاية السطر
    const char* end = start;
    while (*end != '\n' && *end != '\0') {
        end++;
    }

    // 3. طباعة رقم السطر والسطر نفسه
    if (use_color) {
        fprintf(stderr, "\n    %s%4d |%s %.*s\n",
                ANSI_CYAN, span.line, ANSI_RESET, (int)(end - start), start);
    } else {
        fprintf(stderr, "\n    %4d | %.*s\n", span.line, (int)(end - start), start);
    }

    int line_bytes = (int)(end - start);
    int start_byte = span.col - 1;
    int end_byte = span.end_col - 1;
    if (start_byte < 0) start_byte = 0;
    if (start_byte > line_bytes) start_byte = line_bytes;
    if (end_byte <= start_byte) end_byte = start_byte + DIAGNOSTIC_MIN_CARET_WIDTH;
    if (end_byte > line_bytes && start_byte < line_bytes) end_byte = line_bytes;

    int pointer_spaces = diagnostic_display_width_bytes(start, start_byte);
    int caret_width = diagnostic_display_width_bytes(start + start_byte, end_byte - start_byte);
    if (caret_width < DIAGNOSTIC_MIN_CARET_WIDTH) caret_width = DIAGNOSTIC_MIN_CARET_WIDTH;

    // 4. طباعة المؤشر (^) بعرض النطاق.
    fprintf(stderr, "         ");
    diagnostic_print_spaces(pointer_spaces);
    
    if (use_color) {
        fprintf(stderr, "%s", pointer_color);
        for (int i = 0; i < caret_width; i++) fputc('^', stderr);
        fprintf(stderr, "%s ", ANSI_RESET);
    } else {
        for (int i = 0; i < caret_width; i++) fputc('^', stderr);
        fprintf(stderr, " ");
    }
}

static void print_hint_line(const char* hint, bool use_color)
{
    if (!hint || !hint[0]) return;

    if (use_color) {
        fprintf(stderr, "    %s= مساعدة:%s %s\n", ANSI_BOLD_CYAN, ANSI_RESET, hint);
    } else {
        fprintf(stderr, "    = مساعدة: %s\n", hint);
    }
}

// ============================================================================
// الإبلاغ عن الأخطاء (Error Reporting)
// ============================================================================

static void error_report_vspan(DiagnosticSpan span,
                               const char* hint,
                               const char* message,
                               va_list args)
{
    span = diagnostic_span_normalize(span);
    had_error = true;
    bool use_color = g_warning_config.colored_output;
    
    // طباعة رأس الخطأ
    if (use_color) {
        fprintf(stderr, "%s[Error]%s %s:%d:%d: ",
                ANSI_BOLD_RED, ANSI_RESET,
                span.filename ? span.filename : "unknown",
                span.line,
                span.col);
    } else {
        fprintf(stderr, "[Error] %s:%d:%d: ",
                span.filename ? span.filename : "unknown",
                span.line,
                span.col);
    }

    // طباعة الرسالة المنسقة
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    print_source_line(span, use_color, ANSI_BOLD_RED);
    fprintf(stderr, "\n");
    print_hint_line(hint, use_color);
}

void error_report_loc(const char* filename, int line, int col, const char* message, ...) {
    va_list args;
    va_start(args, message);
    error_report_vspan(diagnostic_span_from_loc(filename, line, col), NULL, message, args);
    va_end(args);
}

void error_report_span(DiagnosticSpan span, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(span, NULL, message, args);
    va_end(args);
}

void error_report_span_hint(DiagnosticSpan span, const char* hint, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(span, hint, message, args);
    va_end(args);
}

// ============================================================================
// الإبلاغ عن التحذيرات (Warning Reporting)
// ============================================================================

static void warning_report_vspan(WarningType type,
                                 DiagnosticSpan span,
                                 const char* message,
                                 va_list args)
{
    // التحقق من تفعيل التحذير
    if (!g_warning_config.all_warnings && !g_warning_config.enabled[type]) {
        return;
    }

    span = diagnostic_span_normalize(span);
    
    had_warning = true;
    warning_count++;
    bool use_color = g_warning_config.colored_output;
    
    // إذا كانت التحذيرات تُعامل كأخطاء
    if (g_warning_config.warnings_as_errors) {
        had_error = true;
    }
    
    // طباعة رأس التحذير
    const char* warn_name = warning_type_name(type);
    
    if (use_color) {
        if (g_warning_config.warnings_as_errors) {
            fprintf(stderr, "%s[Error]%s %s:%d:%d: ",
                    ANSI_BOLD_RED, ANSI_RESET,
                    span.filename ? span.filename : "unknown", span.line, span.col);
        } else {
            fprintf(stderr, "%s[Warning]%s %s:%d:%d: ",
                    ANSI_BOLD_YELLOW, ANSI_RESET,
                    span.filename ? span.filename : "unknown", span.line, span.col);
        }
        fprintf(stderr, "%s[-W%s]%s ", ANSI_CYAN, warn_name, ANSI_RESET);
    } else {
        if (g_warning_config.warnings_as_errors) {
            fprintf(stderr, "[Error] %s:%d:%d: ",
                    span.filename ? span.filename : "unknown", span.line, span.col);
        } else {
            fprintf(stderr, "[Warning] %s:%d:%d: ",
                    span.filename ? span.filename : "unknown", span.line, span.col);
        }
        fprintf(stderr, "[-W%s] ", warn_name);
    }

    // طباعة الرسالة المنسقة
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    const char* ptr_color = g_warning_config.warnings_as_errors ? ANSI_BOLD_RED : ANSI_BOLD_YELLOW;
    print_source_line(span, use_color, ptr_color);
    fprintf(stderr, "\n");
}

void warning_report(WarningType type, const char* filename, int line, int col, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    warning_report_vspan(type, diagnostic_span_from_loc(filename, line, col), message, args);
    va_end(args);
}

void warning_report_span(WarningType type, DiagnosticSpan span, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    warning_report_vspan(type, span, message, args);
    va_end(args);
}
