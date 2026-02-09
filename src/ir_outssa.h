/**
 * @file ir_outssa.h
 * @brief تمريرة الخروج من SSA — إزالة عقد فاي قبل الخلفية.
 * @version 0.3.2.5.2
 *
 * الهدف:
 * - إزالة تعليمات `فاي` (IR_OP_PHI) من الـ IR بإعادة كتابتها إلى نسخ على الحواف.
 * - تقسيم الحواف الحرجة عند الحاجة لضمان إدراج النسخ بشكل صحيح.
 *
 * سبب وجود هذه التمريرة:
 * - مرحلة اختيار التعليمات/تخصيص السجلات الحالية لا تُنفّذ فاي فعلياً.
 * - لذلك يجب ضمان عدم وصول أي `فاي` إلى الخلفية (`ISel`/`RegAlloc`/`Emit`).
 */

#ifndef BAA_IR_OUTSSA_H
#define BAA_IR_OUTSSA_H

#include <stdbool.h>

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تشغيل تمريرة الخروج من SSA على وحدة IR كاملة.
 *
 * @param module وحدة IR الهدف.
 * @return قيمة منطقية: 1 إذا تم تعديل الـ IR (إزالة فاي أو تقسيم حواف أو إدراج نسخ)، وإلا 0.
 */
bool ir_outssa_run(IRModule* module);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_OUTSSA_H
