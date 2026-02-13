/**
 * @file ir_cfg_simplify.h
 * @brief تمريرة تبسيط مخطط التدفق (CFG Simplification) للـ IR (v0.3.2.6.5).
 *
 * الهدف:
 * - دمج الكتل التافهة (trivial blocks) لتقليل حجم الـ CFG:
 *   - كتلة تحتوي فقط على `قفز` إلى كتلة أخرى
 *   - (مع الحذر من تأثيرات `فاي`)
 *
 * - إزالة الأفرع الزائدة (redundant branches):
 *   - `قفز_شرط cond, X, X` -> `قفز X`
 *
 * - توفير أداة عامة لتقسيم الحواف الحرجة (critical edge splitting)
 *   لاستخدامها في التمريرات الأخرى.
 *
 * ملاحظة:
 * - التمريرة تعتمد على `ir_func_rebuild_preds()` لضمان أن pred/succ دقيقة.
 */

#ifndef BAA_IR_CFG_SIMPLIFY_H
#define BAA_IR_CFG_SIMPLIFY_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تقسيم حافة حرجة (Critical Edge Splitting) داخل CFG.
 *
 * تعريف الحافة الحرجة:
 * - pred لديه أكثر من تابع (succ_count > 1)
 * - succ لديه أكثر من سابق (pred_count > 1)
 *
 * إن كانت الحافة غير حرجة، تعيد الدالة `succ` دون تعديل.
 *
 * @param func الدالة الهدف.
 * @param pred كتلة السابق.
 * @param succ كتلة التابع.
 * @return كتلة وسيطة جديدة عند التقسيم، أو `succ` إن لم تكن حرجة، أو NULL عند الفشل.
 */
IRBlock* ir_cfg_split_critical_edge(IRFunc* func, IRBlock* pred, IRBlock* succ);

/**
 * @brief تشغيل تبسيط CFG على وحدة IR.
 * @return true إذا تم تعديل الـ IR؛ false خلاف ذلك.
 */
bool ir_cfg_simplify_run(IRModule* module);

/**
 * @brief واصف تمريرة تبسيط CFG لاستخدامه ضمن خط أنابيب المُحسِّن.
 */
extern IRPass IR_PASS_CFG_SIMPLIFY;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_CFG_SIMPLIFY_H
