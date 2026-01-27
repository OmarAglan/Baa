/**
 * @file ir_copyprop.h
 * @brief IR copy propagation pass (نشر_النسخ).
 * @version 0.3.1.4
 *
 * Copy propagation replaces uses of copy-defined SSA registers with their original
 * source values and removes redundant copy instructions where possible.
 *
 * This pass targets explicit IR copy instructions:
 *   %dest = نسخ <type> <source>
 */

#ifndef BAA_IR_COPYPROP_H
#define BAA_IR_COPYPROP_H

#include <stdbool.h>

#include "ir.h"
#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run copy propagation on an IR module.
 *
 * - Detects `IR_OP_COPY` instructions.
 * - Replaces all uses of the copy destination register with the copy source value.
 * - Removes copy instructions whose destination becomes unused.
 *
 * @return true if the module was modified; false otherwise.
 */
bool ir_copyprop_run(IRModule* module);

/**
 * @brief `IRPass` descriptor for copy propagation (نشر_النسخ).
 */
extern IRPass IR_PASS_COPYPROP;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_COPYPROP_H