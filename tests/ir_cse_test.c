/**
 * @file ir_cse_test.c
 * @brief IR common subexpression elimination tests (v0.3.1.5).
 *
 * Builds a small IR module manually and runs CSE:
 * - Duplicate arithmetic: جمع/ضرب
 * - Duplicate comparison: قارن
 *
 * Verifies:
 * - Duplicate instruction removed
 * - Uses of duplicate register replaced with original
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_cse.h"

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

static IRInst* find_first_inst_by_op(IRBlock* block, IROp op) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == op) return inst;
    }
    return NULL;
}

int main(void) {
    IRModule* module = ir_module_new("cse_test");
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

    // %م٠ = حمل (simulate a variable)
    // For CSE testing, we use register values directly
    
    // %r0 = جمع ص٦٤ 5, 3   -> first occurrence
    int r0 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                 ir_value_const_int(5, IR_TYPE_I64_T),
                                 ir_value_const_int(3, IR_TYPE_I64_T));

    // %r1 = ضرب ص٦٤ %r0, 2   -> use r0
    int r1 = ir_builder_emit_mul(b, IR_TYPE_I64_T,
                                 ir_value_reg(r0, IR_TYPE_I64_T),
                                 ir_value_const_int(2, IR_TYPE_I64_T));

    // %r2 = جمع ص٦٤ 5, 3   -> DUPLICATE of r0! Should be eliminated
    int r2 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                 ir_value_const_int(5, IR_TYPE_I64_T),
                                 ir_value_const_int(3, IR_TYPE_I64_T));

    // %r3 = ضرب ص٦٤ %r2, 4   -> uses r2, should be rewritten to use r0
    int r3 = ir_builder_emit_mul(b, IR_TYPE_I64_T,
                                 ir_value_reg(r2, IR_TYPE_I64_T),
                                 ir_value_const_int(4, IR_TYPE_I64_T));

    // Return r3
    ir_builder_emit_ret(b, ir_value_reg(r3, IR_TYPE_I64_T));

    // Run CSE pass
    int changed = ir_cse_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "CSE should report changed");

    // Duplicate add (r2) should be removed
    ok &= require(find_inst_by_dest(entry, r2) == NULL, "duplicate add inst should be removed");

    // Original add (r0) should still exist
    ok &= require(find_inst_by_dest(entry, r0) != NULL, "original add inst should exist");

    // The mul instruction using r2 should now use r0
    IRInst* mul_inst = find_inst_by_dest(entry, r3);
    ok &= require(mul_inst != NULL, "mul inst using r2 should exist");
    if (mul_inst) {
        ok &= require(mul_inst->operand_count >= 1, "mul inst should have operands");
        ok &= require(mul_inst->operands[0] != NULL, "mul lhs should exist");
        ok &= require(mul_inst->operands[0]->kind == IR_VAL_REG, "mul lhs should be reg");
        ok &= require(mul_inst->operands[0]->data.reg_num == r0, 
                      "mul lhs should now be r0 (not r2)");
    }

    // Non-duplicate mul (r1) should still exist
    ok &= require(find_inst_by_dest(entry, r1) != NULL, "non-dup mul inst should exist");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_cse_test: PASS\n");
    return 0;
}
