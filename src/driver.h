/**
 * @file driver.h
 * @brief تعريفات واجهة الـ Driver (CLI) المشتركة بين ملفات التنفيذ.
 * @version 0.3.3
 */

#ifndef BAA_DRIVER_H
#define BAA_DRIVER_H

#include <stdbool.h>
#include <stddef.h>

#include "code_model.h"
#include "ir_optimizer.h"
#include "target.h"

typedef struct
{
    char *output_file;  // ملف الخرج (.exe, .o, .s)
    bool assembly_only; // -S: إنتاج كود تجميع فقط
    bool compile_only;  // -c: تجميع إلى كائن فقط (بدون ربط)
    bool verbose;       // -v: وضع التفاصيل
    bool dump_ir;       // --dump-ir: طباعة IR بعد التحليل الدلالي
    bool emit_ir;       // --emit-ir: كتابة IR إلى ملف .ir بجانب المصدر
    bool dump_ir_opt;   // --dump-ir-opt: طباعة IR بعد التحسين
    bool verify_ir;     // --verify-ir: التحقق من سلامة/تشكيل الـ IR
    bool verify_ssa;    // --verify-ssa: التحقق من صحة SSA
    bool verify_gate;   // --verify-gate: بوابة تحقق داخل المُحسِّن
    bool time_phases;   // --time-phases: قياس أزمنة المراحل
    bool debug_info;    // --debug-info: إصدار معلومات ديبغ
    bool custom_startup; // --startup=custom: استخدام نقطة دخول مخصصة (مع الحفاظ على CRT/libc)
    bool funroll_loops; // -funroll-loops
    OptLevel opt_level; // -O0/-O1/-O2
    const BaaTarget* target; // --target=...
    BaaCodegenOptions codegen_opts;
    double start_time;
} CompilerConfig;

typedef struct
{
    double read_file_s;
    double parse_s;
    double analyze_s;
    double lower_ir_s;
    double optimize_s;
    double verify_ir_s;
    double verify_ssa_s;
    double outssa_s;
    double unroll_s;
    double isel_s;
    double regalloc_s;
    double emit_s;
    double assemble_s;
    double link_s;

    size_t ir_arena_used_max;
    size_t ir_arena_cap_max;
    size_t ir_arena_chunks_max;
} CompilerPhaseTimes;

#endif // BAA_DRIVER_H
