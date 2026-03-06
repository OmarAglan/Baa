/**
 * @file ir_unroll.h
 * @brief فك الحلقات (Loop Unrolling) — تنفيذ محافظ مع عدّ ثابت.
 * @version 0.3.2.7.1
 *
 * هذه التمريرة تطبق فك الحلقات بشكل محافظ جداً:
 * - فقط إذا كان عدد الدورات (trip count) ثابتاً وصغيراً.
 * - فقط على حلقات طبيعية ذات preheader وحيد.
 * - تعمل بعد الخروج من SSA (Out-of-SSA) لأن ذلك يجعل القيم الحلقية صريحة عبر نسخ.
 */

#ifndef BAA_IR_UNROLL_H
#define BAA_IR_UNROLL_H

#include <stdbool.h>

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تشغيل فك الحلقات على الوحدة.
 *
 * @param module وحدة IR.
 * @param max_trip الحد الأقصى لعدد الدورات التي نقبل فكها بالكامل (لمنع انفجار الحجم).
 * @return true إذا حدث تعديل، false خلاف ذلك.
 */
bool ir_unroll_run(IRModule* module, int max_trip);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_UNROLL_H
