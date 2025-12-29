/**
 * @file error.c
 * @brief نظام تشخيص الأخطاء (Diagnostic Engine).
 * @version 0.2.2
 */

#include "baa.h"
#include <stdarg.h>

// حالة النظام
static const char* current_source = NULL;
static bool had_error = false;

void error_init(const char* source) {
    current_source = source;
    had_error = false;
}

bool error_has_occurred() {
    return had_error;
}

void error_reset() {
    had_error = false;
}

/**
 * @brief طباعة سطر الكود الذي حدث فيه الخطأ مع مؤشر.
 */
static void print_source_line(int line, int col) {
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

    // 3. طباعة السطر
    fprintf(stderr, "\n    %.*s\n", (int)(end - start), start);

    // 4. طباعة المؤشر (^)
    // نحتاج لحساب المسافة. المسافة هي 'col - 1' مسافة.
    // ملاحظة: هذا تقريب بسيط. التعامل الدقيق مع UTF-8 وعرض الخطوط يتطلب مكتبات أعقد.
    fprintf(stderr, "    ");
    for (int i = 1; i < col; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^-- ");
}

void error_report(Token token, const char* message, ...) {
    had_error = true;
    
    // طباعة رأس الخطأ
    fprintf(stderr, "[Error] %s:%d:%d: ", 
            token.filename ? token.filename : "unknown", 
            token.line, 
            token.col);

    // طباعة الرسالة المنسقة
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");

    // طباعة سياق الكود
    print_source_line(token.line, token.col);
    fprintf(stderr, "\n");
}