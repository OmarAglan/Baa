/**
 * @file ir_gvn.h
 * @brief تمريرة ترقيم القيم العالمية (GVN) — v0.3.2.8.6.
 */

#ifndef BAA_IR_GVN_H
#define BAA_IR_GVN_H

#include <stdbool.h>

#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ir_gvn_run(IRModule* module);

extern IRPass IR_PASS_GVN;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_GVN_H
