/**
 * @file ir_cse.h
 * @brief IR common subexpression elimination pass (حذف_المكرر).
 * @version 0.3.1.5
 *
 * Common Subexpression Elimination (CSE) detects duplicate computations
 * with identical opcode and operands, replacing subsequent uses with
 * the first computed result.
 *
 * Eligible operations:
 * - Arithmetic: جمع، طرح، ضرب، قسم، باقي
 * - Comparisons: قارن
 * - Logical: و، أو، نفي
 *
 * NOT eligible (side effects or non-deterministic):
 * - Memory: حجز، حمل، خزن
 * - Control: نداء، فاي، أفرع
 */

#ifndef BAA_IR_CSE_H
#define BAA_IR_CSE_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run common subexpression elimination on an IR module.
 *
 * For each function:
 * - Hash each pure expression (opcode + operands)
 * - If duplicate found, replace uses with original result
 * - Remove redundant instruction
 *
 * @return true if the module was modified; false otherwise.
 */
bool ir_cse_run(IRModule* module);

/**
 * @brief `IRPass` descriptor for CSE (حذف_المكرر).
 */
extern IRPass IR_PASS_CSE;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_CSE_H
