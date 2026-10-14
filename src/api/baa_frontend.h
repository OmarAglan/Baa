/**
 * @file baa_frontend.h
 * @brief واجهة الواجهة الأمامية داخل العملية (baa-frontend-api-v1).
 *
 * تشغّل أوضاع أدوات باء التحليلية داخل عملية المستدعي بدلاً من تشغيل baa
 * كعملية منفصلة: --check مع --diagnostics=json، و--dump-symbols=json،
 * و--dump-tokens=json، و--dump-structure=json، و--semantic-query=json،
 * و--semantic-index=json، و--inlay-hints=json، و--format=json،
 * و--completion-data=json، و--version.
 *
 * الوسائط والدخل والمخرجات ورموز الخروج هي نفسها في سطر الأوامر، فيبقى
 * عقد JSON لكل وضع واحداً بين المسارين. أوضاع توليد الكود والربط والتحديث
 * مرفوضة برمز الخروج 2.
 *
 * كل نداء مستقل: تحرر الواجهة كل ما خصصه المصرّف أثناءه وتعيد ضبط حالته
 * العامة. النداءات متسلسلة بقفل داخلي، ومجلد العمل الذي يطلبه النداء يضبط
 * للعملية كلها مدة النداء ثم يعاد.
 */

#ifndef BAA_FRONTEND_H
#define BAA_FRONTEND_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAA_FRONTEND_CONTRACT "baa-frontend-api-v1"

typedef struct BaaFrontendRequest
{
    /* مسار baa التنفيذي الذي يقوم النداء مقامه؛ منه يُحل جذر التثبيت
     * (stdlib/). NULL يعني مجلد العملية الحالية. */
    const char *executable_path;
    /* مجلد العمل أثناء النداء؛ NULL يبقي مجلد العملية. */
    const char *working_directory;
    /* وسائط سطر الأوامر بعد اسم البرنامج، بترميز UTF-8. */
    const char *const *arguments;
    size_t argument_count;
    /* الدخل القياسي لوضع --source-stdin. */
    const char *standard_input;
    size_t standard_input_size;
} BaaFrontendRequest;

typedef struct BaaFrontendResult
{
    int exit_code;
    /* مخازن منتهية بصفر يملكها المستدعي ويحررها baa_frontend_result_free. */
    char *standard_output;
    size_t standard_output_size;
    char *standard_error;
    size_t standard_error_size;
} BaaFrontendResult;

typedef enum BaaFrontendStatus
{
    BAA_FRONTEND_OK = 0,
    BAA_FRONTEND_INVALID_REQUEST = 1,
    BAA_FRONTEND_IO_ERROR = 2,
    BAA_FRONTEND_OUT_OF_MEMORY = 3
} BaaFrontendStatus;

/** @brief اسم العقد ("baa-frontend-api-v1"). */
const char *baa_frontend_contract(void);

/** @brief إصدار باء المرتبط (مثل "0.6.0"). */
const char *baa_frontend_compiler_version(void);

/**
 * @brief تشغيل نداء أدوات واحد داخل العملية.
 *
 * عند BAA_FRONTEND_OK تُملأ النتيجة، ورمز الخروج فيها هو رمز baa نفسه.
 * عند غيره تبقى النتيجة فارغة ولم يشغل المصرّف.
 */
BaaFrontendStatus baa_frontend_run(const BaaFrontendRequest *request,
                                   BaaFrontendResult *result);

/** @brief تحرير مخازن النتيجة وتصفيرها. */
void baa_frontend_result_free(BaaFrontendResult *result);

#ifdef __cplusplus
}
#endif

#endif /* BAA_FRONTEND_H */
