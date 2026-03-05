/**
 * @file ir_pass.c
 * @brief واجهة تمريرات IR (بنية المُحسِّن).
 * @version 0.3.1.1
 */

#include "ir_pass.h"

bool ir_pass_run(IRPass* pass, IRModule* module) {
    if (!pass || !pass->run || !module) return false;
    return pass->run(module);
}
