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
#include "ir_constfold.h"
#include "ir_copyprop.h"
#include "ir_cse.h"
#include "ir_dce.h"

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
static bool optimizer_iteration(IRModule* module, OptLevel level) {
    bool changed = false;

    // Pass 0: Mem2Reg (ترقية الذاكرة إلى سجلات) — SSA (فاي + إعادة تسمية)
    // يُحوِّل المتغيرات المحلية من alloca/load/store إلى SSA مع إدراج فاي عند الدمج.
    changed |= ir_mem2reg_run(module);

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

    return changed;
}

/**
 * @brief Run the optimization pipeline on an IR module.
 */
bool ir_optimizer_run(IRModule* module, OptLevel level) {
    if (!module) return false;

    // ضمان تخصيصات IR داخل تمريرات المُحسِّن ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);
    
    // O0: No optimization
    if (level == OPT_LEVEL_0) {
        return false;
    }

    bool changed_total = false;
    int iteration = 0;

    // Fixpoint iteration: run passes until no changes
    while (iteration < MAX_ITERATIONS) {
        bool changed = optimizer_iteration(module, level);
        if (!changed) {
            // Fixpoint reached — no pass made changes
            break;
        }
        changed_total = true;
        iteration++;
    }

    return changed_total;
}
