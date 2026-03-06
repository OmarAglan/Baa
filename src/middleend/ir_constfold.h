/**
 * @file ir_constfold.h
 * @brief IR constant folding pass (طي_الثوابت).
 * @version 0.3.1.2
 *
 * Implements a simple constant folding pass over Baa IR:
 * - Fold arithmetic ops when operands are immediate integer constants
 * - Fold comparisons when operands are immediate integer constants
 * - Replace uses of folded registers with constant immediates
 */

#ifndef BAA_IR_CONSTFOLD_H
#define BAA_IR_CONSTFOLD_H

#include <stdbool.h>
#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run constant folding on a module.
 *
 * @return true if the module was modified; false otherwise.
 */
bool ir_constfold_run(IRModule* module);

/**
 * @brief `IRPass` descriptor for constant folding (طي_الثوابت).
 */
extern IRPass IR_PASS_CONSTFOLD;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_CONSTFOLD_H