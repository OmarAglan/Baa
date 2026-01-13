/**
 * @file error.c
 * @brief نظام التشخيص (Diagnostic Engine) - الأخطاء والتحذيرات.
 * @version 0.2.8
 */

#include "baa.h"
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

// ============================================================================
// حالة النظام (System State)
// ============================================================================

static const char* current_source = NULL;
static bool had_error = false;
static bool had_warning = false;
static int warning_count = 0;

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
        default:                   return "unknown";
    }
}

// ============================================================================
// تهيئة النظام (Initialization)
// ============================================================================

void error_init(const char* source) {
    current_source = source;
    had_error = false;
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
 * @brief طباعة سطر الكود مع مؤشر.
 */
static void print_source_line(int line, int col, bool use_color, const char* pointer_color) {
    if (!current_source) return;

    // 1. البحث عن بداية السطر
    const char* start = current_source;
    int current_line = 1;
    while (current_line < line && *start != '\0') {
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
                ANSI_CYAN, line, ANSI_RESET, (int)(end - start), start);
    } else {
        fprintf(stderr, "\n    %4d | %.*s\n", line, (int)(end - start), start);
    }

    // 4. طباعة المؤشر (^)
    fprintf(stderr, "         ");
    for (int i = 1; i < col; i++) {
        fprintf(stderr, " ");
    }
    
    if (use_color) {
        fprintf(stderr, "%s^--%s ", pointer_color, ANSI_RESET);
    } else {
        fprintf(stderr, "^-- ");
    }
}

// ============================================================================
// الإبلاغ عن الأخطاء (Error Reporting)
// ============================================================================

void error_report(Token token, const char* message, ...) {
    had_error = true;
    bool use_color = g_warning_config.colored_output;
    
    // طباعة رأس الخطأ
    if (use_color) {
        fprintf(stderr, "%s[Error]%s %s:%d:%d: ",
                ANSI_BOLD_RED, ANSI_RESET,
                token.filename ? token.filename : "unknown",
                token.line,
                token.col);
    } else {
        fprintf(stderr, "[Error] %s:%d:%d: ",
                token.filename ? token.filename : "unknown",
                token.line,
                token.col);
    }

    // طباعة الرسالة المنسقة
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    print_source_line(token.line, token.col, use_color, ANSI_BOLD_RED);
    fprintf(stderr, "\n");
}

// ============================================================================
// الإبلاغ عن التحذيرات (Warning Reporting)
// ============================================================================

void warning_report(WarningType type, const char* filename, int line, int col, const char* message, ...) {
    // التحقق من تفعيل التحذير
    if (!g_warning_config.all_warnings && !g_warning_config.enabled[type]) {
        return;
    }
    
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
                    filename ? filename : "unknown", line, col);
        } else {
            fprintf(stderr, "%s[Warning]%s %s:%d:%d: ",
                    ANSI_BOLD_YELLOW, ANSI_RESET,
                    filename ? filename : "unknown", line, col);
        }
        fprintf(stderr, "%s[-W%s]%s ", ANSI_CYAN, warn_name, ANSI_RESET);
    } else {
        if (g_warning_config.warnings_as_errors) {
            fprintf(stderr, "[Error] %s:%d:%d: ", filename ? filename : "unknown", line, col);
        } else {
            fprintf(stderr, "[Warning] %s:%d:%d: ", filename ? filename : "unknown", line, col);
        }
        fprintf(stderr, "[-W%s] ", warn_name);
    }

    // طباعة الرسالة المنسقة
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    const char* ptr_color = g_warning_config.warnings_as_errors ? ANSI_BOLD_RED : ANSI_BOLD_YELLOW;
    print_source_line(line, col, use_color, ptr_color);
    fprintf(stderr, "\n");
}