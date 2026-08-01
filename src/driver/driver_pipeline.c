/**
 * @file driver_pipeline.c
 * @brief تنفيذ خط أنابيب الترجمة لكل ملف مصدر.
 * @version 0.3.4
 */

#include "driver_internal.h"
#include "driver_artifacts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief إلحاق نص بنهاية ملف (append).
 */
static int driver_append_text_file(const char* path, const char* text)
{
    if (!path || !text) return 1;
    FILE* f = baa_fopen_utf8(path, "ab");
    if (!f) return 1;
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, f);
    fclose(f);
    return (w == n) ? 0 : 1;
}

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

    const char *dot = strrchr(filename, '.');
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

static void driver_free_if_owned(char* ptr, const char* borrowed_ptr)
{
    if (ptr && ptr != borrowed_ptr) free(ptr);
}

static char* driver_strdup_alloc(const char* text)
{
    if (!text) return NULL;
    size_t n = strlen(text);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, text, n + 1);
    return out;
}

#include "driver_pipeline_validation.inc"

static BaaCompilerExitCode compile_one_ir(const CompilerConfig *config,
                                          int input_count,
                                          const char *current_input,
                                          CompilerPhaseTimes *phase_times,
                                          DriverBuildManifest *build_manifest,
                                          DriverOneDefinitionRegistry *odr_registry,
                                          char **out_obj_file,
                                          char **out_nazm_shadow_object)
{
    if (out_obj_file) *out_obj_file = NULL;
    if (out_nazm_shadow_object) *out_nazm_shadow_object = NULL;

    if (config->verbose)
        printf("\n[INFO] Processing %s...\n", current_input);

    char *early_obj_file = NULL;
    if (!config->assembly_only && !config->emit_nazm &&
        !config->check_only && !config->header_check)
    {
        if (config->compile_only)
        {
            if (input_count == 1 && config->output_file)
                early_obj_file = config->output_file;
            else
                early_obj_file = change_extension_alloc(current_input, ".o");
        }
        else
        {
            const char* artifact_base =
                config->output_file ? config->output_file : current_input;
            early_obj_file =
                driver_make_temp_artifact_path(artifact_base, "obj", ".o");
            if (!early_obj_file)
            {
                fprintf(stderr, "خطأ: فشل إنشاء مسار أثر مؤقت لملف الكائن.\n");
                return BAA_COMPILER_EXIT_INTERNAL_ERROR;
            }
        }

        if (driver_build_try_reuse_object(config, current_input, early_obj_file, build_manifest))
        {
            if (config->verbose)
                printf("[INFO] Reused cached object: %s\n", current_input);
            if (out_obj_file) *out_obj_file = early_obj_file;
            return BAA_COMPILER_EXIT_SUCCESS;
        }
    }

    double t0 = 0.0;
    if (config->time_phases) t0 = driver_time_seconds();
    char *source = config->source_stdin_file
        ? read_stdin_source()
        : read_file(current_input);
    if (config->time_phases) phase_times->read_file_s += (driver_time_seconds() - t0);
    if (!source)
    {
        fprintf(stderr, "خطأ: تعذر قراءة مصدر باء من الدخل القياسي.\n");
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
    if (config->source_stdin_file &&
        !driver_validate_stdin_utf8(source, current_input))
    {
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_SOURCE_ERROR;
    }

    if (config->time_phases) t0 = driver_time_seconds();
    Lexer lexer;
    lexer_init(&lexer, source, current_input, config->include_dirs, config->include_dir_count);
    Node *ast = parse(&lexer);
    size_t lexer_dep_count = 0;
    const char* const* lexer_deps = lexer_get_dependencies(&lexer, &lexer_dep_count);
    if (config->time_phases) phase_times->parse_s += (driver_time_seconds() - t0);

    if (error_has_occurred())
    {
        if (config->semantic_query_json && ast &&
            driver_semantic_query_json_write(stdout,
                                             BAA_VERSION,
                                             current_input,
                                             source,
                                             ast,
                                             config->semantic_query_byte))
        {
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file)
                free(early_obj_file);
            return BAA_COMPILER_EXIT_SUCCESS;
        }
        fprintf(stderr, "Aborting %s due to syntax errors.\n", current_input);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_SOURCE_ERROR;
    }

    if (config->verbose)
        printf("[INFO] Running semantic analysis...\n");
    if (config->time_phases) t0 = driver_time_seconds();
    if (!analyze(ast))
    {
        if (config->semantic_query_json &&
            driver_semantic_query_json_write(stdout,
                                             BAA_VERSION,
                                             current_input,
                                             source,
                                             ast,
                                             config->semantic_query_byte))
        {
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file)
                free(early_obj_file);
            return BAA_COMPILER_EXIT_SUCCESS;
        }
        fprintf(stderr, "Aborting %s due to semantic errors.\n", current_input);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_SOURCE_ERROR;
    }
    if (config->time_phases) phase_times->analyze_s += (driver_time_seconds() - t0);

    if (g_warning_config.warnings_as_errors && warning_has_occurred())
    {
        fprintf(stderr, "Aborting %s: warnings treated as errors (-Werror).\n", current_input);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_SOURCE_ERROR;
    }

    if (config->dump_symbols_json)
    {
        if (!driver_symbols_json_write(stdout,
                                       BAA_VERSION,
                                       current_input,
                                       source,
                                       ast))
        {
            fprintf(stderr, "خطأ: فشل إصدار symbols-json-v1.\n");
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }
    }

    if (config->semantic_query_json)
    {
        if (!driver_semantic_query_json_write(stdout,
                                              BAA_VERSION,
                                              current_input,
                                              source,
                                              ast,
                                              config->semantic_query_byte))
        {
            fprintf(stderr, "خطأ: فشل إصدار semantic-query-json-v1.\n");
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file)
                free(early_obj_file);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }
    }

    if (config->semantic_index_json)
    {
        if (!driver_semantic_index_json_write(stdout,
                                              BAA_VERSION,
                                              current_input,
                                              source,
                                              ast))
        {
            fprintf(stderr, "خطأ: فشل إصدار semantic-index-json-v1.\n");
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file)
                free(early_obj_file);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }
    }

    if (config->check_only || config->header_check)
    {
        const char* reason = config->check_only ? "check-only" : "header-check";
        if (!driver_build_record_uncached(config,
                                          current_input,
                                          NULL,
                                          lexer_deps,
                                          lexer_dep_count,
                                          reason,
                                          build_manifest))
        {
            fprintf(stderr, "خطأ: فشل تحديث بيان فحص المصدر.\n");
            lexer_free_dependencies(&lexer);
            free(source);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }
        lexer_free_dependencies(&lexer);
        free(source);
        return BAA_COMPILER_EXIT_SUCCESS;
    }

    if (config->time_phases) t0 = driver_time_seconds();
    IRModule *ir_module = ir_lower_program(ast, current_input, config->runtime_check_mask, config->target);
    if (config->time_phases) phase_times->lower_ir_s += (driver_time_seconds() - t0);
    if (!ir_module)
    {
        fprintf(stderr, "Aborting %s: internal IR lowering failure.\n", current_input);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    if (!driver_record_exported_definitions(odr_registry, ir_module, current_input))
    {
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_SOURCE_ERROR;
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
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INVALID_INVOCATION;
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
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
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
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
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
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
            return BAA_COMPILER_EXIT_INVALID_INVOCATION;
        }

        if (config->verbose)
            printf("[INFO] Verifying SSA (--verify-ssa)...\n");

        if (config->time_phases) t0 = driver_time_seconds();
        if (!ir_module_verify_ssa(ir_module, stderr))
        {
            fprintf(stderr, "فشل التحقق من SSA.\n");
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }
        if (config->time_phases) phase_times->verify_ssa_s += (driver_time_seconds() - t0);
    }

    if (config->time_phases) t0 = driver_time_seconds();
    bool outssa_changed = false;
    if (!ir_outssa_run_ex(ir_module, &outssa_changed))
    {
        fprintf(stderr, "فشل تمريرة الخروج من SSA.\n");
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }
    (void)outssa_changed;
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
                lexer_free_dependencies(&lexer);
                free(source);
                if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
                return BAA_COMPILER_EXIT_INTERNAL_ERROR;
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
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    if (config->verbose)
        printf("[INFO] Running register allocation...\n");
    if (config->time_phases) t0 = driver_time_seconds();
    if (!regalloc_run_ex(mach_module, config->target))
    {
        fprintf(stderr, "Aborting %s: register allocation failed.\n", current_input);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }
    if (config->time_phases) phase_times->regalloc_s += (driver_time_seconds() - t0);

    if (config->emit_nazm ||
        (config->assembly_only && config->assembler == BAA_ASSEMBLER_NAZM))
    {
        char *nazm_output = NULL;
        if (input_count == 1 && config->output_file)
            nazm_output = config->output_file;
        else
            nazm_output = change_extension_alloc(current_input, ".نظم");

        if (!nazm_output)
        {
            fprintf(stderr, "خطأ: تعذر تحديد مسار خرج نظم.\n");
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }

        if (config->time_phases) t0 = driver_time_seconds();
        BaaCompilerExitCode nazm_rc =
            driver_emit_nazm_source(config, mach_module, nazm_output);
        if (config->time_phases) phase_times->emit_s += (driver_time_seconds() - t0);

        if (nazm_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            driver_free_if_owned(nazm_output, config->output_file);
            return nazm_rc;
        }

        bool manifest_ok = driver_build_record_uncached(config,
                                                         current_input,
                                                         nazm_output,
                                                         lexer_deps,
                                                         lexer_dep_count,
                                                         config->assembly_only
                                                            ? "assembly-only-nazm"
                                                            : "nazm-source",
                                                         build_manifest);
        if (config->verbose)
            printf("[INFO] Generated Nazm source: %s\n", nazm_output);

        mach_module_free(mach_module);
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        driver_free_if_owned(nazm_output, config->output_file);
        return manifest_ok
            ? BAA_COMPILER_EXIT_SUCCESS
            : BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    if (config->nazm_shadow_executable)
    {
        BaaCompilerExitCode shadow_rc = driver_emit_nazm_shadow_object(
            config, mach_module, out_nazm_shadow_object);
        if (shadow_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            return shadow_rc;
        }
    }

    if (config->assembler == BAA_ASSEMBLER_NAZM)
    {
        char *obj_file = early_obj_file;
        char *nazm_source =
            driver_make_temp_artifact_path(obj_file, "nazm", ".نظم");
        if (!nazm_source || !obj_file)
        {
            fprintf(stderr, "خطأ: تعذر تحديد مسار مصدر/كائن نظم المؤقت.\n");
            free(nazm_source);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            if (early_obj_file && early_obj_file != config->output_file)
                free(early_obj_file);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }

        BaaCompilerExitCode assemble_rc = driver_assemble_nazm_module(
            config,
            phase_times,
            mach_module,
            nazm_source,
            obj_file,
            config->verbose);
        if (assemble_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            free(nazm_source);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            if (obj_file != config->output_file) free(obj_file);
            return assemble_rc;
        }

        if (config->verbose)
        {
            printf("[INFO] Assembled with Nazm: %s\n", obj_file);
            printf("[INFO] Retained Nazm source: %s\n", nazm_source);
        }
        free(nazm_source);
        mach_module_free(mach_module);

        if (config->time_phases)
        {
            IRArenaStats s = {0};
            ir_arena_get_stats(&ir_module->arena, &s);
            if (s.used_bytes > phase_times->ir_arena_used_max)
                phase_times->ir_arena_used_max = s.used_bytes;
            if (s.cap_bytes > phase_times->ir_arena_cap_max)
                phase_times->ir_arena_cap_max = s.cap_bytes;
            if (s.chunks > phase_times->ir_arena_chunks_max)
                phase_times->ir_arena_chunks_max = s.chunks;
        }

        ir_module_free(ir_module);
        const char* const* build_deps = lexer_deps;
        size_t build_dep_count = lexer_dep_count;
        free(source);

        if (!driver_build_update_cache(config,
                                       current_input,
                                       obj_file,
                                       build_deps,
                                       build_dep_count,
                                       build_manifest))
        {
            fprintf(stderr, "خطأ: فشل تحديث بيان/كاش البناء.\n");
            if (obj_file != config->output_file) free(obj_file);
            lexer_free_dependencies(&lexer);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }

        lexer_free_dependencies(&lexer);
        if (out_obj_file) *out_obj_file = obj_file;
        return BAA_COMPILER_EXIT_SUCCESS;
    }

    char* final_asm_output = NULL;
    if (config->assembly_only)
    {
        if (input_count == 1 && config->output_file)
            final_asm_output = config->output_file;
        else
            final_asm_output = change_extension_alloc(current_input, ".s");
    }

    char* asm_file = config->assembly_only
        ? driver_strdup_alloc(final_asm_output)
        : driver_make_temp_artifact_path(early_obj_file, "asm", ".s");
    if (!asm_file)
    {
        fprintf(stderr, "خطأ: فشل تحديد مسار ملف التجميع.\n");
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        driver_free_if_owned(final_asm_output, config->output_file);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    FILE *f_asm = baa_fopen_utf8(asm_file, "w");
    if (!f_asm)
    {
        fprintf(stderr, "خطأ: تعذرت كتابة ملف التجميع '%s'.\n", asm_file);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        free(asm_file);
        driver_free_if_owned(final_asm_output, config->output_file);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
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
            if (config->assembly_only)
                (void)driver_toolchain_delete_file_utf8(asm_file);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            lexer_free_dependencies(&lexer);
            free(source);
            free(asm_file);
            driver_free_if_owned(final_asm_output, config->output_file);
            if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
            return BAA_COMPILER_EXIT_UNSUPPORTED;
        }
    }

    if (config->time_phases) t0 = driver_time_seconds();
    if (!emit_module_ex2(mach_module, f_asm, config->debug_info, config->target, config->codegen_opts))
    {
        fprintf(stderr, "Aborting %s: code emission failed.\n", current_input);
        fclose(f_asm);
        if (config->assembly_only)
            (void)driver_toolchain_delete_file_utf8(asm_file);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        free(asm_file);
        driver_free_if_owned(final_asm_output, config->output_file);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
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
    const char* const* build_deps = lexer_deps;
    size_t build_dep_count = lexer_dep_count;
    free(source);

    if (config->assembly_only)
    {
        // عند طلب -S مع --startup=custom وفي ملف واحد، نلحق `الرئيسية_بدء` بنفس ملف assembly
        // حتى يتمكن المستخدم من ربطه يدوياً أو فحصه (ويُستخدم أيضاً لاختبارات asm-only).
        if (config->custom_startup && input_count == 1)
        {
            const char* stub = driver_startup_gas_source(config->target);
            if (stub && driver_append_text_file(asm_file, "\n\n") == 0)
            {
                (void)driver_append_text_file(asm_file, stub);
            }
        }

        if (config->verbose)
            printf("[INFO] Generated assembly: %s\n", final_asm_output);
        (void)driver_build_record_uncached(config,
                                           current_input,
                                           final_asm_output,
                                           build_deps,
                                           build_dep_count,
                                           "assembly-only",
                                           build_manifest);
        free(asm_file);
        driver_free_if_owned(final_asm_output, config->output_file);
        lexer_free_dependencies(&lexer);
        return BAA_COMPILER_EXIT_SUCCESS;
    }

    char *obj_file = early_obj_file;

    BaaCompilerExitCode assemble_rc =
        driver_toolchain_assemble_one(config, phase_times, asm_file, obj_file);
    if (assemble_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        if (!config->verbose) (void)driver_toolchain_delete_file_utf8(asm_file);
        free(asm_file);
        if (obj_file != config->output_file) free(obj_file);
        driver_free_if_owned(final_asm_output, config->output_file);
        lexer_free_dependencies(&lexer);
        return assemble_rc;
    }

    if (!config->verbose)
    {
        (void)driver_toolchain_delete_file_utf8(asm_file);
    }
    free(asm_file);
    driver_free_if_owned(final_asm_output, config->output_file);

    if (!driver_build_update_cache(config,
                                   current_input,
                                   obj_file,
                                   build_deps,
                                   build_dep_count,
                                   build_manifest))
    {
        fprintf(stderr, "خطأ: فشل تحديث بيان/كاش البناء.\n");
        if (obj_file != config->output_file) free(obj_file);
        lexer_free_dependencies(&lexer);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    lexer_free_dependencies(&lexer);
    if (out_obj_file) *out_obj_file = obj_file;
    return BAA_COMPILER_EXIT_SUCCESS;
}

BaaCompilerExitCode driver_compile_files(const CompilerConfig *config,
                                         char **input_files,
                                         int input_count,
                                         CompilerPhaseTimes *phase_times,
                                         DriverBuildManifest *build_manifest,
                                         char ***out_obj_files,
                                         int *out_obj_count,
                                         char **out_nazm_shadow_object)
{
    if (out_obj_files) *out_obj_files = NULL;
    if (out_obj_count) *out_obj_count = 0;
    if (out_nazm_shadow_object) *out_nazm_shadow_object = NULL;
    if (!config || !input_files || input_count <= 0 || !phase_times)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    BaaCompilerExitCode input_rc =
        driver_validate_nazm_inputs(config, input_files, input_count);
    if (input_rc != BAA_COMPILER_EXIT_SUCCESS) return input_rc;

    // عند -S لا نحتاج لإرجاع قائمة كائنات.
    // كل ربط تنفيذي إنتاجي يضيف نقطة بدء عربية؛ لا يعتمد ABI على main.
    bool need_startup_obj =
        (!config->assembly_only && !config->emit_nazm && !config->compile_only &&
         !config->check_only && !config->header_check &&
         !(config->target && config->target->obj_format == BAA_OBJFORMAT_COFF));
    int cap = (config->assembly_only || config->emit_nazm ||
               config->check_only || config->header_check) ? 0
        : (input_count + (need_startup_obj ? 1 : 0));
    char **obj_files = NULL;
    int obj_count = 0;
    if (cap > 0)
    {
        obj_files = (char **)calloc((size_t)cap, sizeof(char *));
        if (!obj_files)
        {
            fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }
    }

    DriverOneDefinitionRegistry odr_registry;
    DriverOneDefinitionRegistry* odr_registry_ptr = NULL;
    if (!config->assembly_only && !config->emit_nazm && !config->compile_only &&
        !config->check_only && !config->header_check && input_count > 1)
    {
        driver_odr_registry_init(&odr_registry);
        odr_registry_ptr = &odr_registry;
    }

    for (int i = 0; i < input_count; i++)
    {
        const char *current_input = input_files[i];
        char *obj_file = NULL;
        char *shadow_object = NULL;

        BaaCompilerExitCode rc = driver_nazm_is_source_path(current_input)
            ? driver_compile_nazm_input(config,
                                        input_count,
                                        current_input,
                                        phase_times,
                                        build_manifest,
                                        &obj_file)
            : compile_one_ir(config,
                             input_count,
                             current_input,
                             phase_times,
                             build_manifest,
                             odr_registry_ptr,
                             &obj_file,
                             &shadow_object);

        if (rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            driver_odr_registry_free(odr_registry_ptr);
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            if (out_obj_files) *out_obj_files = NULL;
            if (out_obj_count) *out_obj_count = 0;
            if (shadow_object)
            {
                (void)driver_toolchain_delete_file_utf8(shadow_object);
                free(shadow_object);
            }
            return rc;
        }

        if (shadow_object)
        {
            if (out_nazm_shadow_object && !*out_nazm_shadow_object)
                *out_nazm_shadow_object = shadow_object;
            else
            {
                (void)driver_toolchain_delete_file_utf8(shadow_object);
                free(shadow_object);
                driver_odr_registry_free(odr_registry_ptr);
                driver_free_obj_files(obj_files, obj_count, config->output_file);
                return BAA_COMPILER_EXIT_INTERNAL_ERROR;
            }
        }

        if (!config->assembly_only && !config->emit_nazm &&
            !config->check_only && !config->header_check)
        {
            obj_files[obj_count++] = obj_file;
        }
    }

    // أضف كائن بدء التشغيل في نهاية قائمة الربط.
    if (need_startup_obj)
    {
        char *startup_object = NULL;
        BaaCompilerExitCode startup_assemble_rc =
            driver_build_startup_object(config, phase_times, &startup_object);
        if (startup_assemble_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            driver_odr_registry_free(odr_registry_ptr);
            fprintf(stderr, "خطأ: فشل تجميع كود بدء التشغيل.\n");
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            return startup_assemble_rc;
        }
        obj_files[obj_count++] = startup_object;
    }

    driver_odr_registry_free(odr_registry_ptr);
    if (out_obj_files) *out_obj_files = obj_files;
    if (out_obj_count) *out_obj_count = obj_count;
    return BAA_COMPILER_EXIT_SUCCESS;
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
