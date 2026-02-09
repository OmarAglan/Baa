/**
 * @file ir_optimizer.h
 * @brief IR optimization pipeline (v0.3.1.6).
 *
 * Provides a unified interface for running IR optimization passes in
 * the correct order with fixpoint iteration until no further changes.
 *
 * Optimization Levels:
 * - O0: No optimization (for debugging)
 * - O1: Basic optimizations (mem2reg, constfold, copyprop, dce)
 * - O2: Full optimizations (+ CSE, fixpoint iteration)
 */

#ifndef BAA_IR_OPTIMIZER_H
#define BAA_IR_OPTIMIZER_H

#include <stdbool.h>
#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Optimization levels for the compiler.
 */
typedef enum {
    OPT_LEVEL_0 = 0,  /**< No optimization (debug mode) */
    OPT_LEVEL_1 = 1,  /**< Basic optimizations (default) */
    OPT_LEVEL_2 = 2   /**< Full optimizations */
} OptLevel;

/**
 * @brief Run the optimization pipeline on an IR module.
 *
 * Pass ordering (O1+):
 * 0. Mem2Reg (ترقية الذاكرة إلى سجلات) — baseline
 * 1. Constant Folding (طي_الثوابت)
 * 2. Copy Propagation (نشر_النسخ)
 * 3. CSE (حذف_المكرر) — O2 only
 * 4. Dead Code Elimination (حذف_الميت)
 *
 * The pipeline iterates until no pass makes changes (fixpoint)
 * or a maximum iteration count is reached.
 *
 * @param module The IR module to optimize.
 * @param level  Optimization level (OPT_LEVEL_0, OPT_LEVEL_1, OPT_LEVEL_2).
 * @return true if any optimization was performed; false otherwise.
 */
bool ir_optimizer_run(IRModule* module, OptLevel level);

/**
 * @brief Get the name of an optimization level.
 * @param level The optimization level.
 * @return String representation (e.g., "O0", "O1", "O2").
 */
const char* ir_optimizer_level_name(OptLevel level);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_OPTIMIZER_H
