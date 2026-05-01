/**
 * @file diagnostics.h
 * @brief واجهة نظام التشخيص (الأخطاء والتحذيرات).
 */

#ifndef BAA_SUPPORT_DIAGNOSTICS_H
#define BAA_SUPPORT_DIAGNOSTICS_H

#include <stdbool.h>

/**
 * @enum WarningType
 * @brief أنواع التحذيرات المدعومة.
 */
typedef enum {
    WARN_UNUSED_VARIABLE,
    WARN_DEAD_CODE,
    WARN_IMPLICIT_RETURN,
    WARN_SHADOW_VARIABLE,
    WARN_IMPLICIT_NARROWING,
    WARN_SIGNED_UNSIGNED_COMPARE,
    WARN_COUNT
} WarningType;

/**
 * @struct WarningConfig
 * @brief إعدادات التحذيرات.
 */
typedef struct {
    bool enabled[WARN_COUNT];
    bool warnings_as_errors;
    bool all_warnings;
    bool colored_output;
} WarningConfig;

/**
 * @struct DiagnosticSpan
 * @brief نطاق تشخيص داخل سطر واحد. end_col حد حصري.
 */
typedef struct {
    const char* filename;
    int line;
    int col;
    int end_col;
} DiagnosticSpan;

extern WarningConfig g_warning_config;

void error_init(const char* source);
void error_register_source(const char* filename, const char* source);
void warning_init(void);
void error_report_loc(const char* filename, int line, int col, const char* message, ...);
void error_report_span(DiagnosticSpan span, const char* message, ...);
void error_report_span_hint(DiagnosticSpan span, const char* hint, const char* message, ...);
void warning_report(WarningType type, const char* filename, int line, int col, const char* message, ...);
void warning_report_span(WarningType type, DiagnosticSpan span, const char* message, ...);
bool error_has_occurred(void);
bool warning_has_occurred(void);
int warning_get_count(void);
void error_reset(void);
void warning_reset(void);

/**
 * @brief غلاف توافق لاستقبال بنية تحتوي filename/line/col مثل Token.
 */
#define error_report(token_like, ...) \
    error_report_loc((token_like).filename, (token_like).line, (token_like).col, __VA_ARGS__)

#define error_report_token(token_like, ...) \
    error_report_span((DiagnosticSpan){(token_like).filename, (token_like).line, (token_like).col, \
                      (token_like).col + ((token_like).length > 0 ? (token_like).length : 1)}, __VA_ARGS__)

#define error_report_token_hint(token_like, hint_text, ...) \
    error_report_span_hint((DiagnosticSpan){(token_like).filename, (token_like).line, (token_like).col, \
                           (token_like).col + ((token_like).length > 0 ? (token_like).length : 1)}, \
                           hint_text, __VA_ARGS__)

#endif
