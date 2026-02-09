/**
 * @file ir_mem2reg_phi_test.c
 * @brief اختبار إدراج فاي + إعادة التسمية في SSA داخل Mem2Reg (v0.3.2.5.2).
 *
 * نبني مخطط تدفق تحكم بسيط:
 * - بداية -> (ثم/وإلا) -> دمج
 * - يتم تعريف نفس المتغير (alloca) في فرعين مختلفين ثم يُستخدم بعد الدمج
 *
 * التوقعات:
 * - إدراج `فاي` في كتلة الدمج
 * - وجود مدخلين (incoming) من الكتلتين السابقتين بالقيم الصحيحة
 * - تحويل `حمل` في الدمج إلى `نسخ` من سجل الفاي
 * - حذف `حجز/خزن` المرتبطة بالمتغير المرقّى
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
        if (inst->operands[1] && inst->operands[1]->kind == IR_VAL_REG &&
            inst->operands[1]->data.reg_num == ptr_reg) {
            return 1;
        }
    }
    return 0;
}

static IRInst* find_first_phi(IRBlock* block) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == IR_OP_PHI) return inst;
        // الفاي يجب أن تكون في الأعلى؛ إذا وصلنا لتعليمة غير فاي مبكراً فانهِ
        if (inst->op != IR_OP_PHI) break;
    }
    return NULL;
}

static int phi_has_incoming_const(IRInst* phi, IRBlock* pred, int64_t value) {
    if (!phi || phi->op != IR_OP_PHI || !pred) return 0;
    for (IRPhiEntry* e = phi->phi_entries; e; e = e->next) {
        if (e->block != pred) continue;
        if (!e->value) return 0;
        if (e->value->kind != IR_VAL_CONST_INT) return 0;
        return (e->value->data.const_int == value) ? 1 : 0;
    }
    return 0;
}

int main(void) {
    IRModule* module = ir_module_new("mem2reg_phi_test");
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

    (void)ir_builder_create_func(b, "الرئيسية", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "بداية");
    IRBlock* then_bb = ir_builder_create_block(b, "ثم");
    IRBlock* else_bb = ir_builder_create_block(b, "وإلا");
    IRBlock* merge = ir_builder_create_block(b, "دمج");

    // البداية
    ir_builder_set_insert_point(b, entry);
    int r_ptr = ir_builder_emit_alloca(b, IR_TYPE_I64_T);

    // خزن ٠، %ptr  (تهيئة)
    ir_builder_emit_store(
        b,
        ir_value_const_int(0, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    // فرع شرط ثابت
    IRValue* cond = ir_value_const_int(1, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cond, then_bb, else_bb);

    // ثم
    ir_builder_set_insert_point(b, then_bb);
    ir_builder_emit_store(
        b,
        ir_value_const_int(1, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );
    ir_builder_emit_br(b, merge);

    // وإلا
    ir_builder_set_insert_point(b, else_bb);
    ir_builder_emit_store(
        b,
        ir_value_const_int(2, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );
    ir_builder_emit_br(b, merge);

    // دمج
    ir_builder_set_insert_point(b, merge);
    int r_x = ir_builder_emit_load(
        b,
        IR_TYPE_I64_T,
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );
    ir_builder_emit_ret(b, ir_value_reg(r_x, IR_TYPE_I64_T));

    int changed = ir_mem2reg_run(module) ? 1 : 0;

    int ok = 1;
    ok &= require(changed == 1, "mem2reg should report changed");

    // alloca يجب أن يُحذف
    ok &= require(find_inst_by_dest(entry, r_ptr) == NULL, "entry: alloca should be removed");

    // لا يجب أن تبقى خزن مرتبطة بالمؤشر
    ok &= require(!block_has_store_to_ptr_reg(entry, r_ptr), "entry: stores should be removed");
    ok &= require(!block_has_store_to_ptr_reg(then_bb, r_ptr), "then: stores should be removed");
    ok &= require(!block_has_store_to_ptr_reg(else_bb, r_ptr), "else: stores should be removed");

    // يجب أن توجد فاي في الدمج
    IRInst* phi = find_first_phi(merge);
    ok &= require(phi != NULL, "merge: phi should exist");

    if (phi) {
        ok &= require(phi->dest >= 0, "merge: phi should define a dest reg");
        ok &= require(phi_has_incoming_const(phi, then_bb, 1), "merge: phi should have incoming 1 from then");
        ok &= require(phi_has_incoming_const(phi, else_bb, 2), "merge: phi should have incoming 2 from else");
    }

    // الحمل في الدمج يجب أن يصبح نسخ من سجل الفاي
    IRInst* x_inst = find_inst_by_dest(merge, r_x);
    ok &= require(x_inst != NULL, "merge: r_x inst should exist");
    if (x_inst && phi) {
        ok &= require(x_inst->op == IR_OP_COPY, "merge: load should become copy");
        ok &= require(x_inst->operand_count == 1, "merge: copy should have 1 operand");
        ok &= require(x_inst->operands[0] && x_inst->operands[0]->kind == IR_VAL_REG, "merge: copy src should be reg");
        if (x_inst->operands[0]) {
            ok &= require(x_inst->operands[0]->data.reg_num == phi->dest, "merge: copy src should be phi dest");
        }
    }

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_mem2reg_phi_test: PASS\n");
    return 0;
}
