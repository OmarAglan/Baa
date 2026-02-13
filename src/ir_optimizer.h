/**
 * @file ir_optimizer.h
 * @brief خط أنابيب تحسين IR (v0.3.2.5.2).
 *
 * يوفر واجهة موحدة لتشغيل تمريرات تحسين IR بالترتيب الصحيح
 * مع تكرار حتى نقطة التثبيت (fixpoint) دون تغييرات إضافية.
 *
 * مستويات التحسين:
 * - O0: بدون تحسين (للتصحيح)
 * - O1: تحسينات أساسية (mem2reg, constfold, copyprop, dce)
 * - O2: تحسينات كاملة (+ CSE, تكرار حتى نقطة التثبيت)
 */

#ifndef BAA_IR_OPTIMIZER_H
#define BAA_IR_OPTIMIZER_H

#include <stdbool.h>
#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief مستويات التحسين في المترجم.
 */
typedef enum {
    OPT_LEVEL_0 = 0,  /**< بدون تحسين (وضع التصحيح) */
    OPT_LEVEL_1 = 1,  /**< تحسينات أساسية (الافتراضي) */
    OPT_LEVEL_2 = 2   /**< تحسينات كاملة */
} OptLevel;

/**
 * @brief تشغيل خط أنابيب التحسين على وحدة IR.
 *
 * ترتيب التمريرات (O1+):
 * 0. Mem2Reg (ترقية الذاكرة إلى سجلات) — SSA كامل
 * 1. Constant Folding (طي_الثوابت)
 * 2. Copy Propagation (نشر_النسخ)
 * 3. CSE (حذف_المكرر) — فقط في O2
 * 4. Dead Code Elimination (حذف_الميت)
 *
 * يكرر الخط حتى لا تُحدث أي تمريرة تغييرات (نقطة التثبيت)
 * أو الوصول لحد أقصى من التكرار.
 *
 * @param module وحدة IR المراد تحسينها.
 * @param level  مستوى التحسين (OPT_LEVEL_0, OPT_LEVEL_1, OPT_LEVEL_2).
 * @return true عند النجاح؛ false عند فشل بوابة التحقق (إن كانت مفعّلة) أو خطأ داخلي.
 */
bool ir_optimizer_run(IRModule* module, OptLevel level);

/**
 * @brief تفعيل/تعطيل بوابة التحقق داخل المُحسِّن (Debug Gate).
 *
 * عند التفعيل، يقوم المُحسِّن بتشغيل:
 * - `ir_module_verify_ir()` (سلامة/تشكيل IR)
 * - `ir_module_verify_ssa()` (صحة SSA بعد Mem2Reg)
 *
 * بعد كل دورة تمريرات (iteration). هذا مفيد لاكتشاف أخطاء تمريرات التحسين مبكراً.
 *
 * @param enabled 1 للتفعيل، 0 للتعطيل.
 */
void ir_optimizer_set_verify_gate(int enabled);

/**
 * @brief الحصول على اسم مستوى التحسين.
 * @param level مستوى التحسين.
 * @return تمثيل نصي (مثل "O0", "O1", "O2").
 */
const char* ir_optimizer_level_name(OptLevel level);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_OPTIMIZER_H
