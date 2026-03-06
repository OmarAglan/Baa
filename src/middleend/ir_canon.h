/**
 * @file ir_canon.h
 * @brief تمريرة توحيد/تطبيع الـ IR (Canonicalization Pass) (v0.3.2.6.5).
 *
 * الهدف:
 * - جعل الـ IR أكثر "قابلية للتطابق" حتى تصبح تمريرات مثل CSE/DCE/ConstFold أكثر فعالية:
 *   - تطبيع معاملات العمليات التبادلية (commutative) مثل جمع/ضرب/و/أو
 *   - وضع الثوابت في جهة ثابتة (مثلاً: الثابت على اليمين) لتقليل الاختلافات
 *   - تطبيع المقارنات (`قارن`) بحيث نقلل صيغ متعددة لنفس المعنى
 *
 * ملاحظة:
 * - هذه التمريرة لا تغيّر الدلالة؛ فقط تعيد ترتيب/توحيد شكل التعليمات.
 */

#ifndef BAA_IR_CANON_H
#define BAA_IR_CANON_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تشغيل تمريرة التطبيع على وحدة IR.
 * @return true إذا تم تعديل الـ IR؛ false خلاف ذلك.
 */
bool ir_canon_run(IRModule* module);

/**
 * @brief واصف تمريرة التطبيع لاستخدامه ضمن خط أنابيب المُحسِّن.
 */
extern IRPass IR_PASS_CANON;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_CANON_H