/**
 * @file ir_inline.h
 * @brief تضمين الدوال (Inlining) — توسيع نداء الدالة داخل موقع النداء.
 * @version 0.3.2.7.2
 *
 * هذه التمريرة تعمل بشكل محافظ:
 * - نُضمّن دوال صغيرة ذات موقع نداء واحد فقط.
 * - تُطبق قبل Mem2Reg (أي قبل بناء SSA) لتفادي تعقيد فاي.
 * - تعتمد على Mem2Reg + تمريرات التحسين اللاحقة كـ "تنظيف بعد التضمين".
 */

#ifndef BAA_IR_INLINE_H
#define BAA_IR_INLINE_H

#include <stdbool.h>

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تشغيل تمريرة التضمين على الوحدة.
 * @return true إذا تم تعديل الـ IR؛ false خلاف ذلك.
 */
bool ir_inline_run(IRModule* module);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_INLINE_H
