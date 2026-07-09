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
#define DIAG_CODE_SYNTAX_GENERIC "B0001"

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

typedef struct {
    char* code;
    char* severity;
    char* category;
    char* message;
    char* filename;
    DiagnosticSpan span;
    int start_byte;
    int end_byte;
    char* hint;
} DiagnosticJsonRecord;

static DiagnosticJsonRecord* g_diagnostic_json_records = NULL;
static int g_diagnostic_json_count = 0;
static int g_diagnostic_json_cap = 0;
static bool g_diagnostics_json_enabled = false;

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

/**
 * @brief الحصول على الرمز التشخيصي الثابت للتحذير.
 */
static const char* warning_type_code(WarningType type)
{
    switch (type) {
        case WARN_UNUSED_VARIABLE: return "B1100";
        case WARN_DEAD_CODE: return "B1101";
        case WARN_IMPLICIT_RETURN: return "B1102";
        case WARN_SHADOW_VARIABLE: return "B1103";
        case WARN_IMPLICIT_NARROWING: return "B1104";
        case WARN_SIGNED_UNSIGNED_COMPARE: return "B1105";
        default: return "B1199";
    }
}

/**
 * @brief اختيار رمز تشخيص صالح مع قيمة احتياطية.
 */
static const char* diagnostic_code_or_default(const char* code, const char* fallback)
{
    return (code && code[0]) ? code : fallback;
}

/**
 * @brief اشتقاق فئة التشخيص من عائلة الرمز الثابت.
 */
static const char* diagnostic_category_for_code(const char* code)
{
    code = diagnostic_code_or_default(code, DIAG_CODE_SYNTAX_GENERIC);
    if (strncmp(code, "B11", 3) == 0) return "warning";

    switch (code[1]) {
        case '0': return "syntax";
        case '1': return "semantic";
        case '2': return "include";
        case '3': return "ir";
        case '4': return "backend";
        case '5': return "runtime";
        case '9': return "internal";
        default: return "internal";
    }
}

static DiagnosticSpan diagnostic_span_normalize(DiagnosticSpan span);
static int diagnostic_byte_offset(const char* filename, int line, int col);

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

static char* diagnostic_strdup(const char* text)
{
    if (!text) text = "";
    char* out = strdup(text);
    return out;
}

static char* diagnostic_vformat(const char* format, va_list args)
{
    if (!format) return diagnostic_strdup("");

    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) return diagnostic_strdup(format);

    char* out = (char*)malloc((size_t)needed + 1u);
    if (!out) return diagnostic_strdup(format);

    va_copy(copy, args);
    (void)vsnprintf(out, (size_t)needed + 1u, format, copy);
    va_end(copy);
    return out;
}

static void diagnostic_json_record_free(DiagnosticJsonRecord* record)
{
    if (!record) return;
    free(record->code);
    free(record->severity);
    free(record->category);
    free(record->message);
    free(record->filename);
    free(record->hint);
    memset(record, 0, sizeof(*record));
}

void diagnostics_json_reset(void)
{
    for (int i = 0; i < g_diagnostic_json_count; ++i) {
        diagnostic_json_record_free(&g_diagnostic_json_records[i]);
    }
    free(g_diagnostic_json_records);
    g_diagnostic_json_records = NULL;
    g_diagnostic_json_count = 0;
    g_diagnostic_json_cap = 0;
}

void diagnostics_set_json_enabled(bool enabled)
{
    g_diagnostics_json_enabled = enabled;
}

bool diagnostics_json_enabled(void)
{
    return g_diagnostics_json_enabled;
}

static bool diagnostics_json_reserve(void)
{
    if (g_diagnostic_json_count < g_diagnostic_json_cap) return true;

    int new_cap = (g_diagnostic_json_cap == 0) ? 8 : g_diagnostic_json_cap * 2;
    DiagnosticJsonRecord* grown = (DiagnosticJsonRecord*)realloc(
        g_diagnostic_json_records,
        (size_t)new_cap * sizeof(*grown));
    if (!grown) return false;

    g_diagnostic_json_records = grown;
    g_diagnostic_json_cap = new_cap;
    return true;
}

