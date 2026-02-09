/**
 * @file ir_mem2reg.h
 * @brief تمريرة ترقية الذاكرة إلى سجلات (Mem2Reg) — خط أساس (v0.3.2.5.1).
 * @version 0.3.2.5.1
 *
 * هدف هذه التمريرة:
 * - ترقية المتغيرات المحلية الممثلة عبر:
 *   - `حجز` (IR_OP_ALLOCA)
 *   - `حمل` (IR_OP_LOAD)
 *   - `خزن` (IR_OP_STORE)
 *   إلى استعمال سجلات SSA مباشرة.
 *
 * هذا الإصدار هو "خط أساس للسلامة" (correctness-first):
 * - نُرقّي فقط `alloca` الذي لا يهرب مؤشّره خارج نفس الكتلة الأساسية:
 *   - كل استعمالات سجل المؤشر يجب أن تكون `load/store` داخل نفس `IRBlock`.
 *   - لا يسمح بتمرير المؤشر إلى `نداء` (call) ولا استخدامه داخل `فاي` (phi)
 *     ولا تخزين المؤشر نفسه كقيمة (pointer escape).
 * - كل `load` يجب أن يأتي بعد `store` سابق داخل نفس الكتلة (منع قراءة غير مهيّأة).
 */

#ifndef BAA_IR_MEM2REG_H
#define BAA_IR_MEM2REG_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تشغيل تمريرة Mem2Reg على وحدة IR كاملة.
 *
 * @param module وحدة IR الهدف.
 * @return true إذا تم تعديل الـ IR، وإلا false.
 */
bool ir_mem2reg_run(IRModule* module);

/**
 * @brief واصف تمريرة Mem2Reg لاستخدامه ضمن خط أنابيب المُحسِّن.
 */
extern IRPass IR_PASS_MEM2REG;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_MEM2REG_H