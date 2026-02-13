/**
 * @file ir_verify_ir_test.c
 * @brief اختبارات متحقق سلامة الـ IR + تمريرة التوحيد + تبسيط CFG (v0.3.2.6.5).
 *
 * نختبر:
 * 1) `ir_module_verify_ir()` يمر على IR صحيح ويُسقط IR خاطئ (مثل: store pointer type mismatch).
 * 2) `ir_canon_run()` توحّد:
 *    - العمليات التبادلية: وضع الثابت على اليمين وترتيب السجلات
 *    - المقارنات: قلب المقارنة إذا كان الثابت على اليسار
 * 3) `ir_cfg_simplify_run()`:
 *    - يحوّل `قفز_شرط cond, X, X` إلى `قفز X`
 *    - يزيل كتلة تافهة (trivial br-only) مع تحديث preds/succs
 *
 * على النجاح: يطبع `ir_verify_ir_test: PASS` إلى stderr ويعيد 0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/ir_analysis.h"
#include "../src/ir_verify_ir.h"
#include "../src/ir_canon.h"
#include "../src/ir_cfg_simplify.h"

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

static IRModule* build_good_module(void) {
    IRModule* m = ir_module_new("verify_ir_good");
    if (!m) return NULL;

    IRBuilder* b = ir_builder_new(m);
    if (!b) {
        ir_module_free(m);
        return NULL;
    }

    (void)ir_builder_create_func(b, "الرئيسية", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    ir_builder_set_insert_point(b, entry);

    int ptr = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    ir_builder_emit_store(b,
                          ir_value_const_int(7, IR_TYPE_I64_T),
                          ir_value_reg(ptr, ir_type_ptr(IR_TYPE_I64_T)));

    int v = ir_builder_emit_load(b, IR_TYPE_I64_T,
                                 ir_value_reg(ptr, ir_type_ptr(IR_TYPE_I64_T)));

    ir_builder_emit_ret(b, ir_value_reg(v, IR_TYPE_I64_T));

    ir_builder_free(b);
    return m;
}

static IRModule* build_bad_module_store_type_mismatch(void) {
    IRModule* m = ir_module_new("verify_ir_bad_store");
    if (!m) return NULL;

    // نبني يدوياً للتأكد أننا نخلق عدم تطابق type: خزن i64 إلى ptr<i32>
    IRFunc* f = ir_func_new("اختبار_خزن_نوع_خاطئ", IR_TYPE_I64_T);
    if (!f) {
        ir_module_free(m);
        return NULL;
    }

    IRBlock* entry = ir_func_new_block(f, "بداية");
    if (!entry) {
        ir_module_free(m);
        return NULL;
    }

    // نُعلن أننا سنستخدم %م0 داخل هذه الدالة
    f->next_reg = 1;

    // %م0 = alloca i32  (type = ptr<i32>)
    IRInst* alloca32 = ir_inst_alloca(IR_TYPE_I32_T, 0);
    if (alloca32) ir_block_append(entry, alloca32);

    // store i64 1 -> ptr<i32>  (مخالفة)
    IRValue* bad_ptr = ir_value_reg(0, ir_type_ptr(IR_TYPE_I32_T));
    IRInst* store_bad = ir_inst_store(ir_value_const_int(1, IR_TYPE_I64_T), bad_ptr);
    if (store_bad) ir_block_append(entry, store_bad);

    IRInst* ret = ir_inst_ret(ir_value_const_int(0, IR_TYPE_I64_T));
    if (ret) ir_block_append(entry, ret);

    ir_module_add_func(m, f);
    return m;
}

static int test_verify_ir(void) {
    int ok = 1;

    IRModule* good = build_good_module();
    ok &= require(good != NULL, "build_good_module failed");
    if (good) {
        ok &= require(ir_module_verify_ir(good, stderr), "verify-ir should accept good module");
        ir_module_free(good);
    }

    IRModule* bad = build_bad_module_store_type_mismatch();
    ok &= require(bad != NULL, "build_bad_module_store_type_mismatch failed");
    if (bad) {
        ok &= require(!ir_module_verify_ir(bad, stderr), "verify-ir should reject bad store type mismatch");
        ir_module_free(bad);
    }

    return ok;
}

static int test_canonicalization(void) {
    int ok = 1;

    IRModule* m = ir_module_new("canon_test");
    ok &= require(m != NULL, "module alloc failed");
    if (!m) return 0;

    IRBuilder* b = ir_builder_new(m);
    ok &= require(b != NULL, "builder alloc failed");
    if (!b) {
        ir_module_free(m);
        return 0;
    }

    (void)ir_builder_create_func(b, "الرئيسية", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    ir_builder_set_insert_point(b, entry);

    // %a = جمع i64 5, %x  => يجب أن يصبح %a = جمع i64 %x, 5
    int x = ir_builder_emit_add(b, IR_TYPE_I64_T,
                               ir_value_const_int(1, IR_TYPE_I64_T),
                               ir_value_const_int(2, IR_TYPE_I64_T));

    int a = ir_builder_emit_add(b, IR_TYPE_I64_T,
                               ir_value_const_int(5, IR_TYPE_I64_T),
                               ir_value_reg(x, IR_TYPE_I64_T));

    // %c = قارن أكبر 5, %x  => يجب أن يصبح قارن أصغر %x, 5
    int c = ir_builder_emit_cmp_gt(b,
                                  ir_value_const_int(5, IR_TYPE_I64_T),
                                  ir_value_reg(x, IR_TYPE_I64_T));

    ir_builder_emit_ret(b, ir_value_reg(a, IR_TYPE_I64_T));

    int changed = ir_canon_run(m) ? 1 : 0;
    ok &= require(changed == 1, "canonicalization should report changed");

    IRInst* add_inst = find_inst_by_dest(entry, a);
    ok &= require(add_inst != NULL, "add inst should exist");
    if (add_inst) {
        ok &= require(add_inst->operand_count == 2, "add operand count");
        ok &= require(add_inst->operands[0] && add_inst->operands[0]->kind == IR_VAL_REG, "add lhs should be reg");
        ok &= require(add_inst->operands[1] && add_inst->operands[1]->kind == IR_VAL_CONST_INT, "add rhs should be const");
    }

    IRInst* cmp_inst = find_inst_by_dest(entry, c);
    ok &= require(cmp_inst != NULL, "cmp inst should exist");
    if (cmp_inst) {
        ok &= require(cmp_inst->op == IR_OP_CMP, "cmp op");
        ok &= require(cmp_inst->operand_count == 2, "cmp operand count");
        ok &= require(cmp_inst->operands[0] && cmp_inst->operands[0]->kind == IR_VAL_REG, "cmp lhs should become reg");
        ok &= require(cmp_inst->operands[1] && cmp_inst->operands[1]->kind == IR_VAL_CONST_INT, "cmp rhs should be const");
        ok &= require(cmp_inst->cmp_pred == IR_CMP_LT, "cmp predicate should swap GT -> LT");
    }

    // sanity: still well-formed
    ok &= require(ir_module_verify_ir(m, stderr), "verify-ir should accept canonicalized module");

    ir_builder_free(b);
    ir_module_free(m);
    return ok;
}

static int test_cfg_simplify(void) {
    int ok = 1;

    IRModule* m = ir_module_new("cfg_simplify_test");
    ok &= require(m != NULL, "module alloc failed");
    if (!m) return 0;

    IRBuilder* b = ir_builder_new(m);
    ok &= require(b != NULL, "builder alloc failed");
    if (!b) {
        ir_module_free(m);
        return 0;
    }

    IRFunc* f = ir_builder_create_func(b, "الرئيسية", IR_TYPE_I64_T);
    ok &= require(f != NULL, "create func failed");

    IRBlock* entry = ir_builder_create_block(b, "بداية");
    IRBlock* mid = ir_builder_create_block(b, "وسط");
    IRBlock* target = ir_builder_create_block(b, "هدف");
    ok &= require(entry && mid && target, "blocks alloc failed");

    // entry: br_cond (cond=true) -> mid, mid  (redundant)
    ir_builder_set_insert_point(b, entry);
    IRValue* cond = ir_value_const_int(1, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cond, mid, mid);

    // mid: br target (trivial block)
    ir_builder_set_insert_point(b, mid);
    ir_builder_emit_br(b, target);

    // target: ret 0
    ir_builder_set_insert_point(b, target);
    ir_builder_emit_ret(b, ir_value_const_int(0, IR_TYPE_I64_T));

    ir_func_rebuild_preds(f);
    ok &= require(ir_func_validate_cfg(f), "cfg should be valid before simplify");

    int changed = ir_cfg_simplify_run(m) ? 1 : 0;
    ok &= require(changed == 1, "cfg simplify should report changed");

    ir_func_rebuild_preds(f);
    ok &= require(ir_func_validate_cfg(f), "cfg should be valid after simplify");

    // entry terminator should now be BR
    ok &= require(entry->last && entry->last->op == IR_OP_BR, "entry should end with BR after simplify");

    // mid should be removed
    ok &= require(find_block_by_label(f, "وسط") == NULL, "mid trivial block should be removed");

    // entry must branch to target now
    IRInst* term = entry->last;
    ok &= require(term && term->operand_count >= 1, "entry term operand_count");
    if (term && term->operand_count >= 1) {
        ok &= require(term->operands[0] && term->operands[0]->kind == IR_VAL_BLOCK, "entry br target is block");
        ok &= require(term->operands[0]->data.block == target, "entry should branch to target");
    }

    ok &= require(ir_module_verify_ir(m, stderr), "verify-ir should accept simplified module");

    ir_builder_free(b);
    ir_module_free(m);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_verify_ir();
    ok &= test_canonicalization();
    ok &= test_cfg_simplify();

    if (!ok) return 1;

    fprintf(stderr, "ir_verify_ir_test: PASS\n");
    return 0;
}
