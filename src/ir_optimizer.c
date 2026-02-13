/**
 * @file ir_optimizer.c
 * @brief IR optimization pipeline implementation (v0.3.1.6).
 *
 * Implements the unified optimization pipeline with:
 * - Pass ordering (constfold → copyprop → CSE → DCE)
 * - Fixpoint iteration until no changes
 * - Optimization level control
 */

#include "ir_optimizer.h"
#include "ir_mem2reg.h"
#include "ir_canon.h"
#include "ir_constfold.h"
#include "ir_copyprop.h"
#include "ir_cse.h"
#include "ir_dce.h"
#include "ir_cfg_simplify.h"
#include "ir_verify_ir.h"
#include "ir_verify_ssa.h"

#include <stdio.h>

/** Maximum iterations to prevent infinite loops */
#define MAX_ITERATIONS 10

/**
 * @brief Get the name of an optimization level.
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
 * @brief Run a single iteration of the optimization pipeline.
 *
 * @param module The IR module to optimize.
 * @param level  Optimization level.
 * @return true if any pass made changes; false otherwise.
 */
static bool optimizer_iteration(IRModule* module,
                                OptLevel level,
                                int verify_gate,
                                FILE* verify_out) {
    bool changed = false;

    // Pass 0: Mem2Reg (ترقية الذاكرة إلى سجلات) — SSA (فاي + إعادة تسمية)
    // يُحوِّل المتغيرات المحلية من alloca/load/store إلى SSA مع إدراج فاي عند الدمج.
    changed |= ir_mem2reg_run(module);

    // Pass 0.5: Canonicalization (توحيد_الـIR)
    // توحيد شكل التعليمات لزيادة فعالية CSE/ConstFold/DCE
    changed |= ir_canon_run(module);

    // Pass 1: Constant Folding (طي_الثوابت)
    // Folds arithmetic with constant operands
    changed |= ir_constfold_run(module);

    // Pass 2: Copy Propagation (نشر_النسخ)
    // Removes redundant copies, simplifies register uses
    changed |= ir_copyprop_run(module);

    // Pass 3: Common Subexpression Elimination (حذف_المكرر) — O2 only
    // Eliminates duplicate expressions
    if (level >= OPT_LEVEL_2) {
        changed |= ir_cse_run(module);
    }

    // Pass 4: Dead Code Elimination (حذف_الميت)
    // Removes unused instructions and unreachable blocks
    changed |= ir_dce_run(module);

    // Pass 5: CFG simplification (تبسيط_CFG)
    // دمج كتل تافهة + إزالة أفرع زائدة لتحسين IR
    changed |= ir_cfg_simplify_run(module);

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
 * @brief Run the optimization pipeline on an IR module.
 */
static int g_ir_optimizer_verify_gate = 0;

void ir_optimizer_set_verify_gate(int enabled) {
    g_ir_optimizer_verify_gate = enabled ? 1 : 0;
}

bool ir_optimizer_run(IRModule* module, OptLevel level) {
    if (!module) return false;

    // ضمان تخصيصات IR داخل تمريرات المُحسِّن ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);

    // O0: No optimization (نجاح بدون تغييرات)
    if (level == OPT_LEVEL_0) {
        return true;
    }

    int iteration = 0;

    // Fixpoint iteration: run passes until no changes
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
            // Fixpoint reached — no pass made changes
            break;
        }

        iteration++;
    }

    return true;
}
