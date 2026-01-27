/**
 * @file ir_dce.h
 * @brief IR dead code elimination pass (حذف_الميت).
 * @version 0.3.1.3
 *
 * This pass removes:
 * - Dead SSA instructions: instructions that produce a value (dest register) that is never used,
 *   and have no side effects.
 * - Unreachable basic blocks: blocks that are not reachable from the function entry block.
 *
 * Notes:
 * - Calls are conservatively treated as side-effecting (never removed).
 * - Stores and terminators (br/br_cond/ret) are always considered live.
 */

#ifndef BAA_IR_DCE_H
#define BAA_IR_DCE_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run dead code elimination on an IR module.
 *
 * @return true if the module was modified; false otherwise.
 */
bool ir_dce_run(IRModule* module);

/**
 * @brief `IRPass` descriptor for dead code elimination (حذف_الميت).
 */
extern IRPass IR_PASS_DCE;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_DCE_H