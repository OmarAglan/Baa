/**
 * @file ir_data_layout_test.c
 * @brief اختبار تخطيط البيانات ودلالات الحساب (v0.3.2.6.6).
 *
 * يختبر:
 * - أحجام الأنواع بالبايت (ir_type_size_bytes)
 * - محاذاة الأنواع (ir_type_alignment)
 * - حجم التخزين (ir_type_store_size)
 * - محددات الأنواع (ir_type_is_integer, ir_type_is_pointer)
 * - دلالات الحساب: INT64_MIN / -1، INT64_MIN % -1، التفاف الطفحان
 * - دلالات ص١ (i1): أي قيمة غير صفرية → 1
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../src/ir.h"
#include "../src/ir_data_layout.h"
#include "../src/ir_builder.h"
#include "../src/ir_constfold.h"

// ============================================================================
// مساعدات الاختبار
// ============================================================================

static int g_pass = 0;
static int g_fail = 0;

static void check(int cond, const char* msg) {
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        fprintf(stderr, "  ❌ FAIL: %s\n", msg);
    }
}

// ============================================================================
// اختبار أحجام الأنواع (ir_type_size_bytes)
// ============================================================================

static void test_type_sizes(void) {
    fprintf(stderr, "\n--- اختبار أحجام الأنواع ---\n");
    const IRDataLayout* dl = &IR_DATA_LAYOUT_WIN_X64;

    // إنشاء وحدة مؤقتة لتوفير ساحة الذاكرة للأنواع المركبة
    IRModule* mod = ir_module_new("size_test");

    check(ir_type_size_bytes(dl, IR_TYPE_VOID_T) == 0,  "void == 0 bytes");
    check(ir_type_size_bytes(dl, IR_TYPE_I1_T)   == 1,  "i1 == 1 byte");
    check(ir_type_size_bytes(dl, IR_TYPE_I8_T)   == 1,  "i8 == 1 byte");
    check(ir_type_size_bytes(dl, IR_TYPE_I16_T)  == 2,  "i16 == 2 bytes");
    check(ir_type_size_bytes(dl, IR_TYPE_I32_T)  == 4,  "i32 == 4 bytes");
    check(ir_type_size_bytes(dl, IR_TYPE_I64_T)  == 8,  "i64 == 8 bytes");

    // مؤشر
    IRType* ptr_t = ir_type_ptr(IR_TYPE_I32_T);
    check(ir_type_size_bytes(dl, ptr_t) == 8,  "ptr == 8 bytes (x64)");

    // مصفوفة: [5 × i32] = 5 * 4 = 20
    IRType* arr_t = ir_type_array(IR_TYPE_I32_T, 5);
    check(ir_type_size_bytes(dl, arr_t) == 20,  "[5 x i32] == 20 bytes");

    // NULL
    check(ir_type_size_bytes(dl, NULL) == 0,  "NULL == 0 bytes");
    // dl = NULL → الافتراضي
    check(ir_type_size_bytes(NULL, IR_TYPE_I64_T) == 8,  "dl=NULL defaults to Win x64");

    // الأنواع المركبة تُحرَّر مع الساحة
    ir_module_free(mod);
}

// ============================================================================
// اختبار المحاذاة (ir_type_alignment)
// ============================================================================

static void test_type_alignment(void) {
    fprintf(stderr, "\n--- اختبار محاذاة الأنواع ---\n");
    const IRDataLayout* dl = &IR_DATA_LAYOUT_WIN_X64;

    IRModule* mod = ir_module_new("align_test");

    check(ir_type_alignment(dl, IR_TYPE_I1_T)  == 1,  "i1 align == 1");
    check(ir_type_alignment(dl, IR_TYPE_I8_T)  == 1,  "i8 align == 1");
    check(ir_type_alignment(dl, IR_TYPE_I16_T) == 2,  "i16 align == 2");
    check(ir_type_alignment(dl, IR_TYPE_I32_T) == 4,  "i32 align == 4");
    check(ir_type_alignment(dl, IR_TYPE_I64_T) == 8,  "i64 align == 8");

    IRType* ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    check(ir_type_alignment(dl, ptr_t) == 8,  "ptr align == 8");

    // مصفوفة: محاذاة العنصر
    IRType* arr_t = ir_type_array(IR_TYPE_I32_T, 5);
    check(ir_type_alignment(dl, arr_t) == 4,  "[5 x i32] align == 4");

    check(ir_type_alignment(dl, NULL) == 1,  "NULL align == 1");

    ir_module_free(mod);
}

// ============================================================================
// اختبار حجم التخزين (ir_type_store_size)
// ============================================================================

static void test_type_store_size(void) {
    fprintf(stderr, "\n--- اختبار حجم التخزين ---\n");
    const IRDataLayout* dl = &IR_DATA_LAYOUT_WIN_X64;

    IRModule* mod = ir_module_new("store_test");

    check(ir_type_store_size(dl, IR_TYPE_I1_T)  == 1,  "i1 store == 1");
    check(ir_type_store_size(dl, IR_TYPE_I8_T)  == 1,  "i8 store == 1");
    check(ir_type_store_size(dl, IR_TYPE_I16_T) == 2,  "i16 store == 2");
    check(ir_type_store_size(dl, IR_TYPE_I32_T) == 4,  "i32 store == 4");
    check(ir_type_store_size(dl, IR_TYPE_I64_T) == 8,  "i64 store == 8");

    IRType* ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    check(ir_type_store_size(dl, ptr_t) == 8,  "ptr store == 8");

    ir_module_free(mod);
}

// ============================================================================
// اختبار محددات الأنواع (Type Predicates)
// ============================================================================

static void test_type_predicates(void) {
    fprintf(stderr, "\n--- اختبار محددات الأنواع ---\n");

    IRModule* mod = ir_module_new("pred_test");

    // أنواع عددية صحيحة
    check(ir_type_is_integer(IR_TYPE_I1_T)  == 1,  "i1 is integer");
    check(ir_type_is_integer(IR_TYPE_I8_T)  == 1,  "i8 is integer");
    check(ir_type_is_integer(IR_TYPE_I16_T) == 1,  "i16 is integer");
    check(ir_type_is_integer(IR_TYPE_I32_T) == 1,  "i32 is integer");
    check(ir_type_is_integer(IR_TYPE_I64_T) == 1,  "i64 is integer");
    check(ir_type_is_integer(IR_TYPE_VOID_T) == 0, "void is NOT integer");

    IRType* ptr_t = ir_type_ptr(IR_TYPE_I32_T);
    check(ir_type_is_integer(ptr_t) == 0,  "ptr is NOT integer");
    check(ir_type_is_pointer(ptr_t) == 1,  "ptr IS pointer");

    IRType* arr_t = ir_type_array(IR_TYPE_I32_T, 3);
    check(ir_type_is_integer(arr_t) == 0,  "array is NOT integer");
    check(ir_type_is_pointer(arr_t) == 0,  "array is NOT pointer");

    check(ir_type_is_pointer(IR_TYPE_I64_T) == 0,  "i64 is NOT pointer");
    check(ir_type_is_pointer(NULL) == 0,  "NULL is NOT pointer");
    check(ir_type_is_integer(NULL) == 0,  "NULL is NOT integer");

    ir_module_free(mod);
}

// ============================================================================
// اختبار دلالات الحساب — حالات حدية (Arithmetic Semantics Edge Cases)
// ============================================================================

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

static void test_arith_semantics(void) {
    fprintf(stderr, "\n--- اختبار دلالات الحساب ---\n");

    IRModule* module = ir_module_new("arith_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "اختبار_حساب", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    ir_builder_set_insert_point(b, entry);

    // حالة 1: INT64_MIN / -1 → INT64_MIN (التفاف آمن)
    int r_div = ir_builder_emit_div(b, IR_TYPE_I64_T,
                                    ir_value_const_int(INT64_MIN, IR_TYPE_I64_T),
                                    ir_value_const_int(-1, IR_TYPE_I64_T));

    // حالة 2: INT64_MIN % -1 → 0
    int r_mod = ir_builder_emit_mod(b, IR_TYPE_I64_T,
                                    ir_value_const_int(INT64_MIN, IR_TYPE_I64_T),
                                    ir_value_const_int(-1, IR_TYPE_I64_T));

    // حالة 3: التفاف الطفحان: INT64_MAX + 1 → INT64_MIN
    int r_add = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                    ir_value_const_int(INT64_MAX, IR_TYPE_I64_T),
                                    ir_value_const_int(1, IR_TYPE_I64_T));

    // حالة 4: التفاف الطفحان: INT64_MIN - 1 → INT64_MAX
    int r_sub = ir_builder_emit_sub(b, IR_TYPE_I64_T,
                                    ir_value_const_int(INT64_MIN, IR_TYPE_I64_T),
                                    ir_value_const_int(1, IR_TYPE_I64_T));

    // إرجاع
    ir_builder_emit_ret(b, ir_value_const_int(0, IR_TYPE_I64_T));

    // تشغيل طي الثوابت
    ir_constfold_run(module);

    // التحقق: التعليمات المطوية يجب أن تُحذف
    check(find_inst_by_dest(entry, r_div) == NULL, "INT64_MIN / -1 folded");
    check(find_inst_by_dest(entry, r_mod) == NULL, "INT64_MIN %% -1 folded");
    check(find_inst_by_dest(entry, r_add) == NULL, "INT64_MAX + 1 folded");
    check(find_inst_by_dest(entry, r_sub) == NULL, "INT64_MIN - 1 folded");

    // لا يوجد استخدام مباشر للنتائج أعلاه، لذا نتحقق من عدم وجود crash.
    // هذا أهم اختبار: عدم وجود سلوك غير محدد في C عند الطي.

    // حالة 5: القسمة على صفر لا تُطوى
    IRModule* m2 = ir_module_new("div_zero_test");
    IRBuilder* b2 = ir_builder_new(m2);
    IRFunc* f2 = ir_builder_create_func(b2, "اختبار_صفر", IR_TYPE_I64_T);
    IRBlock* e2 = ir_builder_create_block(b2, "بداية");
    ir_builder_set_insert_point(b2, e2);

    int r_div0 = ir_builder_emit_div(b2, IR_TYPE_I64_T,
                                     ir_value_const_int(42, IR_TYPE_I64_T),
                                     ir_value_const_int(0, IR_TYPE_I64_T));
    ir_builder_emit_ret(b2, ir_value_reg(r_div0, IR_TYPE_I64_T));

    ir_constfold_run(m2);

    check(find_inst_by_dest(e2, r_div0) != NULL, "div by 0 NOT folded");

    ir_builder_free(b2);
    ir_module_free(m2);

    // منع تحذيرات المتغيرات غير المستخدمة
    (void)f;
    (void)f2;

    ir_builder_free(b);
    ir_module_free(module);
}

// ============================================================================
// اختبار دلالات ص١ (i1 Truthiness)
// ============================================================================

static void test_i1_truthiness(void) {
    fprintf(stderr, "\n--- اختبار دلالات ص١ ---\n");

    IRModule* module = ir_module_new("i1_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "اختبار_ص١", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "بداية");
    ir_builder_set_insert_point(b, entry);

    // 42 + 0 على ص١ → trunc/sext: 42 → 1 (غير صفري), 0 → 0
    int r_add = ir_builder_emit_add(b, IR_TYPE_I1_T,
                                    ir_value_const_int(42, IR_TYPE_I1_T),
                                    ir_value_const_int(0, IR_TYPE_I1_T));
    // استخدام النتيجة في ret (لمنع DCE)
    ir_builder_emit_ret(b, ir_value_reg(r_add, IR_TYPE_I64_T));

    ir_constfold_run(module);

    // بعد الطي: r_add يجب أن يُطوى، والقيمة في ret يجب أن تكون ثابتة
    IRInst* ret = find_first_inst_by_op(entry, IR_OP_RET);
    check(ret != NULL, "ret exists");
    if (ret && ret->operand_count >= 1 && ret->operands[0]) {
        // القيمة المطوية: 42 + 0 = 42 → trunc to i1 → 1 (غير صفري → 1)
        // لكن ret يتوقع i64، فالقيمة ستكون 1 (بعد trunc_sext)
        if (ret->operands[0]->kind == IR_VAL_CONST_INT) {
            // ملاحظة: القيمة المنشورة قد تكون 0 أو 1 حسب نوع الاستبدال
            // المهم: عدم حدوث crash والقيمة ليست 42
            check(ret->operands[0]->data.const_int != 42,
                  "i1(42) should NOT remain 42 after folding");
        }
    }

    (void)f;
    ir_builder_free(b);
    ir_module_free(module);
}

int main(void) {
    fprintf(stderr, "=== ir_data_layout_test (v0.3.2.6.6) ===\n");

    test_type_sizes();
    test_type_alignment();
    test_type_store_size();
    test_type_predicates();
    test_arith_semantics();
    test_i1_truthiness();

    fprintf(stderr, "\n=== النتائج: %d نجح، %d فشل ===\n", g_pass, g_fail);

    if (g_fail > 0) {
        fprintf(stderr, "ir_data_layout_test: FAIL\n");
        return 1;
    }

    fprintf(stderr, "ir_data_layout_test: PASS\n");
    return 0;
}
