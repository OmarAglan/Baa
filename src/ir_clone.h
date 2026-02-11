/**
 * @file ir_clone.h
 * @brief استنساخ (Clone) دوال/كتل IR بعمق.
 */

#ifndef BAA_IR_CLONE_H
#define BAA_IR_CLONE_H

#include "ir.h"

IRFunc* ir_func_clone(IRModule* module, IRFunc* src, const char* new_name);

#endif // BAA_IR_CLONE_H
