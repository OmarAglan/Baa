/**
 * @file ir_licm_test.c
 * @brief اختبار LICM (Loop Invariant Code Motion) بشكل محافظ.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_licm.h"
#include "../src/ir_analysis.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "ir_licm_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static IRInst* find_add_by_operands(IRBlock* b, int dest, int lhs_reg, int rhs_reg) {
    if (!b) return NULL;
    for (IRInst* inst = b->first; inst; inst = inst->next) {
        if (inst->op != IR_OP_ADD) continue;
        if (inst->dest != dest) continue;
        if (inst->operand_count < 2) continue;
        IRValue* a = inst->operands[0];
        IRValue* c = inst->operands[1];
        if (!a || !c) continue;
        if (a->kind != IR_VAL_REG || c->kind != IR_VAL_REG) continue;
        if (a->data.reg_num == lhs_reg && c->data.reg_num == rhs_reg) return inst;
        if (a->data.reg_num == rhs_reg && c->data.reg_num == lhs_reg) return inst;
    }
    return NULL;
}

int main(void) {
    int ok = 1;

    IRModule* m = ir_module_new("ir_licm_test");
    ok &= require(m != NULL, "failed to create module");
    if (!ok) return 1;

    // دالة بسيطة مع حلقة طبيعية: P -> H -> B -> H
    IRFunc* f = ir_func_new("test", IR_TYPE_VOID_T);
    ok &= require(f != NULL, "failed to create func");
    if (!ok) return 1;
    ir_module_add_func(m, f);

    IRBlock* P = ir_block_new("pre", 0);
    IRBlock* H = ir_block_new("header", 1);
    IRBlock* B = ir_block_new("body", 2);
    IRBlock* E = ir_block_new("exit", 3);

    ok &= require(P && H && B && E, "failed to create blocks");
    if (!ok) return 1;

    ir_func_add_block(f, P);
    ir_func_add_block(f, H);
    ir_func_add_block(f, B);
    ir_func_add_block(f, E);
    f->entry = P;

    // سجلات خارج الحلقة
    int rA = ir_func_alloc_reg(f);
    int rB = ir_func_alloc_reg(f);
    int rT = ir_func_alloc_reg(f);

    ir_block_append(P, ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, rA,
                                     ir_value_const_int(40, IR_TYPE_I64_T),
                                     ir_value_const_int(2, IR_TYPE_I64_T)));
    ir_block_append(P, ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, rB,
                                     ir_value_const_int(7, IR_TYPE_I64_T),
                                     ir_value_const_int(3, IR_TYPE_I64_T)));
    ir_block_append(P, ir_inst_br(H));

    // header: شرط ثابت "صواب" لمجرد بناء CFG
    int rCond = ir_func_alloc_reg(f);
    ir_block_append(H, ir_inst_cmp(IR_CMP_NE, rCond,
                                  ir_value_const_int(1, IR_TYPE_I64_T),
                                  ir_value_const_int(0, IR_TYPE_I64_T)));
    ir_block_append(H, ir_inst_br_cond(ir_value_reg(rCond, IR_TYPE_I1_T), B, E));

    // body: تعليمة invariant داخل الحلقة: %rT = add %rA, %rB
    ir_block_append(B, ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, rT,
                                     ir_value_reg(rA, IR_TYPE_I64_T),
                                     ir_value_reg(rB, IR_TYPE_I64_T)));

    // استخدم rT داخل CALL حتى لا تُحذف التعليمة (لكننا لا ننقل CALL).
    {
        IRValue* args[1];
        args[0] = ir_value_reg(rT, IR_TYPE_I64_T);
        ir_block_append(B, ir_inst_call("side_effect", IR_TYPE_VOID_T, -1, args, 1));
    }

    ir_block_append(B, ir_inst_br(H));

    ir_block_append(E, ir_inst_ret(NULL));

    // قبل LICM: يجب أن يكون add rA,rB موجوداً في body
    ok &= require(find_add_by_operands(B, rT, rA, rB) != NULL, "expected invariant add in body before LICM");

    // تشغيل LICM
    (void)ir_func_compute_dominators(f);
    ok &= require(ir_licm_run(m) == true, "LICM should report changes");

    // بعد LICM: يجب أن ينتقل add إلى preheader، ولا يبقى في body
    ok &= require(find_add_by_operands(P, rT, rA, rB) != NULL, "expected invariant add in preheader after LICM");
    ok &= require(find_add_by_operands(B, rT, rA, rB) == NULL, "invariant add should be hoisted out of body");

    ir_module_free(m);

    if (!ok) return 1;
    fprintf(stderr, "ir_licm_test: PASS\n");
    return 0;
}
