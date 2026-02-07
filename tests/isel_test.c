/**
 * @file isel_test.c
 * @brief اختبارات اختيار التعليمات (v0.3.2.1).
 *
 * يبني وحدة IR يدوياً ويشغل isel_run() ثم يتحقق من:
 * - العمليات الحسابية (جمع، طرح، ضرب) → mov+op
 * - القسمة → mov+cqo+idiv
 * - المقارنة → cmp+setCC+movzx
 * - تضمين القيم الفورية
 * - القفز المشروط → test+jne+jmp
 * - الرجوع → mov ret, val + ret
 * - تخصيص المكدس (alloca) → lea
 * - التحميل والتخزين → load/store
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/isel.h"

// ============================================================================
// دوال مساعدة للاختبار
// ============================================================================

static int test_count = 0;
static int pass_count = 0;

static int require(int cond, const char* msg) {
    test_count++;
    if (!cond) {
        fprintf(stderr, "  FAIL: %s\n", msg);
        return 0;
    }
    pass_count++;
    return 1;
}

/**
 * @brief البحث عن أول تعليمة بكود عملية معين في كتلة آلية.
 */
static MachineInst* find_mach_inst(MachineBlock* block, MachineOp op) {
    if (!block) return NULL;
    for (MachineInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == op) return inst;
    }
    return NULL;
}

/**
 * @brief عدّ تعليمات بكود عملية معين في كتلة آلية.
 */
static int count_mach_inst(MachineBlock* block, MachineOp op) {
    int count = 0;
    if (!block) return 0;
    for (MachineInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == op) count++;
    }
    return count;
}

/**
 * @brief البحث عن تعليمة آلة بكود عملية و ir_reg معين.
 */
static MachineInst* find_mach_inst_by_ir_reg(MachineBlock* block, MachineOp op, int ir_reg) {
    if (!block) return NULL;
    for (MachineInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == op && inst->ir_reg == ir_reg) return inst;
    }
    return NULL;
}

// ============================================================================
// اختبار ١: العمليات الحسابية الثنائية
// ============================================================================

/**
 * @brief اختبار خفض الجمع والطرح والضرب.
 *
 * IR:
 *   %0 = جمع ص٦٤ 10, 20
 *   %1 = طرح ص٦٤ %0, 5
 *   %2 = ضرب ص٦٤ %1, 3
 *   رجوع %2
 *
 * المتوقع:
 *   mov v0, $10; add v0, $20
 *   mov v1, v0;  sub v1, $5
 *   mov v2, v1;  imul v2, $3
 *   mov %ret, v2; ret
 */
