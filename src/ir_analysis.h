/**
 * @file ir_analysis.h
 * @brief IR analysis infrastructure (CFG + dominance) for the Baa compiler.
 * @version 0.3.1.1
 *
 * This module provides analysis utilities over Baa IR:
 *  - CFG validation (each block must end with a terminator)
 *  - Rebuilding predecessor lists from terminator instructions
 *  - Dominator tree computation (immediate dominators + dominance frontier)
 *
 * This is the first step toward implementing the optimizer pipeline (v0.3.1.x).
 */

#ifndef BAA_IR_ANALYSIS_H
#define BAA_IR_ANALYSIS_H

#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CFG Validation
// ============================================================================

/**
 * @brief Validate that an IR function has a well-formed CFG.
 *
 * Checks:
 * - Every block has a terminator instruction (br/br_cond/ret)
 * - Terminator operands are well-formed (branch targets are blocks, etc.)
 *
 * @return true if the CFG is valid; false otherwise.
 */
bool ir_func_validate_cfg(IRFunc* func);

/**
 * @brief Validate CFG of all (non-prototype) functions in a module.
 * @return true if all functions are valid; false if any function is invalid.
 */
bool ir_module_validate_cfg(IRModule* module);

// ============================================================================
// Predecessor List Rebuild
// ============================================================================

/**
 * @brief Rebuild successor and predecessor edges for every block in a function.
 *
 * This clears `succs[]/succ_count` and rebuilds them from the terminator
 * instruction of each block. Predecessor arrays (`preds[]`) are also rebuilt.
 *
 * Dominance caches (idom/frontier) are cleared as part of this rebuild.
 */
void ir_func_rebuild_preds(IRFunc* func);

/**
 * @brief Rebuild successor and predecessor edges for every function in a module.
 */
void ir_module_rebuild_preds(IRModule* module);

// ============================================================================
// Dominance Analysis
// ============================================================================

/**
 * @brief Compute immediate dominators + dominance frontier for a function.
 *
 * Requirements:
 * - `ir_func_rebuild_preds()` should have been run (or CFG edges must be valid).
 *
 * Notes:
 * - Unreachable blocks will have `idom = NULL`.
 * - Entry block will have `idom = entry`.
 */
void ir_func_compute_dominators(IRFunc* func);

/**
 * @brief Compute dominators for every function in a module.
 */
void ir_module_compute_dominators(IRModule* module);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_ANALYSIS_H