/**
 * @file process.h
 * @brief تشغيل عمليات خارجية (gcc/برامج الاختبار) بدون الاعتماد على system().
 * @version 0.3.2.9.4
 */

#ifndef BAA_PROCESS_H
#define BAA_PROCESS_H

#include <stdbool.h>

typedef struct
{
    int exit_code;
    bool started;
} BaaProcessResult;

/**
 * @brief تشغيل عملية وانتظار انتهائها.
 * @param argv مصفوفة وسائط منتهية بـ NULL (argv[0] هو اسم/مسار التنفيذ).
 * @param cwd  مسار العمل (NULL = لا تغيير).
 */
bool baa_process_run(const char* const* argv, const char* cwd, BaaProcessResult* out_result);

/**
 * @brief تشغيل عملية مع إعادة توجيه stdout/stderr إلى ملفات.
 * @param stdout_path NULL = وراثة stdout.
 * @param stderr_path NULL = وراثة stderr.
 */
bool baa_process_run_redirect(const char* const* argv,
                              const char* cwd,
                              const char* stdout_path,
                              const char* stderr_path,
                              BaaProcessResult* out_result);

#endif // BAA_PROCESS_H
