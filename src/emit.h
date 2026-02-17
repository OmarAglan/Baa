/**
 * @file emit.h
 * @brief إصدار كود التجميع - تحويل تمثيل الآلة إلى نص تجميع AT&T (v0.3.2.3)
 *
 * يأخذ وحدة آلية (MachineModule) بعد اختيار التعليمات وتخصيص السجلات
 * ويولد ملف تجميع x86-64 بصيغة AT&T متوافق مع GAS.
 *
 * ملاحظة (v0.3.2.8+): الخلفية أصبحت متعددة الأهداف.
 * - COFF/Windows يستخدم أقسام مثل `.rdata`
 * - ELF/Linux يستخدم أقسام مثل `.rodata`
 *
 * يتم تمرير الهدف عبر `emit_module_ex()`.
 *
 * المراحل:
 *   IR → اختيار التعليمات (isel) → تخصيص السجلات (regalloc)
 *   → إصدار كود التجميع (emit) → ملف .s
 */

#ifndef BAA_EMIT_H
#define BAA_EMIT_H

#include <stdio.h>
#include <stdbool.h>
#include "isel.h"
#include "regalloc.h"

typedef struct BaaTarget BaaTarget;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// واجهة إصدار كود التجميع (Code Emission API)
// ============================================================================

/**
 * @brief إصدار كود تجميع x86-64 (AT&T) لوحدة آلية كاملة.
 *
 * يكتب إلى ملف الخرج:
 * 1. قسم البيانات الثابتة (.rdata) - صيغ الطباعة والقراءة
 * 2. قسم البيانات (.data) - المتغيرات العامة
 * 3. قسم النصوص (.text) - الدوال مع:
 *    - مقدمة الدالة (prologue): حفظ إطار المكدس والسجلات المحفوظة
 *    - تعليمات الجسم: ترجمة كل MachineInst إلى AT&T
 *    - خاتمة الدالة (epilogue): استعادة السجلات والرجوع
 * 4. جدول النصوص (.rdata) - السلاسل النصية الثابتة
 *
 * @param module الوحدة الآلية (بعد تخصيص السجلات).
 * @param out ملف الخرج للكتابة.
 * @return صحيح عند النجاح، خطأ عند الفشل.
 */
bool emit_module(MachineModule* module, FILE* out, bool debug_info);

/**
 * @brief نسخة موسعة من emit_module مع دعم هدف.
 * @param module الوحدة الآلية.
 * @param out ملف الخرج.
 * @param debug_info تفعيل معلومات الديبغ.
 * @param target الهدف (يحدد الأقسام/ABI).
 */
bool emit_module_ex(MachineModule* module, FILE* out, bool debug_info, const BaaTarget* target);

/**
 * @brief إصدار كود تجميع لدالة واحدة.
 *
 * @param func الدالة الآلية.
 * @param out ملف الخرج.
 * @return صحيح عند النجاح.
 */
bool emit_func(MachineFunc* func, FILE* out);

/**
 * @brief إصدار تعليمة آلة واحدة بصيغة AT&T.
 *
 * @param inst التعليمة الآلية.
 * @param func الدالة الحاوية (للسياق).
 * @param out ملف الخرج.
 */
void emit_inst(MachineInst* inst, MachineFunc* func, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // BAA_EMIT_H
