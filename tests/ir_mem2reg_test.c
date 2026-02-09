/**
 * @file ir_mem2reg_test.c
 * @brief اختبارات تمريرة Mem2Reg (ترقية الذاكرة إلى سجلات) — SSA (v0.3.2.5.2).
 *
 * نتحقق من:
 * - ترقية `حجز/خزن/حمل` إلى استعمال سجلات عبر `نسخ` (حتى عبر كتل متعددة عند وجود dominance).
 * - حذف تعليمات `خزن` و `حجز` للـ alloca المُرقّى.
 * - المحافظة: إذا هرب المؤشر (تم تمريره لـ نداء)، لا تتم الترقية.
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

static IRInst* find_inst_by_dest(IRBlock* block, int dest) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->dest == dest) return inst;
    }
    return NULL;
}

static int block_has_store_to_ptr_reg(IRBlock* block, int ptr_reg) {
    if (!block) return 0;

    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op != IR_OP_STORE) continue;
        if (inst->operand_count < 2) continue;

        IRValue* ptr = inst->operands[1];
        if (ptr && ptr->kind == IR_VAL_REG && ptr->data.reg_num == ptr_reg) {
            return 1;
        }
    }

    return 0;
}

int main(void) {
    IRModule* module = ir_module_new("mem2reg_test");
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

    IRBlock* entry = ir_builder_create_block(b, "بداية");
    IRBlock* bb2 = ir_builder_create_block(b, "كتلة_٢");

    // -------------------------------------------------------------------------
    // حالة إيجابية (١): alloca داخل نفس الكتلة (يجب أن تتم ترقيته)
    // -------------------------------------------------------------------------
    ir_builder_set_insert_point(b, entry);

    int r_ptr = ir_builder_emit_alloca(b, IR_TYPE_I64_T);

    // خزن ٥، %ptr
    ir_builder_emit_store(
        b,
        ir_value_const_int(5, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // %a = حمل %ptr
    int r_a = ir_builder_emit_load(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // %b = جمع %a، ١
    int r_b = ir_builder_emit_add(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_a, IR_TYPE_I64_T),
        ir_value_const_int(1, IR_TYPE_I64_T)
    );

    // خزن %b، %ptr
    ir_builder_emit_store(
        b,
        ir_value_reg(r_b, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // %c = حمل %ptr  (يجب أن يصبح نسخ من %b)
    int r_c = ir_builder_emit_load(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // -------------------------------------------------------------------------
    // حالة إيجابية (٢): alloca يُستخدم عبر كتل متعددة (يجب أن تتم ترقيته)
    // -------------------------------------------------------------------------
    int r_ptr2 = ir_builder_emit_alloca(b, IR_TYPE_I64_T);

    ir_builder_emit_store(
        b,
        ir_value_const_int(11, IR_TYPE_I64_T),
        ir_value_reg(r_ptr2, ir_type_ptr(IR_TYPE_I64_T))
    );

    // -------------------------------------------------------------------------
    // حالة سلبية/محافظة: هروب المؤشر عبر نداء (لا تتم الترقية)
    // -------------------------------------------------------------------------
    int r_ptr3 = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    ir_builder_emit_store(
        b,
        ir_value_const_int(7, IR_TYPE_I64_T),
        ir_value_reg(r_ptr3, ir_type_ptr(IR_TYPE_I64_T))
    );

    IRValue* args1[1];
    args1[0] = ir_value_reg(r_ptr3, ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_call_void(b, "اطبع", args1, 1);

    int r_y = ir_builder_emit_load(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_ptr3, ir_type_ptr(IR_TYPE_I64_T))
    );
    (void)r_y;

    // انتقال إلى كتلة ثانية تستخدم المؤشر
    ir_builder_emit_br(b, bb2);

    ir_builder_set_insert_point(b, bb2);

    int r_x = ir_builder_emit_load(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_ptr2, ir_type_ptr(IR_TYPE_I64_T))
    );

    ir_builder_emit_ret(b, ir_value_reg(r_x, IR_TYPE_I64_T));

    // -------------------------------------------------------------------------
    // تشغيل التمريرة
    // -------------------------------------------------------------------------
    int changed = ir_mem2reg_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "mem2reg should report changed (it should promote r_ptr/r_ptr2)");

    // -------------------------------------------------------------------------
    // تحقق: المسار الأول تمت ترقيته
    // -------------------------------------------------------------------------

    // alloca الأول يجب أن يُحذف
    ok &= require(find_inst_by_dest(entry, r_ptr) == NULL, "entry: promotable alloca should be removed");

    // لا يجب أن تبقى أي خزن مرتبطة بالمؤشر المرقّى (r_ptr).
    // ملاحظة: قد تبقى تعليمات خزن أخرى لنفس الكتلة تخص alloca غير قابل للترقية (r_ptr2).
    ok &= require(!block_has_store_to_ptr_reg(entry, r_ptr), "entry: stores to promoted alloca should be removed");

    // %a يجب أن يصبح نسخ من ٥
    IRInst* a_inst = find_inst_by_dest(entry, r_a);
    ok &= require(a_inst != NULL, "entry: r_a inst should exist");
    if (a_inst) {
        ok &= require(a_inst->op == IR_OP_COPY, "entry: r_a should become IR_OP_COPY");
        ok &= require(a_inst->operand_count == 1, "entry: copy should have 1 operand");
        ok &= require(a_inst->operands[0] && a_inst->operands[0]->kind == IR_VAL_CONST_INT, "entry: r_a copy src should be const");
        if (a_inst->operands[0]) {
            ok &= require(a_inst->operands[0]->data.const_int == 5, "entry: r_a copy src const should be 5");
        }
    }

    // %c يجب أن يصبح نسخ من %b
    IRInst* c_inst = find_inst_by_dest(entry, r_c);
    ok &= require(c_inst != NULL, "entry: r_c inst should exist");
    if (c_inst) {
        ok &= require(c_inst->op == IR_OP_COPY, "entry: r_c should become IR_OP_COPY");
        ok &= require(c_inst->operand_count == 1, "entry: copy should have 1 operand");
        ok &= require(c_inst->operands[0] && c_inst->operands[0]->kind == IR_VAL_REG, "entry: r_c copy src should be reg");
        if (c_inst->operands[0]) {
            ok &= require(c_inst->operands[0]->data.reg_num == r_b, "entry: r_c copy src should be r_b");
        }
    }

    // -------------------------------------------------------------------------
    // تحقق: المسار الثاني تمت ترقيته (عبر كتل متعددة)
    // -------------------------------------------------------------------------
    ok &= require(find_inst_by_dest(entry, r_ptr2) == NULL, "entry: cross-block alloca should be removed");
    ok &= require(!block_has_store_to_ptr_reg(entry, r_ptr2), "entry: stores to cross-block promoted alloca should be removed");

    // الحمل في كتلة ثانية يجب أن يصبح نسخ من ١١
    IRInst* x_inst = find_inst_by_dest(bb2, r_x);
    ok &= require(x_inst != NULL, "bb2: r_x inst should exist");
    if (x_inst) {
        ok &= require(x_inst->op == IR_OP_COPY, "bb2: cross-block load should become IR_OP_COPY");
        ok &= require(x_inst->operand_count == 1, "bb2: copy should have 1 operand");
        ok &= require(x_inst->operands[0] && x_inst->operands[0]->kind == IR_VAL_CONST_INT, "bb2: copy src should be const");
        if (x_inst->operands[0]) {
            ok &= require(x_inst->operands[0]->data.const_int == 11, "bb2: copy src const should be 11");
        }
    }

    // -------------------------------------------------------------------------
    // تحقق: المسار الثالث لم تتم ترقيته (هروب المؤشر)
    // -------------------------------------------------------------------------
    ok &= require(find_inst_by_dest(entry, r_ptr3) != NULL, "entry: escaping alloca should remain");
    ok &= require(block_has_store_to_ptr_reg(entry, r_ptr3), "entry: store to escaping alloca should remain");
    IRInst* y_inst = find_inst_by_dest(entry, r_y);
    ok &= require(y_inst != NULL, "entry: r_y inst should exist");
    if (y_inst) {
        ok &= require(y_inst->op == IR_OP_LOAD, "entry: load from escaping alloca should remain IR_OP_LOAD");
    }

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_mem2reg_test: PASS\n");
    return 0;
}
