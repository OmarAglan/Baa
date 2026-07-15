/**
 * @file emit_nazm.h
 * @brief مُصدّر مصدر نظم العربي من تمثيل الآلة بعد تخصيص السجلات.
 */

#ifndef BAA_EMIT_NAZM_H
#define BAA_EMIT_NAZM_H

#include <stdio.h>

#include "isel.h"
#include "target.h"

typedef enum
{
    BAA_NAZM_EMIT_OK = 0,
    BAA_NAZM_EMIT_UNSUPPORTED,
    BAA_NAZM_EMIT_IO_ERROR,
} BaaNazmEmitStatus;

typedef struct
{
    BaaNazmEmitStatus status;
    MachineOp op;
    const char *reason;
    const char *source_file;
    int source_line;
    int source_col;
} BaaNazmEmitResult;

/**
 * @brief إصدار مصدر نظم عربي خالص لمسار الظل غير الافتراضي.
 *
 * تفحص الدالة الوحدة كاملة قبل أن تكتب أي بايت. لا يوجد رجوع ضمني إلى GAS:
 * كل صيغة غير مدعومة تعيد BAA_NAZM_EMIT_UNSUPPORTED مع موضع باء إن توفر.
 */
BaaNazmEmitResult emit_nazm_module(const MachineModule *module,
                                   FILE *out,
                                   const BaaTarget *target);

/**
 * @brief إصدار مصدر نظم مع خريطة `baa-nazm-source-map-v1` اختيارية.
 *
 * تربط الخريطة أسطر نظم المولدة بملف/سطر/عمود باء الأصلي، وتحفظ المسارات
 * كبايتات UTF-8 سداسية لتبقى قابلة للقراءة بلا غموض escaping على كل مضيف.
 */
BaaNazmEmitResult emit_nazm_module_with_source_map(const MachineModule *module,
                                                   FILE *out,
                                                   FILE *source_map,
                                                   const char *generated_path,
                                                   const BaaTarget *target);

#endif
