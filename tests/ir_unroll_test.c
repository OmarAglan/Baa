/**
 * @file ir_unroll_test.c
 * @brief اختبار فك الحلقات (Full unroll) لحلقة بسيطة بعد Out-of-SSA.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_unroll.h"
#include "../src/ir_analysis.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "ir_unroll_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static int count_add_imm(IRBlock* b, int dest, int64_t imm) {
    if (!b) return 0;
    int c = 0;
    for (IRInst* inst = b->first; inst; inst = inst->next) {
        if (inst->op != IR_OP_ADD) continue;
        if (inst->dest != dest) continue;
        if (inst->operand_count < 2) continue;
        IRValue* a = inst->operands[0];
        IRValue* d = inst->operands[1];
        if (!a || !d) continue;
        if (d->kind == IR_VAL_CONST_INT && d->data.const_int == imm) c++;
        else if (a->kind == IR_VAL_CONST_INT && a->data.const_int == imm) c++;
    }
    return c;
}

int main(void) {
    int ok = 1;

    IRModule* m = ir_module_new("ir_unroll_test");
    ok &= require(m != NULL, "failed to create module");
    if (!ok) return 1;

    // Loop shape:
    // P: i = 0; sum = 0; br H
    // H: cond = (i < 3); br_cond cond, B, E
    // B: sum = sum + i; i = i + 1; br H
    // E: ret sum

    IRFunc* f = ir_func_new("test", IR_TYPE_I64_T);
    ok &= require(f != NULL, "failed to create func");
    if (!ok) return 1;
    ir_module_add_func(m, f);

    IRBlock* P = ir_block_new("pre", 0);
    IRBlock* H = ir_block_new("hdr", 1);
    IRBlock* B = ir_block_new("body", 2);
    IRBlock* E = ir_block_new("exit", 3);
    ok &= require(P && H && B && E, "failed to create blocks");
    if (!ok) return 1;

    ir_func_add_block(f, P);
    ir_func_add_block(f, H);
    ir_func_add_block(f, B);
    ir_func_add_block(f, E);
    f->entry = P;

    int r_i = ir_func_alloc_reg(f);
    int r_sum = ir_func_alloc_reg(f);
    int r_cond = ir_func_alloc_reg(f);

    // init
    ir_block_append(P, ir_inst_new(IR_OP_COPY, IR_TYPE_I64_T, r_i));
    ir_inst_add_operand(P->last, ir_value_const_int(0, IR_TYPE_I64_T));
    ir_block_append(P, ir_inst_new(IR_OP_COPY, IR_TYPE_I64_T, r_sum));
    ir_inst_add_operand(P->last, ir_value_const_int(0, IR_TYPE_I64_T));
    ir_block_append(P, ir_inst_br(H));

    // header
    IRInst* cmp = ir_inst_cmp(IR_CMP_LT, r_cond,
                              ir_value_reg(r_i, IR_TYPE_I64_T),
                              ir_value_const_int(3, IR_TYPE_I64_T));
    ir_block_append(H, cmp);
    ir_block_append(H, ir_inst_br_cond(ir_value_reg(r_cond, IR_TYPE_I1_T), B, E));

    // body
    IRInst* add_sum = ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, r_sum,
                                     ir_value_reg(r_sum, IR_TYPE_I64_T),
                                     ir_value_reg(r_i, IR_TYPE_I64_T));
    ir_block_append(B, add_sum);
    IRInst* inc = ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, r_i,
                                 ir_value_reg(r_i, IR_TYPE_I64_T),
                                 ir_value_const_int(1, IR_TYPE_I64_T));
    ir_block_append(B, inc);
    ir_block_append(B, ir_inst_br(H));

    // exit
    ir_block_append(E, ir_inst_ret(ir_value_reg(r_sum, IR_TYPE_I64_T)));

    // compute succ/preds
    ir_func_rebuild_preds(f);

    // Unroll
    ok &= require(ir_unroll_run(m, 8) == true, "expected unroll to modify IR");

    // preheader should now branch to exit, not header
    ok &= require(P->last && P->last->op == IR_OP_BR, "pre should end with br");
    ok &= require(P->last->operands[0] && P->last->operands[0]->kind == IR_VAL_BLOCK,
                  "pre br target must be block");
    ok &= require(P->last->operands[0]->data.block == E, "pre should branch directly to exit after unroll");

    // Expect exactly 3 increments of i by 1 in preheader (for trip=3)
    ok &= require(count_add_imm(P, r_i, 1) == 3, "expected 3 i=i+1 in preheader after unroll");

    ir_module_free(m);
    if (!ok) return 1;
    fprintf(stderr, "ir_unroll_test: PASS\n");
    return 0;
}
