/**
 * @file ir_instcombine_test.c
 * @brief اختبارات InstCombine — v0.3.2.8.6.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_instcombine.h"

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
    IRModule* module = ir_module_new("instcombine_test");
    if (!module) return 1;

    IRBuilder* b = ir_builder_new(module);
    if (!b) {
        ir_module_free(module);
        return 1;
    }

    IRFunc* f = ir_builder_create_func(b, "f", IR_TYPE_I64_T);
    (void)ir_builder_add_param(b, "x", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int x = f->params[0].reg;
    int y = ir_builder_emit_add(b, IR_TYPE_I64_T,
                               ir_value_reg(x, IR_TYPE_I64_T),
                               ir_value_const_int(0, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(y, IR_TYPE_I64_T));

    int changed = ir_instcombine_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "InstCombine should report changed");

    IRInst* yi = find_inst_by_dest(entry, y);
    ok &= require(yi != NULL, "expected instruction for y");
    ok &= require(yi && yi->op == IR_OP_COPY, "y should be rewritten to COPY");
    ok &= require(yi && yi->operand_count == 1, "COPY should have 1 operand");
    ok &= require(yi && yi->operands[0] && yi->operands[0]->kind == IR_VAL_REG, "COPY operand should be reg");
    ok &= require(yi && yi->operands[0] && yi->operands[0]->data.reg_num == x, "COPY should forward x");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;
    fprintf(stderr, "ir_instcombine_test: PASS\n");
    return 0;
}
