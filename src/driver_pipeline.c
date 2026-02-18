/**
 * @file driver_pipeline.c
 * @brief تنفيذ خط أنابيب الترجمة لكل ملف مصدر.
 * @version 0.3.4
 */

#include "driver_pipeline.h"

#include "baa.h"
#include "driver_time.h"
#include "driver_toolchain.h"

#include "emit.h"
#include "ir_arena.h"
#include "ir_lower.h"
#include "ir_optimizer.h"
#include "ir_outssa.h"
#include "ir_unroll.h"
#include "ir_verify_ir.h"
#include "ir_verify_ssa.h"
#include "isel.h"
#include "regalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// أدوات ملفات بسيطة
// ============================================================================

static char *change_extension_alloc(const char *filename, const char *new_ext)
{
    if (!filename || !new_ext)
    {
        fprintf(stderr, "خطأ: change_extension استلم مُدخلات فارغة (NULL)\n");
        exit(1);
    }

    char *dot = strrchr(filename, '.');
    size_t base_len = 0;
    if (!dot || dot == filename)
    {
        // لا يوجد امتداد (أو ملف مخفي يبدأ بنقطة) → اعتبر الاسم كاملاً كقاعدة
        base_len = strlen(filename);
    }
    else
    {
        base_len = (size_t)(dot - filename);
    }

    size_t ext_len = strlen(new_ext);
    char *new_name = (char *)malloc(base_len + ext_len + 1);
    if (!new_name)
    {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    memcpy(new_name, filename, base_len);
    memcpy(new_name + base_len, new_ext, ext_len + 1);
    return new_name;
}

// ============================================================================
// خط الأنابيب لكل ملف
// ============================================================================

static int compile_one_ir(const CompilerConfig *config,
                         int input_count,
                         const char *current_input,
                         CompilerPhaseTimes *phase_times,
                         char **out_obj_file)
{
    if (out_obj_file) *out_obj_file = NULL;

    if (config->verbose)
        printf("\n[INFO] Processing %s...\n", current_input);

    double t0 = 0.0;
    if (config->time_phases) t0 = driver_time_seconds();
    char *source = read_file(current_input);
    if (config->time_phases) phase_times->read_file_s += (driver_time_seconds() - t0);

    if (config->time_phases) t0 = driver_time_seconds();
    Lexer lexer;
    lexer_init(&lexer, source, current_input);
    Node *ast = parse(&lexer);
    if (config->time_phases) phase_times->parse_s += (driver_time_seconds() - t0);

    if (error_has_occurred())
    {
        fprintf(stderr, "Aborting %s due to syntax errors.\n", current_input);
        free(source);
        return 1;
    }

    if (config->verbose)
        printf("[INFO] Running semantic analysis...\n");
    if (config->time_phases) t0 = driver_time_seconds();
    if (!analyze(ast))
    {
        fprintf(stderr, "Aborting %s due to semantic errors.\n", current_input);
        free(source);
        return 1;
    }
    if (config->time_phases) phase_times->analyze_s += (driver_time_seconds() - t0);

    if (g_warning_config.warnings_as_errors && warning_has_occurred())
    {
        fprintf(stderr, "Aborting %s: warnings treated as errors (-Werror).\n", current_input);
        free(source);
        return 1;
    }

    if (config->time_phases) t0 = driver_time_seconds();
    IRModule *ir_module = ir_lower_program(ast, current_input);
    if (config->time_phases) phase_times->lower_ir_s += (driver_time_seconds() - t0);
    if (!ir_module)
    {
        fprintf(stderr, "Aborting %s: internal IR lowering failure.\n", current_input);
        free(source);
        return 1;
    }

    if (config->dump_ir)
    {
        if (config->verbose)
            printf("[INFO] Dumping IR (--dump-ir)...\n");
        ir_module_print(ir_module, stdout, 1);
    }

    if (config->emit_ir)
    {
        char *ir_file = change_extension_alloc(current_input, ".ir");
        if (config->verbose)
            printf("[INFO] Writing IR (--emit-ir): %s\n", ir_file);
        ir_module_dump(ir_module, ir_file, 1);
        free(ir_file);
    }

    if (config->verify_gate && config->opt_level == OPT_LEVEL_0)
    {
        fprintf(stderr,
                "خطأ: --verify-gate يتطلب -O1 أو -O2 لأن بوابة التحقق تعمل داخل المُحسِّن.\n");
        ir_module_free(ir_module);
        free(source);
        return 1;
    }

    if (config->opt_level > OPT_LEVEL_0)
    {
        if (config->verbose)
            printf("[INFO] Running optimizer (-%s)...\n", ir_optimizer_level_name(config->opt_level));

        if (config->time_phases) t0 = driver_time_seconds();
        if (config->verify_gate) ir_optimizer_set_verify_gate(1);

        if (!ir_optimizer_run(ir_module, config->opt_level))
        {
            fprintf(stderr, "Aborting %s: optimizer failed.\n", current_input);
            if (config->verify_gate)
                fprintf(stderr, "ملاحظة: قد يكون سبب الفشل هو بوابة التحقق (--verify-gate).\n");
            if (config->verify_gate) ir_optimizer_set_verify_gate(0);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        if (config->verify_gate) ir_optimizer_set_verify_gate(0);
        if (config->time_phases) phase_times->optimize_s += (driver_time_seconds() - t0);
    }

    if (config->dump_ir_opt)
    {
        if (config->verbose)
            printf("[INFO] Dumping optimized IR (--dump-ir-opt)...\n");
        ir_module_print(ir_module, stdout, 1);
    }

    if (config->verify_ir)
    {
        if (config->verbose)
            printf("[INFO] Verifying IR well-formedness (--verify-ir)...\n");

        if (config->time_phases) t0 = driver_time_seconds();
        if (!ir_module_verify_ir(ir_module, stderr))
        {
            fprintf(stderr, "فشل التحقق من سلامة الـ IR.\n");
            ir_module_free(ir_module);
            free(source);
            return 1;
        }
        if (config->time_phases) phase_times->verify_ir_s += (driver_time_seconds() - t0);
    }

    if (config->verify_ssa)
    {
        if (config->opt_level == OPT_LEVEL_0)
        {
            fprintf(stderr,
                    "خطأ: --verify-ssa يتطلب -O1 أو -O2 لأن SSA يُبنى عبر Mem2Reg داخل المُحسِّن.\n");
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        if (config->verbose)
            printf("[INFO] Verifying SSA (--verify-ssa)...\n");

        if (config->time_phases) t0 = driver_time_seconds();
        if (!ir_module_verify_ssa(ir_module, stderr))
        {
            fprintf(stderr, "فشل التحقق من SSA.\n");
            ir_module_free(ir_module);
            free(source);
            return 1;
        }
        if (config->time_phases) phase_times->verify_ssa_s += (driver_time_seconds() - t0);
    }

    if (config->time_phases) t0 = driver_time_seconds();
    (void)ir_outssa_run(ir_module);
    if (config->time_phases) phase_times->outssa_s += (driver_time_seconds() - t0);

    if (config->funroll_loops)
    {
        if (config->verbose)
            printf("[INFO] Unrolling loops (-funroll-loops)...\n");
        if (config->time_phases) t0 = driver_time_seconds();
        (void)ir_unroll_run(ir_module, 8);
        if (config->time_phases) phase_times->unroll_s += (driver_time_seconds() - t0);

        if (config->verify_ir)
        {
            if (config->verbose)
                printf("[INFO] Re-verifying IR after unrolling (--verify-ir)...\n");
            if (!ir_module_verify_ir(ir_module, stderr))
            {
                fprintf(stderr, "فشل التحقق من سلامة الـ IR بعد فك الحلقات.\n");
                ir_module_free(ir_module);
                free(source);
                return 1;
            }
        }
    }

    if (config->verbose)
        printf("[INFO] Running instruction selection...\n");
    bool enable_tco = (config->opt_level >= OPT_LEVEL_2);
    if (config->time_phases) t0 = driver_time_seconds();
    MachineModule *mach_module = isel_run_ex(ir_module, enable_tco, config->target);
    if (config->time_phases) phase_times->isel_s += (driver_time_seconds() - t0);
    if (!mach_module)
    {
        fprintf(stderr, "Aborting %s: instruction selection failed.\n", current_input);
        ir_module_free(ir_module);
        free(source);
        return 1;
    }

    if (config->verbose)
        printf("[INFO] Running register allocation...\n");
    if (config->time_phases) t0 = driver_time_seconds();
    if (!regalloc_run_ex(mach_module, config->target))
    {
        fprintf(stderr, "Aborting %s: register allocation failed.\n", current_input);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        free(source);
        return 1;
    }
    if (config->time_phases) phase_times->regalloc_s += (driver_time_seconds() - t0);

    char *asm_file;
    if (config->assembly_only && input_count == 1 && config->output_file)
        asm_file = config->output_file;
    else
        asm_file = change_extension_alloc(current_input, ".s");

    FILE *f_asm = fopen(asm_file, "w");
    if (!f_asm)
    {
        printf("Error: Could not write assembly file '%s'\n", asm_file);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        free(source);
        if (asm_file != config->output_file) free(asm_file);
        return 1;
    }

    if (config->verbose)
        printf("[INFO] Emitting assembly: %s\n", asm_file);

    // حماية المكدس حالياً مدعومة فقط على ELF/Linux
    if (config->codegen_opts.stack_protector != BAA_STACKPROT_OFF)
    {
        if (!config->target || config->target->obj_format != BAA_OBJFORMAT_ELF)
        {
            fprintf(stderr, "خطأ: -fstack-protector مدعوم حالياً فقط لهدف ELF/Linux.\n");
            fclose(f_asm);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            if (asm_file != config->output_file) free(asm_file);
            return 1;
        }
    }

    if (config->time_phases) t0 = driver_time_seconds();
    if (!emit_module_ex2(mach_module, f_asm, config->debug_info, config->target, config->codegen_opts))
    {
        fprintf(stderr, "Aborting %s: code emission failed.\n", current_input);
        fclose(f_asm);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        free(source);
        if (asm_file != config->output_file) free(asm_file);
        return 1;
    }
    if (config->time_phases) phase_times->emit_s += (driver_time_seconds() - t0);
    fclose(f_asm);

    mach_module_free(mach_module);

    if (config->time_phases)
    {
        IRArenaStats s = {0};
        ir_arena_get_stats(&ir_module->arena, &s);
        if (s.used_bytes > phase_times->ir_arena_used_max) phase_times->ir_arena_used_max = s.used_bytes;
        if (s.cap_bytes > phase_times->ir_arena_cap_max) phase_times->ir_arena_cap_max = s.cap_bytes;
        if (s.chunks > phase_times->ir_arena_chunks_max) phase_times->ir_arena_chunks_max = s.chunks;
    }

    ir_module_free(ir_module);
    free(source);

    if (config->assembly_only)
    {
        if (config->verbose)
            printf("[INFO] Generated assembly: %s\n", asm_file);
        if (asm_file != config->output_file) free(asm_file);
        return 0;
    }

    char *obj_file;
    if (config->compile_only && input_count == 1 && config->output_file)
        obj_file = config->output_file;
    else
        obj_file = change_extension_alloc(current_input, ".o");

    if (driver_toolchain_assemble_one(config, phase_times, asm_file, obj_file) != 0)
    {
        if (asm_file != config->output_file) free(asm_file);
        if (obj_file != config->output_file) free(obj_file);
        return 1;
    }

    if (!config->verbose)
    {
        remove(asm_file);
    }
    if (asm_file != config->output_file) free(asm_file);

    if (out_obj_file) *out_obj_file = obj_file;
    return 0;
}

int driver_compile_files(const CompilerConfig *config,
                         char **input_files,
                         int input_count,
                         CompilerPhaseTimes *phase_times,
                         char ***out_obj_files,
                         int *out_obj_count)
{
    if (out_obj_files) *out_obj_files = NULL;
    if (out_obj_count) *out_obj_count = 0;
    if (!config || !input_files || input_count <= 0 || !phase_times) return 1;

    // عند -S لا نحتاج لإرجاع قائمة كائنات.
    int cap = config->assembly_only ? 0 : input_count;
    char **obj_files = NULL;
    int obj_count = 0;
    if (cap > 0)
    {
        obj_files = (char **)calloc((size_t)cap, sizeof(char *));
        if (!obj_files)
        {
            fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
            return 1;
        }
    }

    for (int i = 0; i < input_count; i++)
    {
        const char *current_input = input_files[i];
        char *obj_file = NULL;

        int rc = compile_one_ir(config, input_count, current_input, phase_times, &obj_file);

        if (rc != 0)
        {
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            if (out_obj_files) *out_obj_files = NULL;
            if (out_obj_count) *out_obj_count = 0;
            return 1;
        }

        if (!config->assembly_only)
        {
            obj_files[obj_count++] = obj_file;
        }
    }

    if (out_obj_files) *out_obj_files = obj_files;
    if (out_obj_count) *out_obj_count = obj_count;
    return 0;
}

void driver_free_obj_files(char **obj_files, int obj_count, const char *output_file)
{
    if (!obj_files) return;
    for (int i = 0; i < obj_count; i++)
    {
        if (obj_files[i] && obj_files[i] != output_file)
            free(obj_files[i]);
    }
    free(obj_files);
}
