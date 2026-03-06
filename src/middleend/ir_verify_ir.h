/**
 * @file ir_verify_ir.h
 * @brief التحقق من سلامة/تشكيل الـ IR (Well-Formedness Verifier) قبل الخلفية.
 * @version 0.3.2.6.5
 *
 * الهدف:
 * - التحقق من قواعد "تشكيل" الـ IR (غير SSA-specific) مثل:
 *   - عدد المعاملات لكل تعليمة
 *   - اتساق الأنواع (type consistency) داخل المعاملات والنتيجة
 *   - قواعد المنهيات (terminators) داخل الكتل
 *   - تموضع `فاي` في بداية الكتل وعدد/صحة مداخلها
 *   - صحة الاستدعاءات (نداء) عندما تكون الدالة الهدف معروفة داخل الوحدة
 *
 * ملاحظة مهمة:
 * - هذا المتحقق منفصل عن `--verify-ssa`:
 *   - `--verify-ssa` يتحقق من خاصية SSA (تعريف واحد/السيطرة/مدخلات فاي).
 *   - هذا الملف يتحقق من "سلامة IR" العامة التي يجب أن تصح دائماً.
 */

#ifndef BAA_IR_VERIFY_IR_H
#define BAA_IR_VERIFY_IR_H

#include <stdbool.h>
#include <stdio.h>

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief التحقق من سلامة الـ IR لدالة واحدة.
 *
 * @param func الدالة الهدف.
 * @param out  مخرجات التشخيص (عادة stderr).
 * @return true إذا كان IR مُشكّلاً بشكل صحيح؛ false إذا وُجدت مخالفات.
 */
bool ir_func_verify_ir(IRFunc* func, FILE* out);

/**
 * @brief التحقق من سلامة الـ IR لكل دوال الوحدة.
 *
 * @param module وحدة IR الهدف.
 * @param out    مخرجات التشخيص (عادة stderr).
 * @return true إذا كانت كل الدوال صحيحة؛ false إذا فشلت أي دالة.
 */
bool ir_module_verify_ir(IRModule* module, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_VERIFY_IR_H