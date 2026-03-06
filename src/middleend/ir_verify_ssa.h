/**
 * @file ir_verify_ssa.h
 * @brief التحقق من صحة SSA للـ IR قبل مرحلة الخروج من SSA.
 * @version 0.3.2.5.3
 *
 * الهدف من هذه الوحدة:
 * - التأكد أن كل سجل افتراضي (%م<n>) يُعرَّف مرة واحدة فقط.
 * - التأكد أن كل استعمال لسجل تُسيطر عليه كتلة تعريفه (Dominance).
 * - التحقق من تعليمات `فاي`:
 *   - مدخل واحد لكل سابق.
 *   - لا يوجد مدخلات مكررة.
 *   - لا توجد كتل ليست سوابق.
 *
 * تُستخدم هذه الوحدة للتشخيص والتقوية (Correctness Hardening) بعد Mem2Reg
 * وقبل تمريرة الخروج من SSA، لأن الخروج من SSA يخرق خاصية SSA عمداً.
 */

#ifndef BAA_IR_VERIFY_SSA_H
#define BAA_IR_VERIFY_SSA_H

#include <stdbool.h>
#include <stdio.h>

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief التحقق من صحة SSA لدالة واحدة.
 *
 * @param func الدالة الهدف.
 * @param out  مخرجات التشخيص (عادة stderr).
 * @return true إذا كان SSA صحيحاً؛ false إذا وُجدت مخالفات.
 */
bool ir_func_verify_ssa(IRFunc* func, FILE* out);

/**
 * @brief التحقق من صحة SSA لكل دوال الوحدة.
 *
 * @param module وحدة IR الهدف.
 * @param out    مخرجات التشخيص (عادة stderr).
 * @return true إذا كانت كل الدوال صحيحة؛ false إذا فشلت أي دالة.
 */
bool ir_module_verify_ssa(IRModule* module, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_VERIFY_SSA_H

