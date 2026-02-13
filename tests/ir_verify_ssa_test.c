/**
 * @file ir_verify_ssa_test.c
 * @brief اختبار خفيف للتحقق من SSA (تعريف واحد + سيطرة + فاي).
 *
 * هذا الاختبار ينفّذ حالتين:
 * 1) مسار صحيح: خفض برنامج `.baa` إلى IR ثم تشغيل المُحسِّن (Mem2Reg) ثم التحقق.
 * 2) حالات خاطئة مُصنَّعة: IR يدوي يخرق قواعد SSA للتأكد أن المُتحقِّق يرفضه.
 *
 * على النجاح: يطبع `ir_verify_ssa_test: PASS` إلى stderr ويعيد 0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/baa.h"
#include "../src/ir.h"
#include "../src/ir_lower.h"
#include "../src/ir_optimizer.h"
#include "../src/ir_verify_ssa.h"
#include "../src/ir_verify_ir.h"

// -----------------------------------------------------------------------------
// تنفيذ اختباري لـ read_file() لأننا لا نربط src/main.c ضمن اختبارات C
// -----------------------------------------------------------------------------
char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "خطأ اختبار: تعذر فتح الملف '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "خطأ اختبار: فشل تخصيص الذاكرة\n");
        exit(1);
    }

    fread(buffer, 1, (size_t)length, f);
    buffer[length] = '\0';
    fclose(f);
    return buffer;
}

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "فشل الاختبار: %s\n", msg);
        return 0;
    }
    return 1;
}

static IRModule* build_bad_module_duplicate_def(void) {
    IRModule* m = ir_module_new("bad_dup_def");
    if (!m) return NULL;

    IRFunc* f = ir_func_new("اختبار_تعريف_مكرر", IR_TYPE_I64_T);
    if (!f) {
        ir_module_free(m);
        return NULL;
    }

    IRBlock* entry = ir_func_new_block(f, "بداية");
    if (!entry) {
        ir_func_free(f);
        ir_module_free(m);
        return NULL;
    }

    IRInst* i0 = ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, 0,
                                ir_value_const_int(1, IR_TYPE_I64_T),
                                ir_value_const_int(2, IR_TYPE_I64_T));
    IRInst* i1 = ir_inst_binary(IR_OP_SUB, IR_TYPE_I64_T, 0,
                                ir_value_const_int(3, IR_TYPE_I64_T),
                                ir_value_const_int(1, IR_TYPE_I64_T));
    IRInst* ret = ir_inst_ret(ir_value_reg(0, IR_TYPE_I64_T));

    if (i0) ir_block_append(entry, i0);
    if (i1) ir_block_append(entry, i1);
    if (ret) ir_block_append(entry, ret);

    ir_module_add_func(m, f);
    return m;
}

static IRModule* build_bad_module_phi_missing_incoming(void) {
    IRModule* m = ir_module_new("bad_phi_missing");
    if (!m) return NULL;

    IRFunc* f = ir_func_new("اختبار_فاي_ناقص", IR_TYPE_I64_T);
    if (!f) {
        ir_module_free(m);
        return NULL;
    }

    IRBlock* entry = ir_func_new_block(f, "بداية");
    IRBlock* tbb = ir_func_new_block(f, "فرع_صواب");
    IRBlock* fbb = ir_func_new_block(f, "فرع_خطأ");
    IRBlock* merge = ir_func_new_block(f, "دمج");

    if (!entry || !tbb || !fbb || !merge) {
        ir_func_free(f);
        ir_module_free(m);
        return NULL;
    }

    IRValue* cond = ir_value_const_int(1, IR_TYPE_I1_T);
    IRInst* brc = ir_inst_br_cond(cond, tbb, fbb);
    if (brc) ir_block_append(entry, brc);

    IRInst* tval = ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, 1,
                                  ir_value_const_int(10, IR_TYPE_I64_T),
                                  ir_value_const_int(1, IR_TYPE_I64_T));
    if (tval) ir_block_append(tbb, tval);
    IRInst* tbr = ir_inst_br(merge);
    if (tbr) ir_block_append(tbb, tbr);

    IRInst* fval = ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, 2,
                                  ir_value_const_int(20, IR_TYPE_I64_T),
                                  ir_value_const_int(1, IR_TYPE_I64_T));
    if (fval) ir_block_append(fbb, fval);
    IRInst* fbr = ir_inst_br(merge);
    if (fbr) ir_block_append(fbb, fbr);

    IRInst* phi = ir_inst_phi(IR_TYPE_I64_T, 3);
    if (phi) {
        // إدخال واحد فقط؛ يجب أن يفشل لأن دمج لديه سابقان
        ir_inst_phi_add(phi, ir_value_reg(1, IR_TYPE_I64_T), tbb);
        ir_block_append(merge, phi);
    }

    IRInst* ret = ir_inst_ret(ir_value_reg(3, IR_TYPE_I64_T));
    if (ret) ir_block_append(merge, ret);

    ir_module_add_func(m, f);
    return m;
}

int main(void) {
    warning_init();

    // -------------------------------------------------------------------------
    // 1) مسار صحيح: برنامج حقيقي → مُحسِّن → SSA Verify
    // -------------------------------------------------------------------------
    const char* input_path = "tests/ir_test.baa";
    char* source = read_file(input_path);
    error_init(source);

    Lexer lexer;
    lexer_init(&lexer, source, input_path);
    Node* ast = parse(&lexer);

    int ok = 1;

    ok &= require(!error_has_occurred(), "أخطاء محلل أثناء التحليل");
    ok &= require(analyze(ast), "أخطاء تحليل دلالي");

    IRModule* module = ir_lower_program(ast, input_path);
    ok &= require(module != NULL, "فشل خفض IR");

    if (module) {
        (void)ir_optimizer_run(module, OPT_LEVEL_1);
        ok &= require(ir_module_verify_ir(module, stderr), "فشل التحقق من سلامة IR لمسار صحيح");
        ok &= require(ir_module_verify_ssa(module, stderr), "فشل التحقق من SSA لمسار صحيح");
        ir_module_free(module);
    }

    free(source);

    // -------------------------------------------------------------------------
    // 2) حالات خاطئة مُصنَّعة
    // -------------------------------------------------------------------------
    IRModule* bad1 = build_bad_module_duplicate_def();
    ok &= require(bad1 != NULL, "فشل بناء IR خاطئ (تعريف مكرر)");
    if (bad1) {
        ok &= require(!ir_module_verify_ssa(bad1, stderr), "المتحقق قبل IR بتعريف مكرر (يجب أن يفشل)");
        ir_module_free(bad1);
    }

    IRModule* bad2 = build_bad_module_phi_missing_incoming();
    ok &= require(bad2 != NULL, "فشل بناء IR خاطئ (فاي ناقص)");
    if (bad2) {
        ok &= require(!ir_module_verify_ssa(bad2, stderr), "المتحقق قبل IR بفاي ناقص (يجب أن يفشل)");
        ir_module_free(bad2);
    }

    if (!ok) {
        return 1;
    }

    fprintf(stderr, "ir_verify_ssa_test: PASS\n");
    return 0;
}

