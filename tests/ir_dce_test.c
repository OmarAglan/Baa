/**
 * @file ir_dce_test.c
 * @brief IR dead code elimination tests (v0.3.1.3).
 *
 * Verifies:
 * - Dead SSA instructions (unused dest) are removed if they have no side effects.
 * - Cascading DCE works (if B is removed, A may become dead too).
 * - Unreachable basic blocks are removed.
 * - Calls/stores/terminators are preserved (conservative side effects).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_analysis.h"
#include "../src/ir_dce.h"

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

static IRBlock* find_block_by_label(IRFunc* func, const char* label) {
    if (!func || !label) return NULL;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->label && strcmp(b->label, label) == 0) return b;
    }
    return NULL;
}

int main(void) {
    IRModule* module = ir_module_new("dce_test");
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

    // Entry block
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    ir_builder_set_insert_point(b, entry);

    // Dead chain:
    //   %r1 = جمع ص٦٤ 1, 2
    //   %r2 = طرح ص٦٤ %r1, 1
    // Neither result is used => both should be removed (cascade).
    int r1 = ir_builder_emit_add(
        b,
        IR_TYPE_I64_T,
        ir_value_const_int(1, IR_TYPE_I64_T),
        ir_value_const_int(2, IR_TYPE_I64_T)
    );

    int r2 = ir_builder_emit_sub(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r1, IR_TYPE_I64_T),
        ir_value_const_int(1, IR_TYPE_I64_T)
    );

    // Side-effecting call (conservatively kept even if unused)
    int r_call = ir_builder_emit_call(b, "دالة_وهمية", IR_TYPE_I64_T, NULL, 0);
    (void)r_call;

    // Alloca + store (store is side-effecting and must remain)
    int r_ptr = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    ir_builder_emit_store(
        b,
        ir_value_const_int(42, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // Dead load (unused) should be removed
    int r_load = ir_builder_emit_load(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // Terminator
    ir_builder_emit_ret(b, ir_value_const_int(0, IR_TYPE_I64_T));

    // Unreachable block: not referenced from any terminator
    IRBlock* unreachable = ir_builder_create_block(b, "غير_قابل_للوصول");
    ir_builder_set_insert_point(b, unreachable);

    // Put something inside (must be removed when block is deleted)
    (void)ir_builder_emit_add(
        b,
        IR_TYPE_I64_T,
        ir_value_const_int(9, IR_TYPE_I64_T),
        ir_value_const_int(9, IR_TYPE_I64_T)
    );
    ir_builder_emit_ret(b, ir_value_const_int(1, IR_TYPE_I64_T));

    // Validate preconditions
    int ok = 1;
    ok &= require(ir_module_validate_cfg(module), "CFG should be valid before DCE");

    // Run pass
    int changed = ir_dce_run(module) ? 1 : 0;
    ok &= require(changed == 1, "DCE should report changed");

    // Re-validate CFG after transformation
    ok &= require(ir_module_validate_cfg(module), "CFG should be valid after DCE");

    IRFunc* main_func = ir_module_find_func(module, "الرئيسية");
    ok &= require(main_func != NULL, "Failed to find function @الرئيسية");
    ok &= require(main_func && main_func->entry != NULL, "Function entry should exist");

    // Unreachable block should be gone
    ok &= require(find_block_by_label(main_func, "غير_قابل_للوصول") == NULL, "Unreachable block should be removed");

    // Dead chain should be removed
    ok &= require(find_inst_by_dest(entry, r2) == NULL, "Dead sub should be removed");
    ok &= require(find_inst_by_dest(entry, r1) == NULL, "Cascade dead add should be removed");

    // Dead load should be removed
    ok &= require(find_inst_by_dest(entry, r_load) == NULL, "Dead load should be removed");

    // Call should remain
    ok &= require(find_inst_by_dest(entry, r_call) != NULL, "Call should be preserved (side effects)");

    // Store should remain
    ok &= require(find_first_inst_by_op(entry, IR_OP_STORE) != NULL, "Store should be preserved (side effects)");

    // Ret should remain
    ok &= require(find_first_inst_by_op(entry, IR_OP_RET) != NULL, "Ret should remain");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_dce_test: PASS\n");
    return 0;
}