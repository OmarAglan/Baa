/**
 * @file regalloc_test.c
 * @brief اختبارات تخصيص السجلات (v0.3.2.2).
 *
 * يبني وحدة IR يدوياً، يشغل isel_run() ثم regalloc_run()، ويتحقق من:
 * - تحليل الحيوية (liveness analysis)
 * - بناء فترات الحيوية (live intervals)
 * - تخصيص السجلات الفيزيائية (linear scan allocation)
 * - حل السجلات الخاصة (special vreg resolution)
 * - التسريب عند نفاد السجلات (spilling)
 * - إعادة كتابة المعاملات (operand rewrite)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/isel.h"
#include "../src/regalloc.h"

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
 * @brief التحقق من عدم وجود معاملات VREG بأرقام سالبة بعد التخصيص.
 *
 * بعد تخصيص السجلات، يجب أن تُحل جميع السجلات الخاصة
 * (مثل -2 → RAX, -10 → RCX, إلخ).
 */
static int check_no_negative_vregs(MachineFunc* func) {
    for (MachineBlock* block = func->blocks; block; block = block->next) {
        for (MachineInst* inst = block->first; inst; inst = inst->next) {
            // نتحقق فقط من VREG (ليس MEM الذي قد يحتوي base_vreg خاص)
            if (inst->dst.kind == MACH_OP_VREG && inst->dst.data.vreg < 0) {
                // السجلات الخاصة يجب أن تُحل إلى أرقام موجبة (0-15)
                // ما عدا حالات معينة نتجاهلها
                return 0;
            }
            if (inst->src1.kind == MACH_OP_VREG && inst->src1.data.vreg < 0) {
                return 0;
            }
            if (inst->src2.kind == MACH_OP_VREG && inst->src2.data.vreg < 0) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * @brief التحقق من أن جميع VREG العادية استُبدلت بأرقام فيزيائية (0-15).
 */
static int check_all_vregs_physical(MachineFunc* func, int original_max_vreg) {
    for (MachineBlock* block = func->blocks; block; block = block->next) {
        for (MachineInst* inst = block->first; inst; inst = inst->next) {
            // dst
            if (inst->dst.kind == MACH_OP_VREG) {
                int v = inst->dst.data.vreg;
                // يجب أن يكون سجل فيزيائي (0-15) أو تم تحويله لذاكرة
                if (v >= PHYS_REG_COUNT) return 0;
            }
            // src1
            if (inst->src1.kind == MACH_OP_VREG) {
                int v = inst->src1.data.vreg;
                if (v >= PHYS_REG_COUNT) return 0;
            }
            // src2
            if (inst->src2.kind == MACH_OP_VREG) {
                int v = inst->src2.data.vreg;
                if (v >= PHYS_REG_COUNT) return 0;
            }
        }
    }
    return 1;
}

static MachineBlock* find_mblock_by_id(MachineFunc* func, int id) {
    if (!func) return NULL;
    for (MachineBlock* b = func->blocks; b; b = b->next) {
        if (b->id == id) return b;
    }
    return NULL;
}

// ============================================================================
// اختبار ١: تخصيص بسيط (عدد قليل من السجلات)
// ============================================================================

/**
 * @brief اختبار تخصيص سجلات لعمليات حسابية بسيطة.
 *
 * IR:
 *   %0 = جمع ص٦٤ 10, 20
 *   %1 = طرح ص٦٤ %0, 5
 *   رجوع %1
 *
 * المتوقع: كل السجلات الافتراضية تُخصص لسجلات فيزيائية
 * بدون تسريب (عدد السجلات أقل بكثير من المتاح).
 */
static int test_simple_allocation(void) {
    fprintf(stderr, "--- اختبار ١: تخصيص بسيط ---\n");

    IRModule* module = ir_module_new("simple_alloc_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_simple", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                  ir_value_const_int(10, IR_TYPE_I64_T),
                                  ir_value_const_int(20, IR_TYPE_I64_T));
    int r1 = ir_builder_emit_sub(b, IR_TYPE_I64_T,
                                  ir_value_reg(r0, IR_TYPE_I64_T),
                                  ir_value_const_int(5, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r1, IR_TYPE_I64_T));

    // اختيار التعليمات
    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod) {
        fprintf(stderr, "  -- قبل تخصيص السجلات --\n");
        mach_module_print(mmod, stderr);

        // تخصيص السجلات
        bool result = regalloc_run(mmod);
        ok &= require(result, "regalloc_run يجب أن ينجح");

        fprintf(stderr, "  -- بعد تخصيص السجلات --\n");
        mach_module_print(mmod, stderr);

        if (mmod->funcs) {
            MachineFunc* mf = mmod->funcs;

            // التحقق من حل السجلات الخاصة
            ok &= require(check_no_negative_vregs(mf),
                          "يجب أن تُحل جميع السجلات الخاصة (السالبة)");

            // التحقق من أن الأرقام في النطاق الفيزيائي
            ok &= require(check_all_vregs_physical(mf, f->next_reg),
                          "يجب أن تكون جميع VREG في نطاق السجلات الفيزيائية");
        }
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٢: تخصيص مع قسمة (سجلات مقيّدة)
// ============================================================================

/**
 * @brief اختبار تخصيص عملية القسمة.
 *
 * القسمة تستخدم RAX و RDX بشكل ضمني.
 * يجب أن يعمل التخصيص بشكل صحيح مع هذه القيود.
 */
static int test_division_allocation(void) {
    fprintf(stderr, "--- اختبار ٢: تخصيص مع قسمة ---\n");

    IRModule* module = ir_module_new("div_alloc_test");
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

    if (mmod) {
        bool result = regalloc_run(mmod);
        ok &= require(result, "regalloc_run يجب أن ينجح");

        fprintf(stderr, "  -- بعد تخصيص السجلات --\n");
        mach_module_print(mmod, stderr);

        if (mmod->funcs) {
            ok &= require(check_no_negative_vregs(mmod->funcs),
                          "يجب أن تُحل السجلات الخاصة بعد تخصيص القسمة");
        }
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٣: تخصيص مع تحكم في التدفق (عدة كتل)
// ============================================================================

/**
 * @brief اختبار تخصيص سجلات مع كتل متعددة (if/else).
 *
 * الحيوية تمتد عبر الكتل، مما يختبر تحليل الحيوية
 * بين الكتل وحسابات live-in/live-out.
 */
static int test_multiblock_allocation(void) {
    fprintf(stderr, "--- اختبار ٣: تخصيص مع كتل متعددة ---\n");

    IRModule* module = ir_module_new("multiblock_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_multi", IR_TYPE_I64_T);

    IRBlock* entry = ir_builder_create_block(b, "entry");
    IRBlock* if_true = ir_builder_create_block(b, "if_true");
    IRBlock* if_false = ir_builder_create_block(b, "if_false");

    // كتلة الدخول
    ir_builder_set_insert_point(b, entry);
    int r0 = ir_builder_emit_cmp(b, IR_CMP_GT,
                                  ir_value_const_int(10, IR_TYPE_I64_T),
                                  ir_value_const_int(5, IR_TYPE_I64_T));
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

    if (mmod) {
        // قبل تخصيص السجلات: يجب أن يكون CFG الآلي مربوطاً (succs غير NULL)
        if (mmod->funcs) {
            MachineFunc* mf = mmod->funcs;
            MachineBlock* b0 = find_mblock_by_id(mf, 0);
            MachineBlock* b1 = find_mblock_by_id(mf, 1);
            MachineBlock* b2 = find_mblock_by_id(mf, 2);
            ok &= require(b0 != NULL && b1 != NULL && b2 != NULL,
                          "يجب أن تُنشأ كتل الآلة بالمعرّفات 0/1/2");
            if (b0) {
                ok &= require(b0->succ_count == 2,
                              "كتلة الدخول يجب أن تملك خلفاء اثنين");
                ok &= require(b0->succs[0] != NULL && b0->succs[1] != NULL,
                              "يجب أن تكون succs[] غير NULL لكتلة الدخول");
            }
        }

        bool result = regalloc_run(mmod);
        ok &= require(result, "regalloc_run يجب أن ينجح");

        fprintf(stderr, "  -- بعد تخصيص السجلات --\n");
        mach_module_print(mmod, stderr);

        if (mmod->funcs) {
            ok &= require(mmod->funcs->block_count == 3,
                          "يجب أن تكون هناك 3 كتل");
            ok &= require(check_no_negative_vregs(mmod->funcs),
                          "يجب أن تُحل السجلات الخاصة في جميع الكتل");
            ok &= require(check_all_vregs_physical(mmod->funcs, f->next_reg),
                          "يجب أن تكون جميع VREG فيزيائية بعد التخصيص");
        }
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٤: تخصيص مع ذاكرة (alloca/load/store)
// ============================================================================

/**
 * @brief اختبار تخصيص مع عمليات الذاكرة.
 *
 * يتضمن alloca و store و load مما يولد سجلات إضافية
 * ومعاملات ذاكرة.
 */
static int test_memory_allocation(void) {
    fprintf(stderr, "--- اختبار ٤: تخصيص مع ذاكرة ---\n");

    IRModule* module = ir_module_new("mem_alloc_test");
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

    if (mmod) {
        bool result = regalloc_run(mmod);
        ok &= require(result, "regalloc_run يجب أن ينجح");

        fprintf(stderr, "  -- بعد تخصيص السجلات --\n");
        mach_module_print(mmod, stderr);

        if (mmod->funcs) {
            // التحقق من أن حجم المكدس لا يزال صحيحاً
            ok &= require(mmod->funcs->stack_size > 0,
                          "يجب أن يكون حجم المكدس > 0 بعد alloca");
            ok &= require(check_no_negative_vregs(mmod->funcs),
                          "يجب أن تُحل السجلات الخاصة");
        }
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٥: أسماء السجلات الفيزيائية
// ============================================================================

/**
 * @brief اختبار أسماء السجلات الفيزيائية والتصنيف.
 */
static int test_phys_reg_names(void) {
    fprintf(stderr, "--- اختبار ٥: أسماء السجلات الفيزيائية ---\n");

    int ok = 1;

    // اختبار الأسماء
    ok &= require(strcmp(phys_reg_name(PHYS_RAX), "rax") == 0,
                  "PHYS_RAX يجب أن يكون 'rax'");
    ok &= require(strcmp(phys_reg_name(PHYS_RCX), "rcx") == 0,
                  "PHYS_RCX يجب أن يكون 'rcx'");
    ok &= require(strcmp(phys_reg_name(PHYS_RDX), "rdx") == 0,
                  "PHYS_RDX يجب أن يكون 'rdx'");
    ok &= require(strcmp(phys_reg_name(PHYS_RBX), "rbx") == 0,
                  "PHYS_RBX يجب أن يكون 'rbx'");
    ok &= require(strcmp(phys_reg_name(PHYS_RSP), "rsp") == 0,
                  "PHYS_RSP يجب أن يكون 'rsp'");
    ok &= require(strcmp(phys_reg_name(PHYS_RBP), "rbp") == 0,
                  "PHYS_RBP يجب أن يكون 'rbp'");
    ok &= require(strcmp(phys_reg_name(PHYS_R8), "r8") == 0,
                  "PHYS_R8 يجب أن يكون 'r8'");
    ok &= require(strcmp(phys_reg_name(PHYS_R15), "r15") == 0,
                  "PHYS_R15 يجب أن يكون 'r15'");
    ok &= require(strcmp(phys_reg_name(PHYS_NONE), "none") == 0,
                  "PHYS_NONE يجب أن يكون 'none'");

    // اختبار callee-saved
    ok &= require(phys_reg_is_callee_saved(PHYS_RBX) == true,
                  "RBX يجب أن يكون callee-saved");
    ok &= require(phys_reg_is_callee_saved(PHYS_R12) == true,
                  "R12 يجب أن يكون callee-saved");
    ok &= require(phys_reg_is_callee_saved(PHYS_R13) == true,
                  "R13 يجب أن يكون callee-saved");
    ok &= require(phys_reg_is_callee_saved(PHYS_RAX) == false,
                  "RAX يجب أن لا يكون callee-saved");
    ok &= require(phys_reg_is_callee_saved(PHYS_R10) == false,
                  "R10 يجب أن لا يكون callee-saved");
    ok &= require(phys_reg_is_callee_saved(PHYS_RCX) == false,
                  "RCX يجب أن لا يكون callee-saved");

    return ok;
}

// ============================================================================
// اختبار ٦: سياق تخصيص السجلات (إنشاء وتحرير)
// ============================================================================

/**
 * @brief اختبار إنشاء وتحرير سياق التخصيص بدون تعطل.
 */
static int test_ctx_lifecycle(void) {
    fprintf(stderr, "--- اختبار ٦: سياق التخصيص ---\n");

    IRModule* module = ir_module_new("ctx_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_ctx", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);
    ir_builder_emit_ret_int(b, 0);

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs) {
        MachineFunc* mf = mmod->funcs;

        // إنشاء سياق التخصيص
        RegAllocCtx* ctx = regalloc_ctx_new(mf);
        ok &= require(ctx != NULL, "regalloc_ctx_new يجب أن يُرجع سياقاً");

        if (ctx) {
            ok &= require(ctx->func == mf,
                          "السياق يجب أن يشير للدالة الصحيحة");
            ok &= require(ctx->max_vreg >= 0,
                          "max_vreg يجب أن يكون >= 0");
            ok &= require(ctx->vreg_to_phys != NULL,
                          "vreg_to_phys يجب أن يكون مخصصاً");

            // ترقيم التعليمات
            regalloc_number_insts(ctx);
            ok &= require(ctx->total_insts > 0,
                          "يجب أن يكون هناك تعليمة واحدة على الأقل");
            ok &= require(ctx->inst_map != NULL,
                          "inst_map يجب أن يكون مخصصاً");

            // حساب def/use
            regalloc_compute_def_use(ctx);
            ok &= require(ctx->block_live != NULL,
                          "block_live يجب أن يكون مخصصاً");

            // حساب الحيوية
            regalloc_compute_liveness(ctx);

            // بناء الفترات
            regalloc_build_intervals(ctx);

            // طباعة الفترات
            fprintf(stderr, "  -- فترات الحيوية --\n");
            regalloc_print_intervals(ctx, stderr);

            // تحرير (يجب أن لا ينهار)
            regalloc_ctx_free(ctx);
        }
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٧: تخصيص مع عمليات متعددة (ضغط السجلات)
// ============================================================================

/**
 * @brief اختبار تخصيص مع العديد من السجلات الافتراضية المتزامنة.
 *
 * يبني سلسلة طويلة من العمليات الحسابية لاختبار
 * سلوك المخصص تحت ضغط السجلات.
 */
static int test_register_pressure(void) {
    fprintf(stderr, "--- اختبار ٧: ضغط السجلات ---\n");

    IRModule* module = ir_module_new("pressure_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_pressure", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    // بناء سلسلة من العمليات التي تخلق سجلات كثيرة
    int regs[8];
    for (int i = 0; i < 8; i++) {
        regs[i] = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                       ir_value_const_int(i * 10, IR_TYPE_I64_T),
                                       ir_value_const_int(i + 1, IR_TYPE_I64_T));
    }

    // استخدام كل السجلات في عملية واحدة تلو الأخرى
    int acc = regs[0];
    for (int i = 1; i < 8; i++) {
        acc = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                   ir_value_reg(acc, IR_TYPE_I64_T),
                                   ir_value_reg(regs[i], IR_TYPE_I64_T));
    }
    ir_builder_emit_ret(b, ir_value_reg(acc, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod) {
        bool result = regalloc_run(mmod);
        ok &= require(result, "regalloc_run يجب أن ينجح تحت ضغط السجلات");

        fprintf(stderr, "  -- بعد تخصيص السجلات (ضغط) --\n");
        mach_module_print(mmod, stderr);

        if (mmod->funcs) {
            ok &= require(check_no_negative_vregs(mmod->funcs),
                          "يجب أن تُحل السجلات الخاصة");
        }
    }

    if (mmod) mach_module_free(mmod);
    ir_builder_free(b);
    ir_module_free(module);

    return ok;
}

// ============================================================================
// اختبار ٨: فترات الحيوية (Liveness Intervals Correctness)
// ============================================================================

/**
 * @brief اختبار بناء فترات الحيوية.
 *
 * يبني IR بسيط ويتحقق من أن كل سجل افتراضي
 * لديه فترة حيوية صحيحة (البداية <= النهاية).
 */
static int test_liveness_intervals(void) {
    fprintf(stderr, "--- اختبار ٨: فترات الحيوية ---\n");

    IRModule* module = ir_module_new("liveness_test");
    IRBuilder* b = ir_builder_new(module);
    IRFunc* f = ir_builder_create_func(b, "test_liveness", IR_TYPE_I64_T);
    IRBlock* entry = ir_builder_create_block(b, "entry");
    ir_builder_set_insert_point(b, entry);

    int r0 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                  ir_value_const_int(1, IR_TYPE_I64_T),
                                  ir_value_const_int(2, IR_TYPE_I64_T));
    int r1 = ir_builder_emit_mul(b, IR_TYPE_I64_T,
                                  ir_value_reg(r0, IR_TYPE_I64_T),
                                  ir_value_const_int(3, IR_TYPE_I64_T));
    int r2 = ir_builder_emit_add(b, IR_TYPE_I64_T,
                                  ir_value_reg(r0, IR_TYPE_I64_T),
                                  ir_value_reg(r1, IR_TYPE_I64_T));
    ir_builder_emit_ret(b, ir_value_reg(r2, IR_TYPE_I64_T));

    MachineModule* mmod = isel_run(module);

    int ok = 1;
    ok &= require(mmod != NULL, "isel_run يجب أن يُرجع وحدة");

    if (mmod && mmod->funcs) {
        MachineFunc* mf = mmod->funcs;
        RegAllocCtx* ctx = regalloc_ctx_new(mf);
        ok &= require(ctx != NULL, "regalloc_ctx_new يجب أن ينجح");

        if (ctx) {
            regalloc_number_insts(ctx);
            regalloc_compute_def_use(ctx);
            regalloc_compute_liveness(ctx);
            regalloc_build_intervals(ctx);

            ok &= require(ctx->interval_count > 0,
                          "يجب أن توجد فترات حيوية");

            // التحقق من أن كل فترة لها start <= end
            for (int i = 0; i < ctx->interval_count; i++) {
                LiveInterval* li = &ctx->intervals[i];
                ok &= require(li->start <= li->end,
                              "فترة الحيوية يجب أن تكون start <= end");
                ok &= require(li->vreg >= 0,
                              "رقم السجل يجب أن يكون >= 0");
            }

            // طباعة للتنقيح
            fprintf(stderr, "  -- فترات الحيوية --\n");
            regalloc_print_intervals(ctx, stderr);

            regalloc_ctx_free(ctx);
        }
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
    fprintf(stderr, "  اختبارات تخصيص السجلات (v0.3.2.2)\n");
    fprintf(stderr, "========================================\n\n");

    int ok = 1;

    ok &= test_simple_allocation();
    fprintf(stderr, "\n");

    ok &= test_division_allocation();
    fprintf(stderr, "\n");

    ok &= test_multiblock_allocation();
    fprintf(stderr, "\n");

    ok &= test_memory_allocation();
    fprintf(stderr, "\n");

    ok &= test_phys_reg_names();
    fprintf(stderr, "\n");

    ok &= test_ctx_lifecycle();
    fprintf(stderr, "\n");

    ok &= test_register_pressure();
    fprintf(stderr, "\n");

    ok &= test_liveness_intervals();
    fprintf(stderr, "\n");

    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  النتيجة: %d/%d اختبار نجح\n", pass_count, test_count);
    fprintf(stderr, "========================================\n");

    if (!ok) {
        fprintf(stderr, "regalloc_test: FAIL\n");
        return 1;
    }

    fprintf(stderr, "regalloc_test: PASS\n");
    return 0;
}
