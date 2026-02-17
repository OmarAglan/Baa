/**
 * @file ir_sccp_test.c
 * @brief اختبارات SCCP — v0.3.2.8.6.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_sccp.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void) {
    IRModule* module = ir_module_new("sccp_test");
    if (!module) return 1;

    IRBuilder* b = ir_builder_new(module);
    if (!b) {
        ir_module_free(module);
        return 1;
    }

    (void)ir_builder_create_func(b, "f", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "entry");
    IRBlock* bt = ir_builder_create_block(b, "t");
    IRBlock* bf = ir_builder_create_block(b, "f");

    ir_builder_set_insert_point(b, entry);
    IRValue* cond = ir_value_const_int(1, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cond, bt, bf);

    ir_builder_set_insert_point(b, bt);
    ir_builder_emit_ret_int(b, 7);

    ir_builder_set_insert_point(b, bf);
    ir_builder_emit_ret_int(b, 9);

    int changed = ir_sccp_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "SCCP should report changed");
    ok &= require(entry->last != NULL, "entry must have terminator");
    ok &= require(entry->last && entry->last->op == IR_OP_BR, "entry br_cond should become br");
    ok &= require(entry->last && entry->last->operand_count >= 1, "br must have target");
    ok &= require(entry->last && entry->last->operands[0] && entry->last->operands[0]->kind == IR_VAL_BLOCK,
                  "br target must be block");
    ok &= require(entry->last && entry->last->operands[0]->data.block == bt, "br should target true block");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;
    fprintf(stderr, "ir_sccp_test: PASS\n");
    return 0;
}