static void diagnostics_json_add(const char* code,
                                 const char* severity,
                                 const char* category,
                                 DiagnosticSpan span,
                                 const char* hint,
                                 const char* message)
{
    if (!g_diagnostics_json_enabled) return;
    if (!diagnostics_json_reserve()) return;

    DiagnosticJsonRecord* record = &g_diagnostic_json_records[g_diagnostic_json_count++];
    memset(record, 0, sizeof(*record));
    record->code = diagnostic_strdup(code);
    record->severity = diagnostic_strdup(severity);
    record->category = diagnostic_strdup(category);
    record->message = diagnostic_strdup(message);
    record->filename = diagnostic_strdup(span.filename ? span.filename : "unknown");
    record->span = diagnostic_span_normalize(span);
    record->start_byte = diagnostic_byte_offset(record->filename, record->span.line, record->span.col);
    record->end_byte = diagnostic_byte_offset(record->filename, record->span.end_line, record->span.end_col);
    record->hint = hint && hint[0] ? diagnostic_strdup(hint) : NULL;
}

static int diagnostic_byte_offset(const char* filename, int line, int col)
{
    const char* src = error_sources_lookup(filename);
    if (!src) return 0;

    int current_line = 1;
    int offset = 0;
    while (src[offset] && current_line < line) {
        if (src[offset] == '\n') current_line++;
        offset++;
    }

    int remaining_col = col > 0 ? col - 1 : 0;
    while (src[offset] && src[offset] != '\n' && remaining_col > 0) {
        offset++;
        remaining_col--;
    }
    return offset;
}

static void diagnostics_json_escape(FILE* out, const char* text)
{
    if (!out) return;
    if (!text) return;

    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        switch (*p) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\b': fputs("\\b", out); break;
            case '\f': fputs("\\f", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (*p < 0x20u) {
                    fprintf(out, "\\u%04x", (unsigned)*p);
                } else {
                    fputc((int)*p, out);
                }
                break;
        }
    }
}

void diagnostics_json_write(FILE* out,
                            const char* compiler_version,
                            const char* mode,
                            const char* target,
                            const char* working_directory)
{
    if (!out) out = stdout;

    int errors = 0;
    int warnings = 0;
    int notes = 0;
    for (int i = 0; i < g_diagnostic_json_count; ++i) {
        const char* severity = g_diagnostic_json_records[i].severity;
        if (severity && strcmp(severity, "warning") == 0)
            warnings++;
        else if (severity && (strcmp(severity, "note") == 0 || strcmp(severity, "help") == 0))
            notes++;
        else
            errors++;
    }

    fputs("{\n", out);
    fputs("  \"schema_version\": \"diagnostics-json-v1\",\n", out);
    fputs("  \"compiler\": {\"name\": \"baa\", \"version\": \"", out);
    diagnostics_json_escape(out, compiler_version ? compiler_version : "");
    fputs("\"},\n", out);
    fputs("  \"invocation\": {\"mode\": \"", out);
    diagnostics_json_escape(out, mode ? mode : "");
    fputs("\", \"target\": \"", out);
    diagnostics_json_escape(out, target ? target : "");
    fputs("\", \"working_directory\": \"", out);
    diagnostics_json_escape(out, working_directory ? working_directory : ".");
    fputs("\"},\n", out);
    fprintf(out,
            "  \"summary\": {\"errors\": %d, \"warnings\": %d, \"notes\": %d},\n",
            errors,
            warnings,
            notes);
    fputs("  \"diagnostics\": [\n", out);

    for (int i = 0; i < g_diagnostic_json_count; ++i) {
        DiagnosticJsonRecord* record = &g_diagnostic_json_records[i];
        DiagnosticSpan span = diagnostic_span_normalize(record->span);
        fputs("    {\n", out);
        fputs("      \"code\": \"", out);
        diagnostics_json_escape(out, record->code);
        fputs("\",\n      \"severity\": \"", out);
        diagnostics_json_escape(out, record->severity);
        fputs("\",\n      \"category\": \"", out);
        diagnostics_json_escape(out, record->category);
        fputs("\",\n      \"message\": \"", out);
        diagnostics_json_escape(out, record->message);
        fputs("\",\n      \"file\": \"", out);
        diagnostics_json_escape(out, record->filename);
        fprintf(out, "\",\n      \"line\": %d,\n      \"column\": %d,\n", span.line, span.col);
        fputs("      \"span\": {", out);
        fprintf(out,
                "\"start\": {\"line\": %d, \"column\": %d, \"byte\": %d}, ",
                span.line,
                span.col,
                record->start_byte);
        fprintf(out,
                "\"end\": {\"line\": %d, \"column\": %d, \"byte\": %d}",
                span.end_line,
                span.end_col,
                record->end_byte);
        fputs("},\n      \"hint\": ", out);
        if (record->hint) {
            fputs("\"", out);
            diagnostics_json_escape(out, record->hint);
            fputs("\"", out);
        } else {
            fputs("null", out);
        }
        fputs(",\n      \"hints\": [", out);
        if (record->hint) {
            fputs("\"", out);
            diagnostics_json_escape(out, record->hint);
            fputs("\"", out);
        }
        fputs("]\n    }", out);
        if (i + 1 < g_diagnostic_json_count) fputs(",", out);
        fputs("\n", out);
    }

    fputs("  ]\n}\n", out);
    fflush(out);
}

