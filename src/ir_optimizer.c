/**
 * @file ir_optimizer.c
 * @brief تنفيذ خط أنابيب تحسين IR (v0.3.1.6).
 *
 * يطبق خط التحسين الموحد مع:
 * - ترتيب التمريرات
 * - تكرار حتى نقطة التثبيت (Fixpoint)
 * - التحكم بمستوى التحسين (-O0/-O1/-O2)
 */

#include "ir_optimizer.h"
#include "ir_inline.h"
#include "ir_mem2reg.h"
#include "ir_canon.h"
#include "ir_licm.h"
#include "ir_constfold.h"
#include "ir_instcombine.h"
#include "ir_sccp.h"
#include "ir_gvn.h"
#include "ir_copyprop.h"
#include "ir_cse.h"
#include "ir_dce.h"
#include "ir_cfg_simplify.h"
#include "ir_verify_ir.h"
#include "ir_verify_ssa.h"

#include <stdio.h>

/** الحد الأقصى للتكرارات لتفادي حلقات لا نهائية */
#define MAX_ITERATIONS 10

/**
 * @brief الحصول على اسم مستوى التحسين.
 */
const char* ir_optimizer_level_name(OptLevel level) {
    switch (level) {
        case OPT_LEVEL_0: return "O0";
        case OPT_LEVEL_1: return "O1";
        case OPT_LEVEL_2: return "O2";
        default:          return "O?";
    }
}

/**
 * @brief تشغيل دورة واحدة من خط التحسين.
 *
 * @param module وحدة IR المراد تحسينها.
 * @param level  مستوى التحسين.
 * @return true إذا أحدثت أي تمريرة تغييرات، false خلاف ذلك.
 */
static bool optimizer_iteration(IRModule* module,
                                OptLevel level,
                                int verify_gate,
                                FILE* verify_out) {
    bool changed = false;

    // تمريرة 0: Mem2Reg (ترقية الذاكرة إلى سجلات) — SSA (فاي + إعادة تسمية)
    // يُحوِّل المتغيرات المحلية من alloca/load/store إلى SSA مع إدراج فاي عند الدمج.
    changed |= ir_mem2reg_run(module);

    // تمريرة 0.5: Canonicalization (توحيد_الـIR)
    // توحيد شكل التعليمات لزيادة فعالية CSE/ConstFold/DCE
    changed |= ir_canon_run(module);

    // تمريرة 0.6: InstCombine (دمج_التعليمات)
    // تبسيطات محلية سريعة قبل نشر الثوابت.
    changed |= ir_instcombine_run(module);

    // تمريرة 0.7: SCCP (نشر_الثوابت_المتناثر)
    // نشر الثوابت + تبسيط CFG بناءً على الوصول.
    changed |= ir_sccp_run(module);

    // تمريرة 1: Constant Folding (طي_الثوابت)
    // تطوي العمليات الحسابية عندما تكون المعاملات ثوابت
    changed |= ir_constfold_run(module);

    // تمريرة 2: Copy Propagation (نشر_النسخ)
    // تزيل نسخاً زائدة وتبسّط استعمالات السجلات
    changed |= ir_copyprop_run(module);

    // تمريرة 2.5: GVN (ترقيم_القيم) — فقط في O2
    // يزيل تعابير متكررة حتى لو اختلفت أرقام السجلات بسبب النسخ.
    if (level >= OPT_LEVEL_2) {
        changed |= ir_gvn_run(module);
    }

    // تمريرة 3: CSE (حذف_المكرر) — فقط في O2
    // تزيل التعابير المكررة
    if (level >= OPT_LEVEL_2) {
        changed |= ir_cse_run(module);
    }

    // تمريرة 4: DCE (حذف_الميت)
    // تزيل التعليمات غير المستخدمة والكتل غير القابلة للوصول
    changed |= ir_dce_run(module);

    // تمريرة 5: تبسيط CFG (تبسيط_CFG)
    // دمج كتل تافهة + إزالة أفرع زائدة لتحسين IR
    changed |= ir_cfg_simplify_run(module);

    // تمريرة 6: LICM (حركة التعليمات غير المتغيرة)
    // نقل التعليمات النقية غير المتغيرة في الحلقات إلى preheader
    changed |= ir_licm_run(module);

    // بوابة التحقق (Debug Gate): بعد كل دورة تمريرات
    if (verify_gate) {
        FILE* out = verify_out ? verify_out : stderr;

        if (!ir_module_verify_ir(module, out)) {
            fprintf(out, "فشل بوابة التحقق: IR غير صالح بعد تمريرة تحسين.\n");
            // فشل بوابة التحقق يعني فشل التحسين (نعتبره عدم نجاح).
            // نُرجع changed=true لإيقاف fixpoint بشكل صريح؟ الأفضل: أوقف فوراً.
            return true;
        }

        // إذا كانت SSA موجودة (بعد Mem2Reg) يمكن التحقق منها أيضاً.
        if (!ir_module_verify_ssa(module, out)) {
            fprintf(out, "فشل بوابة التحقق: SSA غير صالح بعد Mem2Reg/تمريرات تحسين.\n");
            return true;
        }
    }

    return changed;
}

/**
 * @brief تشغيل خط التحسين على وحدة IR.
 */
static int g_ir_optimizer_verify_gate = 0;

void ir_optimizer_set_verify_gate(int enabled) {
    g_ir_optimizer_verify_gate = enabled ? 1 : 0;
}

bool ir_optimizer_run(IRModule* module, OptLevel level) {
    if (!module) return false;

    // ضمان تخصيصات IR داخل تمريرات المُحسِّن ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);

    // O0: بدون تحسين (نجاح بدون تغييرات)
    if (level == OPT_LEVEL_0) {
        return true;
    }

    // v0.3.2.7.2: التضمين (Inlining) — O2 فقط، قبل Mem2Reg.
    if (level >= OPT_LEVEL_2) {
        (void)ir_inline_run(module);
    }

    int iteration = 0;

    // تكرار حتى نقطة التثبيت: تشغيل التمريرات حتى عدم وجود تغييرات
    while (iteration < MAX_ITERATIONS) {
        bool changed = optimizer_iteration(module, level, g_ir_optimizer_verify_gate, stderr);

        // عندما تكون بوابة التحقق مفعلة: optimizer_iteration قد تطبع الأخطاء
        // وتُرجع true بشكل غير دلالي. لذا نتحقق مباشرة من "سلامة" IR/SSA ونوقف.
        if (g_ir_optimizer_verify_gate) {
            if (!ir_module_verify_ir(module, stderr)) {
                return false;
            }
            if (!ir_module_verify_ssa(module, stderr)) {
                return false;
            }
        }

        if (!changed) {
            // تم الوصول لنقطة التثبيت — لم تُحدث أي تمريرة تغييرات
            break;
        }

        iteration++;
    }

    return true;
}
