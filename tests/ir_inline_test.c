/**
 * @file ir_inline_test.c
 * @brief اختبار تضمين الدوال (Inlining) — توسيع CALL داخل الدالة.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_inline.h"
#include "../src/ir_verify_ir.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "ir_inline_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static int block_has_call(IRFunc* f, const char* target) {
    if (!f || !target) return 0;
    for (IRBlock* b = f->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->op == IR_OP_CALL && inst->call_target && strcmp(inst->call_target, target) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    int ok = 1;

    IRModule* m = ir_module_new("ir_inline_test");
    ok &= require(m != NULL, "failed to create module");
    if (!ok) return 1;

    // callee: i64 @add1(i64 %p0) { entry: %t = add %p0, 1; ret %t }
    IRFunc* callee = ir_func_new("add1", IR_TYPE_I64_T);
    ok &= require(callee != NULL, "failed to create callee");
    ir_func_add_param(callee, "p", IR_TYPE_I64_T);
    IRBlock* ce = ir_func_new_block(callee, "entry");
    ok &= require(ce != NULL, "failed to create callee entry");
    int rp = callee->params[0].reg;
    int rt = ir_func_alloc_reg(callee);
    ir_block_append(ce, ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, rt,
                                      ir_value_reg(rp, IR_TYPE_I64_T),
                                      ir_value_const_int(1, IR_TYPE_I64_T)));
    ir_block_append(ce, ir_inst_ret(ir_value_reg(rt, IR_TYPE_I64_T)));
    ir_module_add_func(m, callee);

    // caller: i64 @main() { entry: %x=const; %y=call add1(%x); ret %y }
    IRFunc* caller = ir_func_new("main", IR_TYPE_I64_T);
    ok &= require(caller != NULL, "failed to create caller");
    IRBlock* be = ir_func_new_block(caller, "entry");
    ok &= require(be != NULL, "failed to create caller entry");

    int rx = ir_func_alloc_reg(caller);
    IRInst* cx = ir_inst_new(IR_OP_COPY, IR_TYPE_I64_T, rx);
    ir_inst_add_operand(cx, ir_value_const_int(41, IR_TYPE_I64_T));
    ir_block_append(be, cx);

    int ry = ir_func_alloc_reg(caller);
    {
        IRValue* args[1];
        args[0] = ir_value_reg(rx, IR_TYPE_I64_T);
        ir_block_append(be, ir_inst_call("add1", IR_TYPE_I64_T, ry, args, 1));
    }
    ir_block_append(be, ir_inst_ret(ir_value_reg(ry, IR_TYPE_I64_T)));
    ir_module_add_func(m, caller);

    ok &= require(block_has_call(caller, "add1") == 1, "expected call before inlining");

    ok &= require(ir_inline_run(m) == true, "expected inliner to change module");
    ok &= require(block_has_call(caller, "add1") == 0, "expected call removed after inlining");

    ok &= require(ir_module_verify_ir(m, stderr) == true, "IR must be well-formed after inlining");

    ir_module_free(m);
    if (!ok) return 1;
    fprintf(stderr, "ir_inline_test: PASS\n");
    return 0;
}
