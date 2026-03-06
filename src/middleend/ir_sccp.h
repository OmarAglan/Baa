/**
 * @file ir_sccp.h
 * @brief تمريرة نشر الثوابت المتناثر (SCCP) — v0.3.2.8.6.
 */

#ifndef BAA_IR_SCCP_H
#define BAA_IR_SCCP_H

#include <stdbool.h>

#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ir_sccp_run(IRModule* module);

extern IRPass IR_PASS_SCCP;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_SCCP_H