static DiagnosticSpan diagnostic_span_normalize(DiagnosticSpan span)
{
    if (span.line <= 0) span.line = 1;
    if (span.col <= 0) span.col = 1;
    if (span.end_line <= 0) span.end_line = span.line;
    if (span.end_line < span.line) span.end_line = span.line;
    if (span.end_line == span.line && span.end_col <= span.col) {
        span.end_col = span.col + DIAGNOSTIC_MIN_CARET_WIDTH;
    } else if (span.end_line > span.line && span.end_col <= 0) {
        span.end_col = DIAGNOSTIC_MIN_CARET_WIDTH + 1;
    }
    return span;
}

static DiagnosticSpan diagnostic_span_from_loc(const char* filename, int line, int col)
{
    DiagnosticSpan span;
    span.filename = filename;
    span.line = line;
    span.col = col;
    span.end_line = line;
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

    // 1. البحث عن بداية السطر الأول.
    const char* start = src;
    int current_line = 1;
    while (current_line < span.line && *start != '\0') {
        if (*start == '\n') current_line++;
        start++;
    }

    while (current_line <= span.end_line && *start != '\0') {
        // 2. البحث عن نهاية السطر الحالي.
        const char* end = start;
        while (*end != '\n' && *end != '\0') {
            end++;
        }

        const char* display_end = end;
        if (display_end > start && *(display_end - 1) == '\r') {
            display_end--;
        }

        // 3. طباعة رقم السطر والسطر نفسه.
        if (use_color) {
            fprintf(stderr, "\n    %s%4d |%s %.*s\n",
                    ANSI_CYAN, current_line, ANSI_RESET, (int)(display_end - start), start);
        } else {
            fprintf(stderr, "\n    %4d | %.*s\n", current_line, (int)(display_end - start), start);
        }

        int line_bytes = (int)(display_end - start);
        int start_byte = (current_line == span.line) ? span.col - 1 : 0;
        int end_byte = (current_line == span.end_line) ? span.end_col - 1 : line_bytes;
        if (start_byte < 0) start_byte = 0;
        if (start_byte > line_bytes) start_byte = line_bytes;
        if (end_byte <= start_byte) end_byte = start_byte + DIAGNOSTIC_MIN_CARET_WIDTH;
        if (end_byte > line_bytes && start_byte < line_bytes) end_byte = line_bytes;

        int pointer_spaces = diagnostic_display_width_bytes(start, start_byte);
        int caret_width = diagnostic_display_width_bytes(start + start_byte, end_byte - start_byte);
        if (caret_width < DIAGNOSTIC_MIN_CARET_WIDTH) caret_width = DIAGNOSTIC_MIN_CARET_WIDTH;

        // 4. طباعة المؤشر (^) بعرض النطاق على كل سطر مشمول.
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

        if (*end == '\n') {
            start = end + 1;
        } else {
            start = end;
        }
        current_line++;
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

static void error_report_vspan(const char* code,
                               DiagnosticSpan span,
                               const char* hint,
                               const char* message,
                               va_list args)
{
    span = diagnostic_span_normalize(span);
    code = diagnostic_code_or_default(code, DIAG_CODE_SYNTAX_GENERIC);
    const char* category = diagnostic_category_for_code(code);
    had_error = true;
    bool use_color = g_warning_config.colored_output;
    char* formatted = diagnostic_vformat(message, args);
    diagnostics_json_add(code, "error", category, span, hint, formatted ? formatted : "");
    if (g_diagnostics_json_enabled) {
        free(formatted);
        return;
    }
    
    // طباعة رأس الخطأ
    if (use_color) {
        fprintf(stderr, "%s[Error]%s %s[%s]%s %s[%s]%s %s:%d:%d: ",
                ANSI_BOLD_RED, ANSI_RESET,
                ANSI_CYAN, code, ANSI_RESET,
                ANSI_CYAN, category, ANSI_RESET,
                span.filename ? span.filename : "unknown",
                span.line,
                span.col);
    } else {
        fprintf(stderr, "[Error] [%s] [%s] %s:%d:%d: ",
                code,
                category,
                span.filename ? span.filename : "unknown",
                span.line,
                span.col);
    }

    // طباعة الرسالة المنسقة
    fputs(formatted ? formatted : "", stderr);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    print_source_line(span, use_color, ANSI_BOLD_RED);
    fprintf(stderr, "\n");
    print_hint_line(hint, use_color);
    free(formatted);
}

void error_report_loc(const char* filename, int line, int col, const char* message, ...) {
    va_list args;
    va_start(args, message);
    error_report_vspan(DIAG_CODE_SYNTAX_GENERIC,
                       diagnostic_span_from_loc(filename, line, col),
                       NULL,
                       message,
                       args);
    va_end(args);
}

void error_report_loc_code(const char* code,
                           const char* filename,
                           int line,
                           int col,
                           const char* message,
                           ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(code, diagnostic_span_from_loc(filename, line, col), NULL, message, args);
    va_end(args);
}

void error_report_span(DiagnosticSpan span, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(DIAG_CODE_SYNTAX_GENERIC, span, NULL, message, args);
    va_end(args);
}

void error_report_span_code(const char* code, DiagnosticSpan span, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(code, span, NULL, message, args);
    va_end(args);
}

void error_report_span_hint(DiagnosticSpan span, const char* hint, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(DIAG_CODE_SYNTAX_GENERIC, span, hint, message, args);
    va_end(args);
}

void error_report_span_hint_code(const char* code,
                                 DiagnosticSpan span,
                                 const char* hint,
                                 const char* message,
                                 ...)
{
    va_list args;
    va_start(args, message);
    error_report_vspan(code, span, hint, message, args);
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
    const char* warn_code = warning_type_code(type);
    const char* warn_category = diagnostic_category_for_code(warn_code);
    const char* severity = g_warning_config.warnings_as_errors ? "error" : "warning";
    char* formatted = diagnostic_vformat(message, args);
    diagnostics_json_add(warn_code, severity, warn_category, span, NULL, formatted ? formatted : "");
    if (g_diagnostics_json_enabled) {
        free(formatted);
        return;
    }
    
    if (use_color) {
        if (g_warning_config.warnings_as_errors) {
            fprintf(stderr, "%s[Error]%s %s[%s]%s %s[%s]%s %s:%d:%d: ",
                    ANSI_BOLD_RED, ANSI_RESET,
                    ANSI_CYAN, warn_code, ANSI_RESET,
                    ANSI_CYAN, warn_category, ANSI_RESET,
                    span.filename ? span.filename : "unknown", span.line, span.col);
        } else {
            fprintf(stderr, "%s[Warning]%s %s[%s]%s %s[%s]%s %s:%d:%d: ",
                    ANSI_BOLD_YELLOW, ANSI_RESET,
                    ANSI_CYAN, warn_code, ANSI_RESET,
                    ANSI_CYAN, warn_category, ANSI_RESET,
                    span.filename ? span.filename : "unknown", span.line, span.col);
        }
        fprintf(stderr, "%s[-W%s]%s ", ANSI_CYAN, warn_name, ANSI_RESET);
    } else {
        if (g_warning_config.warnings_as_errors) {
            fprintf(stderr, "[Error] [%s] [%s] %s:%d:%d: ",
                    warn_code, warn_category,
                    span.filename ? span.filename : "unknown", span.line, span.col);
        } else {
            fprintf(stderr, "[Warning] [%s] [%s] %s:%d:%d: ",
                    warn_code, warn_category,
                    span.filename ? span.filename : "unknown", span.line, span.col);
        }
        fprintf(stderr, "[-W%s] ", warn_name);
    }

    // طباعة الرسالة المنسقة
    fputs(formatted ? formatted : "", stderr);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    const char* ptr_color = g_warning_config.warnings_as_errors ? ANSI_BOLD_RED : ANSI_BOLD_YELLOW;
    print_source_line(span, use_color, ptr_color);
    fprintf(stderr, "\n");
    free(formatted);
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
