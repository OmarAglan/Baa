/**
 * @file ir_copyprop_test.c
 * @brief IR copy propagation tests (v0.3.1.4).
 *
 * Verifies:
 * - Uses of registers defined by `نسخ` are replaced with their original source values.
 * - Copy chains are canonicalized (%م٣ = نسخ %م٢ = نسخ %م١ ...).
 * - Copy propagation applies to:
 *   - normal operands
 *   - call arguments
 *   - phi incoming values
 * - Redundant `نسخ` instructions are removed.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_analysis.h"
#include "../src/ir_builder.h"
#include "../src/ir_copyprop.h"

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

static int block_has_op(IRBlock* block, IROp op) {
    if (!block) return 0;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == op) return 1;
    }
    return 0;
}

int main(void) {
    IRModule* module = ir_module_new("copyprop_test");
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

    // func @الرئيسية() -> ص٦٤
    (void)ir_builder_create_func(b, "الرئيسية", IR_TYPE_I64_T);

    // Blocks for phi test
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    IRBlock* then_bb = ir_builder_create_block(b, "فرع١");
    IRBlock* else_bb = ir_builder_create_block(b, "فرع٢");
    IRBlock* merge_bb = ir_builder_create_block(b, "دمج");

    // --------------------------------------
    // Entry block: normal operands + call args
    // --------------------------------------
    ir_builder_set_insert_point(b, entry);

    // %م0 = نسخ ص٦٤ 42
    int r_c42 = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_const_int(42, IR_TYPE_I64_T));

    // %م1 = جمع ص٦٤ %م0, 1    ==> should become: جمع ص٦٤ 42, 1
    int r_add = ir_builder_emit_add(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_c42, IR_TYPE_I64_T),
        ir_value_const_int(1, IR_TYPE_I64_T)
    );

    // %م2 = نسخ ص٦٤ %م1
    int r_copy1 = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_reg(r_add, IR_TYPE_I64_T));

    // %م3 = نسخ ص٦٤ %م2     (copy chain)
    int r_copy2 = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_reg(r_copy1, IR_TYPE_I64_T));

    // %م4 = طرح ص٦٤ %م3, 5   ==> should become: طرح ص٦٤ %م1, 5
    int r_sub = ir_builder_emit_sub(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_copy2, IR_TYPE_I64_T),
        ir_value_const_int(5, IR_TYPE_I64_T)
    );

    // Call with argument using a copied register:
    // %م5 = نداء @دالة_وهمية(%م0)  ==> argument should become immediate 42
    IRValue* args1[1];
    args1[0] = ir_value_reg(r_c42, IR_TYPE_I64_T);
    int r_call = ir_builder_emit_call(b, "دالة_وهمية", IR_TYPE_I64_T, args1, 1);
    (void)r_call;

    // Branch into phi diamond.
    ir_builder_emit_br_cond(b, ir_value_const_int(1, IR_TYPE_I1_T), then_bb, else_bb);

    // --------------------------------------
    // Then block: phi incoming via copy
    // --------------------------------------
    ir_builder_set_insert_point(b, then_bb);
    int r_then = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_const_int(7, IR_TYPE_I64_T));
    ir_builder_emit_br(b, merge_bb);

    // --------------------------------------
    // Else block: phi incoming via copy
    // --------------------------------------
    ir_builder_set_insert_point(b, else_bb);
    int r_else = ir_builder_emit_copy(b, IR_TYPE_I64_T, ir_value_const_int(9, IR_TYPE_I64_T));
    ir_builder_emit_br(b, merge_bb);

    // --------------------------------------
    // Merge block: phi should see propagated constants
    // --------------------------------------
    ir_builder_set_insert_point(b, merge_bb);

    int r_phi = ir_builder_emit_phi(b, IR_TYPE_I64_T);
    ir_builder_phi_add_incoming(b, r_phi, ir_value_reg(r_then, IR_TYPE_I64_T), then_bb);
    ir_builder_phi_add_incoming(b, r_phi, ir_value_reg(r_else, IR_TYPE_I64_T), else_bb);

    // Return something to keep CFG valid.
    ir_builder_emit_ret(b, ir_value_reg(r_phi, IR_TYPE_I64_T));

    // Validate preconditions
    int ok = 1;
    ok &= require(ir_module_validate_cfg(module), "CFG should be valid before copyprop");

    // Run pass
    int changed = ir_copyprop_run(module) ? 1 : 0;
    ok &= require(changed == 1, "copyprop should report changed");

    // CFG should still be valid (copyprop doesn't touch terminators).
    ok &= require(ir_module_validate_cfg(module), "CFG should be valid after copyprop");

    // Copies in entry should be removed
    ok &= require(find_inst_by_dest(entry, r_c42) == NULL, "entry: const copy should be removed");
    ok &= require(find_inst_by_dest(entry, r_copy1) == NULL, "entry: copy1 should be removed");
    ok &= require(find_inst_by_dest(entry, r_copy2) == NULL, "entry: copy2 should be removed");

    // Add should have const 42 as LHS
    IRInst* add_inst = find_inst_by_dest(entry, r_add);
    ok &= require(add_inst != NULL, "add inst should exist");
    if (add_inst) {
        ok &= require(add_inst->operand_count >= 2, "add should have 2 operands");
        ok &= require(add_inst->operands[0] && add_inst->operands[0]->kind == IR_VAL_CONST_INT, "add lhs should be const");
        ok &= require(add_inst->operands[0]->data.const_int == 42, "add lhs const should be 42");
    }

    // Sub should reference %م1 (the add result), not %م3
    IRInst* sub_inst = find_inst_by_dest(entry, r_sub);
    ok &= require(sub_inst != NULL, "sub inst should exist");
    if (sub_inst) {
        ok &= require(sub_inst->operand_count >= 2, "sub should have 2 operands");
        ok &= require(sub_inst->operands[0] && sub_inst->operands[0]->kind == IR_VAL_REG, "sub lhs should be reg");
        ok &= require(sub_inst->operands[0]->data.reg_num == r_add, "sub lhs should be r_add after chain canonicalization");
    }

    // Call argument should become immediate 42
    IRInst* call_inst = find_first_inst_by_op(entry, IR_OP_CALL);
    ok &= require(call_inst != NULL, "call inst should exist");
    if (call_inst) {
        ok &= require(call_inst->call_arg_count == 1, "call should have 1 arg");
        ok &= require(call_inst->call_args && call_inst->call_args[0], "call arg should exist");
        ok &= require(call_inst->call_args[0]->kind == IR_VAL_CONST_INT, "call arg should be const after copyprop");
        ok &= require(call_inst->call_args[0]->data.const_int == 42, "call arg const should be 42");
    }

    // Then/else copies should be removed
    ok &= require(find_inst_by_dest(then_bb, r_then) == NULL, "then: copy should be removed");
    ok &= require(find_inst_by_dest(else_bb, r_else) == NULL, "else: copy should be removed");

    // Phi incoming values should be constants 7 and 9 (order not guaranteed)
    IRInst* phi_inst = find_inst_by_dest(merge_bb, r_phi);
    ok &= require(phi_inst != NULL, "phi inst should exist");
    if (phi_inst) {
        int seen_then = 0;
        int seen_else = 0;

        for (IRPhiEntry* e = phi_inst->phi_entries; e; e = e->next) {
            if (e->block == then_bb) {
                seen_then = 1;
                ok &= require(e->value && e->value->kind == IR_VAL_CONST_INT, "phi then incoming should be const");
                if (e->value) ok &= require(e->value->data.const_int == 7, "phi then incoming const should be 7");
            } else if (e->block == else_bb) {
                seen_else = 1;
                ok &= require(e->value && e->value->kind == IR_VAL_CONST_INT, "phi else incoming should be const");
                if (e->value) ok &= require(e->value->data.const_int == 9, "phi else incoming const should be 9");
            }
        }

        ok &= require(seen_then, "phi should have then incoming");
        ok &= require(seen_else, "phi should have else incoming");
    }

    // No copies should remain anywhere in the function.
    ok &= require(!block_has_op(entry, IR_OP_COPY), "entry should not contain IR_OP_COPY after copyprop");
    ok &= require(!block_has_op(then_bb, IR_OP_COPY), "then should not contain IR_OP_COPY after copyprop");
    ok &= require(!block_has_op(else_bb, IR_OP_COPY), "else should not contain IR_OP_COPY after copyprop");
    ok &= require(!block_has_op(merge_bb, IR_OP_COPY), "merge should not contain IR_OP_COPY after copyprop");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_copyprop_test: PASS\n");
    return 0;
}