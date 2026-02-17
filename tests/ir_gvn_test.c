/**
 * @file ir_gvn_test.c
 * @brief اختبارات GVN — v0.3.2.8.6.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_gvn.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static IRInst* find_inst_by_dest(IRBlock* block, int dest) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->dest == dest) return inst;
    }
    return NULL;
}

int main(void) {
    IRModule* module = ir_module_new("gvn_test");
    if (!module) return 1;

    IRBuilder* b = ir_builder_new(module);
    if (!b) {
        ir_module_free(module);
        return 1;
    }

    IRFunc* f = ir_builder_create_func(b, "f", IR_TYPE_I64_T);
    (void)ir_builder_add_param(b, "a", IR_TYPE_I64_T);
    (void)ir_builder_add_param(b, "b", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "entry");
    IRBlock* bb1 = ir_builder_create_block(b, "bb1");

    ir_builder_set_insert_point(b, entry);
    int a = f->params[0].reg;
    int br = f->params[1].reg;

    int r0 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                ir_value_reg(a, IR_TYPE_I64_T),
                                ir_value_reg(br, IR_TYPE_I64_T));
    ir_builder_emit_br(b, bb1);

    ir_builder_set_insert_point(b, bb1);
    int a2 = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_reg(a, IR_TYPE_I64_T));
    int b2 = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_reg(br, IR_TYPE_I64_T));
    int r1 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                ir_value_reg(a2, IR_TYPE_I64_T),
                                ir_value_reg(b2, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r1, IR_TYPE_I64_T));

    int changed = ir_gvn_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "GVN should report changed");
    ok &= require(find_inst_by_dest(bb1, r1) == NULL, "duplicate add should be removed");

    // تحقق أن return يستخدم r0 الآن.
    ok &= require(bb1->last && bb1->last->op == IR_OP_RET, "bb1 must end with ret");
    ok &= require(bb1->last && bb1->last->operand_count == 1, "ret must return a value");
    ok &= require(bb1->last && bb1->last->operands[0] && bb1->last->operands[0]->kind == IR_VAL_REG,
                  "ret operand must be reg");
    ok &= require(bb1->last && bb1->last->operands[0]->data.reg_num == r0, "ret should use r0");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;
    fprintf(stderr, "ir_gvn_test: PASS\n");
    return 0;
}
