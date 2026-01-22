/**
 * @file ir_constfold_test.c
 * @brief IR constant folding tests (v0.3.1.2).
 *
 * Builds a small IR module manually and runs constant folding:
 * - Arithmetic: جمع/طرح
 * - Comparison: قارن أكبر
 *
 * Verifies:
 * - Folded instruction removed
 * - Uses of folded registers replaced with immediate constants
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_constfold.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static IRInst* find_first_inst_by_op(IRBlock* block, IROp op) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == op) return inst;
    }
    return NULL;
}

static IRInst* find_inst_by_dest(IRBlock* block, int dest) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->dest == dest) return inst;
    }
    return NULL;
}

int main(void) {
    IRModule* module = ir_module_new("constfold_test");
    if (!module) {
        fprintf(stderr, "TEST FAIL: module alloc\n");
        return 1;
    }

    IRBuilder* b = ir_builder_new(module);
    if (!b) {
        fprintf(stderr, "TEST FAIL: builder alloc\n");
        ir_module_free(module);
        return 1;
    }

    // func @الرئيسية() -> ص٦٤ { بداية: ... }
    IRFunc* f = ir_builder_create_func(b, "الرئيسية", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    ir_builder_set_insert_point(b, entry);

    // %r_add = جمع ص٦٤ 5, 3   -> fold to 8
    int r_add = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                   ir_value_const_int(5, IR_TYPE_I64_T),
                                   ir_value_const_int(3, IR_TYPE_I64_T));

    // %r_sub = طرح ص٦٤ %r_add, 1   -> after folding should become طرح ص٦٤ 8, 1 (and then itself could fold later)
    int r_sub = ir_builder_emit_sub(b, IR_TYPE_I64_T,
                                   ir_value_reg(r_add, IR_TYPE_I64_T),
                                   ir_value_const_int(1, IR_TYPE_I64_T));

    // %r_cmp = قارن أكبر ص٦٤ 10, 5 -> fold to 1
    int r_cmp = ir_builder_emit_cmp_gt(b,
                                      ir_value_const_int(10, IR_TYPE_I64_T),
                                      ir_value_const_int(5, IR_TYPE_I64_T));

    // Use cmp as operand to a boolean op so replacement is tested: %r_and = و ص١ %r_cmp, 1
    int r_and = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                   ir_value_reg(r_cmp, IR_TYPE_I1_T),
                                   ir_value_const_int(1, IR_TYPE_I1_T));

    // Return %r_sub (not relevant to folding itself)
    ir_builder_emit_ret(b, ir_value_reg(r_sub, IR_TYPE_I64_T));

    // Run pass
    int changed = ir_constfold_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "constfold should report changed");

    // Folded add and cmp instructions should be removed
    ok &= require(find_inst_by_dest(entry, r_add) == NULL, "folded add inst should be removed");
    ok &= require(find_inst_by_dest(entry, r_cmp) == NULL, "folded cmp inst should be removed");

    // The sub instruction should be folded away; its uses (return) should become constant 7.
    ok &= require(find_inst_by_dest(entry, r_sub) == NULL, "folded sub inst should be removed");

    // The AND instruction should now have const 1 as LHS (since %r_cmp folded to true)
    IRInst* and_inst = find_inst_by_dest(entry, r_and);
    ok &= require(and_inst != NULL, "and inst should exist");
    if (and_inst) {
        ok &= require(and_inst->operand_count >= 2, "and inst should have 2 operands");
        ok &= require(and_inst->operands[0] && and_inst->operands[0]->kind == IR_VAL_CONST_INT, "and lhs should be const");
        ok &= require(and_inst->operands[0]->data.const_int == 1, "and lhs const should be 1");
    }

    // Sanity: we still have a return terminator, and it returns constant 7.
    IRInst* ret_inst = find_first_inst_by_op(entry, IR_OP_RET);
    ok &= require(ret_inst != NULL, "entry should have ret");
    if (ret_inst) {
        ok &= require(ret_inst->operand_count == 1, "ret should have 1 operand");
        ok &= require(ret_inst->operands[0] && ret_inst->operands[0]->kind == IR_VAL_CONST_INT, "ret operand should be const");
        ok &= require(ret_inst->operands[0]->data.const_int == 7, "ret constant should be 7");
    }

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_constfold_test: PASS\n");
    return 0;
}