/**
 * @file ir_pass.c
 * @brief IR pass interface (optimizer infrastructure).
 * @version 0.3.1.1
 */

#include "ir_pass.h"

bool ir_pass_run(IRPass* pass, IRModule* module) {
    if (!pass || !pass->run || !module) return false;
    return pass->run(module);
}