static int test_binop(void) {
    fprintf(stderr, "--- اختبار ١: العمليات الحسابية ---\n");

    IRModule* module = ir_module_new("binop_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_binop", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                  ir_value_const_int(10, IR_TYPE_I64_T),
                                  ir_value_const_int(20, IR_TYPE_I64_T));
    int r1 = ir_builder_emit_sub(b, IR_TYPE_I64_T,
                                  ir_value_reg(r0, IR_TYPE_I64_T),
                                  ir_value_const_int(5, IR_TYPE_I64_T));
    int r2 = ir_builder_emit_mul(b, IR_TYPE_I64_T,
                                  ir_value_reg(r1, IR_TYPE_I64_T),
                                  ir_value_const_int(3, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r2, IR_TYPE_I64_T));

    // تشغيل اختيار التعليمات
    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة غير فارغة");
    ok &= require(mmod->func_count == 1, "يجب أن تكون هناك دالة واحدة");

    if (mmod && mmod->funcs) {
        MachineFunc* mf = mmod->funcs;
        ok &= require(mf->block_count == 1, "يجب أن تكون هناك كتلة واحدة");

        if (mf->blocks) {
            MachineBlock* mb = mf->blocks;

            // التحقق من وجود تعليمات الجمع والطرح والضرب
            ok &= require(find_mach_inst(mb, MACH_ADD) != NULL,
                          "يجب أن توجد تعليمة add");
            ok &= require(find_mach_inst(mb, MACH_SUB) != NULL,
                          "يجب أن توجد تعليمة sub");
            ok &= require(find_mach_inst(mb, MACH_IMUL) != NULL,
                          "يجب أن توجد تعليمة imul");

            // التحقق من تعليمات mov (واحدة لكل عملية + واحدة لـ ret)
            int mov_count = count_mach_inst(mb, MACH_MOV);
            ok &= require(mov_count >= 4,
                          "يجب أن توجد 4 تعليمات mov على الأقل (3 binop + 1 ret)");

            // التحقق من وجود ret
            ok &= require(find_mach_inst(mb, MACH_RET) != NULL,
                          "يجب أن توجد تعليمة ret");

            // التحقق من أن add تستخدم قيمة فورية
            MachineInst* add = find_mach_inst(mb, MACH_ADD);
            if (add) {
                ok &= require(add->src2.kind == MACH_OP_IMM,
                              "add يجب أن تستخدم قيمة فورية كمعامل ثاني");
                ok &= require(add->src2.data.imm == 20,
                              "add يجب أن تستخدم القيمة 20");
            }
        }
    }

    // طباعة للتنقيح
    if (mmod) {
        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٢: القسمة والباقي
// ============================================================================

/**
 * @brief اختبار خفض القسمة.
 *
 * IR:
 *   %0 = قسم ص٦٤ 100, 7
 *   رجوع %0
 *
 * المتوقع:
 *   mov tmp, $7         (لأن idiv لا يقبل فوري)
 *   mov v0, $100
 *   cqo
 *   idiv tmp
 *   mov %ret, v0; ret
 */
static int test_division(void) {
    fprintf(stderr, "--- اختبار ٢: القسمة ---\n");

    IRModule* module = ir_module_new("div_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_div", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_div(b, IR_TYPE_I64_T,
                                  ir_value_const_int(100, IR_TYPE_I64_T),
                                  ir_value_const_int(7, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r0, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs && mmod->funcs->blocks) {
        MachineBlock* mb = mmod->funcs->blocks;

        // التحقق من نمط القسمة: mov + cqo + idiv
        ok &= require(find_mach_inst(mb, MACH_CQO) != NULL,
                      "يجب أن توجد تعليمة cqo");
        ok &= require(find_mach_inst(mb, MACH_IDIV) != NULL,
                      "يجب أن توجد تعليمة idiv");

        // التحقق من أن القاسم الفوري نُقل لسجل مؤقت
        // (لأن idiv لا يقبل فوري مباشرة)
        MachineInst* idiv = find_mach_inst(mb, MACH_IDIV);
        if (idiv) {
            ok &= require(idiv->src1.kind == MACH_OP_VREG,
                          "idiv يجب أن يستخدم سجل (لا فوري) كقاسم");
        }

        // طباعة للتنقيح
        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٣: المقارنة
// ============================================================================

/**
 * @brief اختبار خفض المقارنة.
 *
 * IR:
 *   %0 = قارن أكبر ص٦٤ 10, 5
 *   رجوع %0
 *
 * المتوقع:
 *   cmp $10, $5     (أو mov tmp, $10; cmp tmp, $5)
 *   setg v0_8bit
 *   movzx v0_64bit, v0_8bit
 *   mov %ret, v0; ret
 */
static int test_comparison(void) {
    fprintf(stderr, "--- اختبار ٣: المقارنة ---\n");

    IRModule* module = ir_module_new("cmp_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_cmp", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_cmp(b, IR_CMP_GT,
                                  ir_value_const_int(10, IR_TYPE_I64_T),
                                  ir_value_const_int(5, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r0, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs && mmod->funcs->blocks) {
        MachineBlock* mb = mmod->funcs->blocks;

        // التحقق من نمط المقارنة: cmp + setg + movzx
        ok &= require(find_mach_inst(mb, MACH_CMP) != NULL,
                      "يجب أن توجد تعليمة cmp");
        ok &= require(find_mach_inst(mb, MACH_SETG) != NULL,
                      "يجب أن توجد تعليمة setg (أكبر)");
        ok &= require(find_mach_inst(mb, MACH_MOVZX) != NULL,
                      "يجب أن توجد تعليمة movzx (توسيع بالأصفار)");

        // التحقق من أن المعامل الأيسر الفوري نُقل لسجل
        MachineInst* cmp = find_mach_inst(mb, MACH_CMP);
        if (cmp) {
            ok &= require(cmp->src1.kind == MACH_OP_VREG,
                          "cmp يجب أن يستخدم سجل كمعامل أول (لا فوري)");
        }

        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٤: القفز المشروط
// ============================================================================

/**
 * @brief اختبار خفض القفز المشروط.
 *
 * IR:
 *   %0 = قارن يساوي ص٦٤ 1, 1
 *   قفز_شرط %0, if_true, if_false
 * if_true:
 *   رجوع 42
 * if_false:
 *   رجوع 0
 *
 * المتوقع:
 *   test cond, cond; jne if_true; jmp if_false
 */
static int test_conditional_branch(void) {
    fprintf(stderr, "--- اختبار ٤: القفز المشروط ---\n");

    IRModule* module = ir_module_new("br_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_br", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "entry");
    IRBlock* if_true = ir_builder_create_block(b, "if_true");
    IRBlock* if_false = ir_builder_create_block(b, "if_false");

    // كتلة الدخول
    ir_builder_set_insert_point(b, entry);
    int r0 = ir_builder_emit_cmp(b, IR_CMP_EQ,
                                  ir_value_const_int(1, IR_TYPE_I64_T),
                                  ir_value_const_int(1, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(r0, IR_TYPE_I1_T), if_true, if_false);

    // كتلة الصحيح
    ir_builder_set_insert_point(b, if_true);
    ir_builder_emit_ret(b, ir_value_const_int(42, IR_TYPE_I64_T));

    // كتلة الخطأ
    ir_builder_set_insert_point(b, if_false);
    ir_builder_emit_ret(b, ir_value_const_int(0, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs) {
        MachineFunc* mf = mmod->funcs;
        ok &= require(mf->block_count == 3, "يجب أن تكون هناك 3 كتل");

        // التحقق من كتلة الدخول
        if (mf->blocks) {
            MachineBlock* entry_mb = mf->blocks;

            ok &= require(find_mach_inst(entry_mb, MACH_TEST) != NULL,
                          "يجب أن توجد تعليمة test في كتلة الدخول");
            ok &= require(find_mach_inst(entry_mb, MACH_JNE) != NULL,
                          "يجب أن توجد تعليمة jne في كتلة الدخول");
            ok &= require(find_mach_inst(entry_mb, MACH_JMP) != NULL,
                          "يجب أن توجد تعليمة jmp في كتلة الدخول");
        }

        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٥: تخصيص المكدس والتحميل والتخزين
// ============================================================================

/**
 * @brief اختبار خفض alloca + store + load.
 *
 * IR:
 *   %0 = حجز ص٦٤
 *   خزن 42, %0
 *   %1 = حمل ص٦٤, %0
 *   رجوع %1
 *
 * المتوقع:
 *   lea v0, [rbp - offset]
 *   store [v0], $42
 *   load v1, [v0]
 *   mov %ret, v1; ret
 */
static int test_memory_ops(void) {
    fprintf(stderr, "--- اختبار ٥: الذاكرة (alloca/store/load) ---\n");

    IRModule* module = ir_module_new("mem_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_mem", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ir_value_const_int(42, IR_TYPE_I64_T),
                           ir_value_reg(r0, ir_type_ptr(IR_TYPE_I64_T)));
    int r1 = ir_builder_emit_load(b, IR_TYPE_I64_T,
                                   ir_value_reg(r0, ir_type_ptr(IR_TYPE_I64_T)));
    ir_builder_emit_ret(b, ir_value_reg(r1, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs && mmod->funcs->blocks) {
        MachineFunc* mf = mmod->funcs;
        MachineBlock* mb = mf->blocks;

        // التحقق من alloca → lea
        ok &= require(find_mach_inst(mb, MACH_LEA) != NULL,
                      "يجب أن توجد تعليمة lea (من alloca)");

        // التحقق من store
        ok &= require(find_mach_inst(mb, MACH_STORE) != NULL,
                      "يجب أن توجد تعليمة store");

        // التحقق من load
        ok &= require(find_mach_inst(mb, MACH_LOAD) != NULL,
                      "يجب أن توجد تعليمة load");

        // التحقق من أن حجم المكدس تم حسابه
        ok &= require(mf->stack_size > 0,
                      "يجب أن يكون حجم المكدس > 0 بعد alloca");

        // التحقق من أن lea يستخدم mem operand مع rbp
        MachineInst* lea = find_mach_inst(mb, MACH_LEA);
        if (lea) {
            ok &= require(lea->src1.kind == MACH_OP_MEM,
                          "lea يجب أن يستخدم معامل ذاكرة");
            ok &= require(lea->src1.data.mem.base_vreg == -1,
                          "lea يجب أن يستخدم RBP كقاعدة (vreg -1)");
            ok &= require(lea->src1.data.mem.offset < 0,
                          "lea يجب أن يكون الإزاحة سالبة (أسفل RBP)");
        }

        // التحقق من أن store يخزن قيمة فورية في الذاكرة
        MachineInst* store = find_mach_inst(mb, MACH_STORE);
        if (store) {
            ok &= require(store->dst.kind == MACH_OP_MEM,
                          "store يجب أن يكون الوجهة معامل ذاكرة");
            ok &= require(store->src1.kind == MACH_OP_IMM,
                          "store يجب أن تكون القيمة فورية (42)");
            ok &= require(store->src1.data.imm == 42,
                          "store يجب أن تكون القيمة = 42");
        }

        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٦: العمليات المنطقية
// ============================================================================

/**
 * @brief اختبار خفض AND, OR, NOT.
 *
 * IR:
 *   %0 = و ص٦٤ 0xFF, 0x0F
 *   %1 = أو ص٦٤ %0, 0xF0
 *   %2 = نفي ص٦٤ %1
 *   رجوع %2
 */
static int test_logical_ops(void) {
    fprintf(stderr, "--- اختبار ٦: العمليات المنطقية ---\n");

    IRModule* module = ir_module_new("logic_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_logic", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_and(b, IR_TYPE_I64_T,
                                  ir_value_const_int(0xFF, IR_TYPE_I64_T),
                                  ir_value_const_int(0x0F, IR_TYPE_I64_T));
    int r1 = ir_builder_emit_or(b, IR_TYPE_I64_T,
                                 ir_value_reg(r0, IR_TYPE_I64_T),
                                 ir_value_const_int(0xF0, IR_TYPE_I64_T));
    int r2 = ir_builder_emit_not(b, IR_TYPE_I64_T,
                                  ir_value_reg(r1, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r2, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs && mmod->funcs->blocks) {
        MachineBlock* mb = mmod->funcs->blocks;

        ok &= require(find_mach_inst(mb, MACH_AND) != NULL,
                      "يجب أن توجد تعليمة and");
        ok &= require(find_mach_inst(mb, MACH_OR) != NULL,
                      "يجب أن توجد تعليمة or");
        ok &= require(find_mach_inst(mb, MACH_NOT) != NULL,
                      "يجب أن توجد تعليمة not");

        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٧: طباعة وتحرير
// ============================================================================

/**
 * @brief اختبار طباعة تمثيل الآلة وتحرير الذاكرة.
 */
static int test_print_and_free(void) {
    fprintf(stderr, "--- اختبار ٧: الطباعة والتحرير ---\n");

    IRModule* module = ir_module_new("print_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_print", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    ir_builder_emit_add(b, IR_TYPE_I64_T,
                         ir_value_const_int(1, IR_TYPE_I64_T),
                         ir_value_const_int(2, IR_TYPE_I64_T));
    ir_builder_emit_ret_int(b, 0);

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    // اختبار أسماء العمليات
    ok &= require(strcmp(mach_op_to_string(MACH_ADD), "add") == 0,
                  "mach_op_to_string(MACH_ADD) يجب أن يُرجع 'add'");
    ok &= require(strcmp(mach_op_to_string(MACH_RET), "ret") == 0,
                  "mach_op_to_string(MACH_RET) يجب أن يُرجع 'ret'");
    ok &= require(strcmp(mach_op_to_string(MACH_IDIV), "idiv") == 0,
                  "mach_op_to_string(MACH_IDIV) يجب أن يُرجع 'idiv'");

    // اختبار بناء المعاملات
    MachineOperand vreg = mach_op_vreg(5, 64);
    ok &= require(vreg.kind == MACH_OP_VREG, "mach_op_vreg يجب أن يُنشئ VREG");
    ok &= require(vreg.data.vreg == 5, "vreg number يجب أن يكون 5");
    ok &= require(vreg.size_bits == 64, "vreg size يجب أن يكون 64");

    MachineOperand imm = mach_op_imm(42, 64);
    ok &= require(imm.kind == MACH_OP_IMM, "mach_op_imm يجب أن يُنشئ IMM");
    ok &= require(imm.data.imm == 42, "imm value يجب أن يكون 42");

    MachineOperand mem = mach_op_mem(-1, -8, 64);
    ok &= require(mem.kind == MACH_OP_MEM, "mach_op_mem يجب أن يُنشئ MEM");
    ok &= require(mem.data.mem.base_vreg == -1, "mem base يجب أن يكون -1 (RBP)");
    ok &= require(mem.data.mem.offset == -8, "mem offset يجب أن يكون -8");

    MachineOperand lbl = mach_op_label(3);
    ok &= require(lbl.kind == MACH_OP_LABEL, "mach_op_label يجب أن يُنشئ LABEL");
    ok &= require(lbl.data.label_id == 3, "label id يجب أن يكون 3");

    MachineOperand none = mach_op_none();
    ok &= require(none.kind == MACH_OP_NONE, "mach_op_none يجب أن يُنشئ NONE");

    // طباعة للتنقيح
    if (mmod) {
        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    // تحرير الذاكرة (لا يجب أن ينهار)
    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٨: السالب (negation)
// ============================================================================

static int test_negation(void) {
    fprintf(stderr, "--- اختبار ٨: السالب ---\n");

    IRModule* module = ir_module_new("neg_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_neg", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_neg(b, IR_TYPE_I64_T,
                                  ir_value_const_int(42, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r0, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs && mmod->funcs->blocks) {
        MachineBlock* mb = mmod->funcs->blocks;

        ok &= require(find_mach_inst(mb, MACH_NEG) != NULL,
                      "يجب أن توجد تعليمة neg");

        fprintf(stderr, "  -- Machine IR output --\n");
        mach_module_print(mmod, stderr);
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// نقطة الدخول الرئيسية
// ============================================================================

int main(void) {
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  اختبارات اختيار التعليمات (v0.3.2.1)\n");
    fprintf(stderr, "========================================\n\n");

    int ok = 1;

    ok &= test_binop();
    fprintf(stderr, "\n");

    ok &= test_division();
    fprintf(stderr, "\n");

    ok &= test_comparison();
    fprintf(stderr, "\n");

    ok &= test_conditional_branch();
    fprintf(stderr, "\n");

    ok &= test_memory_ops();
    fprintf(stderr, "\n");

    ok &= test_logical_ops();
    fprintf(stderr, "\n");

    ok &= test_print_and_free();
    fprintf(stderr, "\n");

    ok &= test_negation();
    fprintf(stderr, "\n");

    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  النتيجة: %d/%d اختبار نجح\n", pass_count, test_count);
    fprintf(stderr, "========================================\n");

    if (!ok) {
        fprintf(stderr, "isel_test: FAIL\n");
        return 1;
    }

    fprintf(stderr, "isel_test: PASS\n");
    return 0;
}
