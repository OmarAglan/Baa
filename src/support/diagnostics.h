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

extern WarningConfig g_warning_config;

void error_init(const char* source);
void error_register_source(const char* filename, const char* source);
void warning_init(void);
void error_report_loc(const char* filename, int line, int col, const char* message, ...);
void warning_report(WarningType type, const char* filename, int line, int col, const char* message, ...);
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

#endif
