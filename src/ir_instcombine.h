/**
 * @file ir_instcombine.h
 * @brief تمريرة دمج/تبسيط التعليمات (InstCombine) — v0.3.2.8.6.
 */

#ifndef BAA_IR_INSTCOMBINE_H
#define BAA_IR_INSTCOMBINE_H

#include <stdbool.h>

#include "ir_pass.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ir_instcombine_run(IRModule* module);

extern IRPass IR_PASS_INSTCOMBINE;

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_INSTCOMBINE_H
