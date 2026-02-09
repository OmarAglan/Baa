/**
 * @file ir_outssa_test.c
 * @brief اختبار تمريرة الخروج من SSA (إزالة فاي قبل الخلفية) (v0.3.2.5.2).
 *
 * نبني مخطط تدفق تحكم يجبر تقسيم حافة حرجة:
 * - بداية: br_cond إلى (دمج) أو (وإلا)
 * - وإلا: يعرّف المتغير ثم يقفز إلى دمج
 * - دمج: يستخدم المتغير (يؤدي إلى إدراج فاي بعد Mem2Reg)
 *
 * التوقعات بعد الخروج من SSA:
 * - عدم وجود أي IR_OP_PHI
 * - إنشاء كتلة وسيطة (split) على الحافة بداية -> دمج
 * - إدراج نسخ إلى سجل الفاي في الكتلة الوسيطة و/أو الكتل السابقة
 */

#include <stdio.h>
#include <stdlib.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_mem2reg.h"
#include "../src/ir_outssa.h"
#include "../src/ir_analysis.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static IRInst* find_first_phi(IRBlock* block) {
    if (!block) return NULL;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == IR_OP_PHI) return inst;
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
        return e->value->data.const_int == value;
    }
    return 0;
}

static int block_has_phi(IRBlock* block) {
    return find_first_phi(block) != NULL;
}

static int block_has_copy_to_reg_from_const(IRBlock* block, int dest_reg, int64_t value) {
    if (!block) return 0;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op != IR_OP_COPY) continue;
        if (inst->dest != dest_reg) continue;
        if (inst->operand_count < 1) continue;
        if (!inst->operands[0]) continue;
        if (inst->operands[0]->kind != IR_VAL_CONST_INT) continue;
        return inst->operands[0]->data.const_int == value;
    }
    return 0;
}

static int term_targets_block(IRInst* term, IRBlock* target) {
    if (!term || !target) return 0;

    if (term->op == IR_OP_BR) {
        if (term->operand_count < 1) return 0;
        IRValue* v = term->operands[0];
        return v && v->kind == IR_VAL_BLOCK && v->data.block == target;
    }

    if (term->op == IR_OP_BR_COND) {
        if (term->operand_count < 3) return 0;
        for (int i = 1; i <= 2; i++) {
            IRValue* v = term->operands[i];
            if (v && v->kind == IR_VAL_BLOCK && v->data.block == target) return 1;
        }
    }

    return 0;
}

static IRBlock* br_cond_other_target(IRInst* term, IRBlock* known) {
    if (!term || term->op != IR_OP_BR_COND || term->operand_count < 3 || !known) return NULL;

    IRValue* v1 = term->operands[1];
    IRValue* v2 = term->operands[2];
    if (!v1 || !v2) return NULL;
    if (v1->kind != IR_VAL_BLOCK || v2->kind != IR_VAL_BLOCK) return NULL;

    IRBlock* b1 = v1->data.block;
    IRBlock* b2 = v2->data.block;

    if (b1 == known) return b2;
    if (b2 == known) return b1;
    return NULL;
}

int main(void) {
    IRModule* module = ir_module_new("outssa_test");
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
    IRBlock* else_bb = ir_builder_create_block(b, "وإلا");
    IRBlock* merge = ir_builder_create_block(b, "دمج");

    // البداية
    ir_builder_set_insert_point(b, entry);
    int r_ptr = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    ir_builder_emit_store(
        b,
        ir_value_const_int(0, IR_TYPE_I64_T),
        ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T))
    );

    IRValue* cond = ir_value_const_int(1, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cond, merge, else_bb);

    // وإلا
    ir_builder_set_insert_point(b, else_bb);
    ir_builder_emit_store(
        b,
        ir_value_const_int(1, IR_TYPE_I64_T),
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

    // 1) Mem2Reg: يجب أن يدرج فاي
    (void)ir_mem2reg_run(module);

    IRFunc* func = module->funcs;
    int ok = 1;
    ok &= require(func != NULL, "func should exist");

    IRInst* phi = find_first_phi(merge);
    ok &= require(phi != NULL, "merge: phi should exist after mem2reg");
    if (!phi) {
        ir_builder_free(b);
        ir_module_free(module);
        return 1;
    }

    int phi_dest = phi->dest;
    ok &= require(phi_dest >= 0, "phi dest should be valid");
    ok &= require(phi_has_incoming_const(phi, entry, 0), "phi should have incoming 0 from entry");
    ok &= require(phi_has_incoming_const(phi, else_bb, 1), "phi should have incoming 1 from else");

    // 2) الخروج من SSA: يجب أن يزيل فاي ويقسم الحافة الحرجة
    int changed = ir_outssa_run(module) ? 1 : 0;
    ok &= require(changed == 1, "outssa should report changed");

    // إعادة بناء preds للتأكد من التماسك
    ir_func_rebuild_preds(func);
    ok &= require(ir_func_validate_cfg(func), "cfg should remain valid after outssa");

    ok &= require(!block_has_phi(merge), "merge: no phi should remain after outssa");

    // يجب أن لا يستهدف entry كتلة الدمج مباشرة بعد التقسيم (على الأقل أحد المسارين)
    ok &= require(entry->last && entry->last->op == IR_OP_BR_COND, "entry: terminator should be br_cond");
    ok &= require(!term_targets_block(entry->last, merge), "entry: should not branch directly to merge after split");

    IRBlock* split = br_cond_other_target(entry->last, else_bb);
    ok &= require(split != NULL, "split block should exist");

    // تحقق من النسخ على الحافة: split يكتب قيمة 0
    if (split) {
        ok &= require(split != merge, "split: should not be merge");
        ok &= require(split != else_bb, "split: should not be else");
        ok &= require(split->last && split->last->op == IR_OP_BR, "split: should end with br");
        ok &= require(term_targets_block(split->last, merge), "split: should branch to merge");
        ok &= require(block_has_copy_to_reg_from_const(split, phi_dest, 0), "split: should copy 0 into phi dest");
    }

    // وإلا يجب أن يكتب قيمة 1 إلى سجل الفاي
    ok &= require(block_has_copy_to_reg_from_const(else_bb, phi_dest, 1), "else: should copy 1 into phi dest");

    ir_builder_free(b);
    ir_module_free(module);

    if (!ok) return 1;

    fprintf(stderr, "ir_outssa_test: PASS\n");
    return 0;
}
