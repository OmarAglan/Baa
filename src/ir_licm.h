/**
 * @file ir_licm.h
 * @brief حركة التعليمات غير المتغيرة داخل الحلقة (LICM) — Loop Invariant Code Motion.
 *
 * الهدف:
 * - اكتشاف تعليمات "نقية" (بدون آثار جانبية وبدون مصائد) داخل الحلقة
 *   والتي تعتمد فقط على قيم ثابتة/خارج الحلقة، ثم نقلها إلى ما قبل رأس الحلقة.
 *
 * قيود السلامة (v0.3.2.7.1):
 * - لا نقوم بنقل عمليات الذاكرة أو الاستدعاءات.
 * - لا ننقل القسمة/الباقي لتجنب تغيير سلوك المصائد عند عدم دخول الحلقة.
 * - نحتاج preheader وحيد لرأس الحلقة (وإلا نتجاوز تلك الحلقة).
 */

#ifndef BAA_IR_LICM_H
#define BAA_IR_LICM_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief تشغيل تمريرة LICM على وحدة IR.
 * @return true إذا تم تعديل الـ IR؛ false خلاف ذلك.
 */
bool ir_licm_run(IRModule* module);

/**
 * @brief واصف التمريرة لاستخدامه ضمن خط أنابيب المُحسِّن.
 */
extern IRPass IR_PASS_LICM;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_LICM_H
