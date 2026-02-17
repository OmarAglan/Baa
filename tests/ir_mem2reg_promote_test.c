/**
 * @file ir_mem2reg_promote_test.c
 * @brief اختبار ترقية alloca مع تهيئة في كتلة مختلفة (must-def) — v0.3.2.8.6.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_mem2reg.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static int block_has_op(IRBlock* b, IROp op) {
    if (!b) return 0;
    for (IRInst* i = b->first; i; i = i->next) {
        if (i->op == op) return 1;
    }
    return 0;
}

static IRInst* find_inst_by_dest(IRBlock* block, int dest) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->dest == dest) return inst;
    }
    return NULL;
}

int main(void) {
    IRModule* module = ir_module_new("mem2reg_promote_test");
    if (!module) return 1;

    IRBuilder* b = ir_builder_new(module);
    if (!b) {
        ir_module_free(module);
        return 1;
    }

    (void)ir_builder_create_func(b, "f", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "entry");
    IRBlock* init = ir_builder_create_block(b, "init");
    IRBlock* use  = ir_builder_create_block(b, "use");

    ir_builder_set_insert_point(b, entry);
    int ptr = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    ir_builder_emit_br(b, init);

    ir_builder_set_insert_point(b, init);
    IRValue* ptrv_init = ir_value_reg(ptr, ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_store(b, ir_value_const_int(7, IR_TYPE_I64_T), ptrv_init);
    ir_builder_emit_br(b, use);

    ir_builder_set_insert_point(b, use);
    IRValue* ptrv_use = ir_value_reg(ptr, ir_type_ptr(IR_TYPE_I64_T));
    int v = ir_builder_emit_load(b, IR_TYPE_I64_T, ptrv_use);
    ir_builder_emit_ret(b, ir_value_reg(v, IR_TYPE_I64_T));

    int changed = ir_mem2reg_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "mem2reg should report changed");
    ok &= require(!block_has_op(init, IR_OP_STORE), "store should be removed");

    IRInst* vi = find_inst_by_dest(use, v);
    ok &= require(vi != NULL, "load destination inst should exist");
    ok &= require(vi && vi->op == IR_OP_COPY, "load should become copy");
    ok &= require(vi && vi->operands[0] && vi->operands[0]->kind == IR_VAL_CONST_INT, "copy src should be const");
    ok &= require(vi && vi->operands[0] && vi->operands[0]->data.const_int == 7, "copy src should be 7");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;
    fprintf(stderr, "ir_mem2reg_promote_test: PASS\n");
    return 0;
}
