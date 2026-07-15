/**
 * @file driver_pipeline.c
 * @brief تنفيذ خط أنابيب الترجمة لكل ملف مصدر.
 * @version 0.3.4
 */

#include "driver_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// بدء تشغيل مخصص (custom startup)
// ============================================================================

#define BAA_CUSTOM_START_SYMBOL "الرئيسية_بدء"

/**
 * @brief الحصول على كود assembly لتعريف نقطة دخول مخصصة.
 *
 * ملاحظة مهمة:
 * - هذا الكود لا يلغي CRT/libc، بل يحافظ على التهيئة الصحيحة:
 *   - Windows/COFF: يستدعي mainCRTStartup (CRT) ثم ExitProcess إن عاد.
 *   - Linux/ELF: يستدعي __libc_start_main لضمان تهيئة libc.
 */
static const char* driver_custom_startup_asm(const BaaTarget* target)
{
    // GAS AT&T syntax
    if (target && target->obj_format == BAA_OBJFORMAT_ELF) {
        return
            ".text\n"
            ".globl " BAA_CUSTOM_START_SYMBOL "\n"
            BAA_CUSTOM_START_SYMBOL ":\n"
            "    movq %rsp, %r10\n"                // stack_end
            "    movq (%rsp), %rsi\n"              // argc
            "    leaq 8(%rsp), %rdx\n"             // argv
            "    leaq الرئيسية(%rip), %rdi\n"     // نقطة دخول البرنامج العربية
            "    xorl %ecx, %ecx\n"                // init = NULL
            "    xorl %r8d, %r8d\n"                // fini = NULL
            "    xorl %r9d, %r9d\n"                // rtld_fini = NULL
            "    subq $8, %rsp\n"                  // محاذاة قبل push
            "    pushq %r10\n"                     // 7th arg: stack_end
            "    call __libc_start_main\n"
            "    hlt\n"
            ".section .note.GNU-stack,\"\",@progbits\n";
    }

    // Windows/COFF (MinGW-w64): runtime bridge decodes UTF-16 argv to UTF-8.
    return
        ".text\n"
        ".globl " BAA_CUSTOM_START_SYMBOL "\n"
        BAA_CUSTOM_START_SYMBOL ":\n"
        "    andq $-16, %rsp\n"
        "    subq $32, %rsp\n"
        "    call بدء_ويندوز\n"
        "    hlt\n";
}

/**
 * @brief كتابة نص إلى ملف (استبدال محتواه).
 */
static int driver_write_text_file(const char* path, const char* text)
{
    if (!path || !text) return 1;
    FILE* f = baa_fopen_utf8(path, "wb");
    if (!f) return 1;
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, f);
    fclose(f);
    return (w == n) ? 0 : 1;
}

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

/**
 * @brief إنشاء مسار مؤقت ASCII داخل مجلد العمل.
 */
static char* driver_make_ascii_temp_path(const char* prefix, const char* ext)
{
    static unsigned long temp_counter = 0;
    if (!prefix || !ext) return NULL;

    unsigned long id = ++temp_counter;
    char tmp_name[256];
    int n = snprintf(tmp_name, sizeof(tmp_name), ".baa_%s_%lu%s", prefix, id, ext);
    if (n <= 0 || (size_t)n >= sizeof(tmp_name)) return NULL;

    size_t need = (size_t)n + 1;
    char* out = (char*)malloc(need);
    if (!out) return NULL;
    memcpy(out, tmp_name, need);
    return out;
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

// ============================================================================
// خط الأنابيب لكل ملف
// ============================================================================

typedef struct
{
    char* name;
    char* source;
    const char* kind;
} DriverOneDefinitionSymbol;

typedef struct
{
    DriverOneDefinitionSymbol* symbols;
    size_t count;
    size_t capacity;
} DriverOneDefinitionRegistry;

static void driver_odr_registry_init(DriverOneDefinitionRegistry* registry)
{
    if (!registry) return;
    registry->symbols = NULL;
    registry->count = 0;
    registry->capacity = 0;
}

static void driver_odr_registry_free(DriverOneDefinitionRegistry* registry)
{
    if (!registry) return;
    for (size_t i = 0; i < registry->count; ++i)
    {
        free(registry->symbols[i].name);
        free(registry->symbols[i].source);
    }
    free(registry->symbols);
    registry->symbols = NULL;
    registry->count = 0;
    registry->capacity = 0;
}

static bool driver_odr_registry_reserve(DriverOneDefinitionRegistry* registry)
{
    if (!registry) return false;
    if (registry->count < registry->capacity) return true;

    size_t new_capacity = registry->capacity ? registry->capacity * 2u : 32u;
    DriverOneDefinitionSymbol* grown =
        (DriverOneDefinitionSymbol*)realloc(registry->symbols,
                                            new_capacity * sizeof(*grown));
    if (!grown) return false;

    registry->symbols = grown;
    registry->capacity = new_capacity;
    return true;
}

static bool driver_odr_record_symbol(DriverOneDefinitionRegistry* registry,
                                     const char* name,
                                     const char* source,
                                     const char* kind)
{
    if (!registry || !name || !name[0]) return true;
    if (!source) source = "<unknown>";
    if (!kind) kind = "رمز";

    for (size_t i = 0; i < registry->count; ++i)
    {
        DriverOneDefinitionSymbol* prev = &registry->symbols[i];
        if (prev->name && strcmp(prev->name, name) == 0)
        {
            fprintf(stderr, "خطأ: تعريف متعدد للرمز العام '%s'.\n", name);
            fprintf(stderr, "  التعريف الأول: %s في %s\n",
                    prev->kind ? prev->kind : "رمز",
                    prev->source ? prev->source : "<unknown>");
            fprintf(stderr, "  التعريف الثاني: %s في %s\n", kind, source);
            fprintf(stderr,
                    "مساعدة: اجعل أحد التعريفين تصريحاً 'خارجي' في ملف .baahd، "
                    "أو استخدم 'ساكن' للرموز المحلية.\n");
            return false;
        }
    }

    if (!driver_odr_registry_reserve(registry))
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة أثناء فحص التعريفات العامة المتعددة.\n");
        return false;
    }

    DriverOneDefinitionSymbol* slot = &registry->symbols[registry->count++];
    slot->name = driver_strdup_alloc(name);
    slot->source = driver_strdup_alloc(source);
    slot->kind = kind;
    if (!slot->name || !slot->source)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة أثناء تسجيل تعريف عام.\n");
        return false;
    }
    return true;
}

static bool driver_record_exported_definitions(DriverOneDefinitionRegistry* registry,
                                               IRModule* module,
                                               const char* source)
{
    if (!registry || !module) return true;

    for (IRFunc* fn = module->funcs; fn; fn = fn->next)
    {
        if (!fn->is_prototype)
        {
            if (!driver_odr_record_symbol(registry, fn->name, source, "دالة"))
                return false;
        }
    }

    for (IRGlobal* global = module->globals; global; global = global->next)
    {
        if (!global->is_extern && !global->is_internal)
        {
            if (!driver_odr_record_symbol(registry, global->name, source, "متغير عام"))
                return false;
        }
    }

    return true;
}

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
            early_obj_file = driver_make_ascii_temp_path("obj", ".o");
            if (!early_obj_file)
            {
                fprintf(stderr, "خطأ: فشل إنشاء مسار مؤقت ASCII لملف الكائن.\n");
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
    char *source = read_file(current_input);
    if (config->time_phases) phase_times->read_file_s += (driver_time_seconds() - t0);

    if (config->time_phases) t0 = driver_time_seconds();
    Lexer lexer;
    lexer_init(&lexer, source, current_input, config->include_dirs, config->include_dir_count);
    Node *ast = parse(&lexer);
    size_t lexer_dep_count = 0;
    const char* const* lexer_deps = lexer_get_dependencies(&lexer, &lexer_dep_count);
    if (config->time_phases) phase_times->parse_s += (driver_time_seconds() - t0);

    if (error_has_occurred())
    {
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

    if (config->emit_nazm)
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
                                                         "nazm-source",
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

    char* final_asm_output = NULL;
    if (config->assembly_only)
    {
        if (input_count == 1 && config->output_file)
            final_asm_output = config->output_file;
        else
            final_asm_output = change_extension_alloc(current_input, ".s");
    }

    char* asm_file = driver_make_ascii_temp_path("asm", ".s");
    if (!asm_file)
    {
        fprintf(stderr, "خطأ: فشل إنشاء مسار مؤقت ASCII لملف التجميع.\n");
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
        printf("Error: Could not write assembly file '%s'\n", asm_file);
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        lexer_free_dependencies(&lexer);
        free(source);
        free(asm_file);
        driver_free_if_owned(final_asm_output, config->output_file);
        if (early_obj_file && early_obj_file != config->output_file) free(early_obj_file);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
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
            const char* stub = driver_custom_startup_asm(config->target);
            if (stub && driver_append_text_file(asm_file, "\n\n") == 0)
            {
                (void)driver_append_text_file(asm_file, stub);
            }
        }

        if (!final_asm_output) {
            fprintf(stderr, "خطأ: مسار خرج التجميع غير صالح.\n");
            free(asm_file);
            lexer_free_dependencies(&lexer);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }

        if (!driver_toolchain_copy_file_utf8(asm_file, final_asm_output))
        {
            fprintf(stderr, "خطأ: فشل نسخ ملف التجميع إلى المسار الهدف '%s'.\n", final_asm_output);
            free(asm_file);
            driver_free_if_owned(final_asm_output, config->output_file);
            lexer_free_dependencies(&lexer);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }

        if (config->verbose)
            printf("[INFO] Generated assembly: %s\n", final_asm_output);
        if (!config->verbose)
            (void)driver_toolchain_delete_file_utf8(asm_file);
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

        BaaCompilerExitCode rc = compile_one_ir(config, input_count, current_input, phase_times,
                                                build_manifest, odr_registry_ptr, &obj_file,
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
        const char* asm_path = ".baa_startup_tmp.s";
        const char* obj_path = ".baa_startup_tmp.o";

        const char* stub = driver_custom_startup_asm(config->target);
        if (!stub)
        {
            driver_odr_registry_free(odr_registry_ptr);
            fprintf(stderr, "خطأ: فشل توليد كود بدء التشغيل.\n");
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }

        if (driver_write_text_file(asm_path, stub) != 0)
        {
            driver_odr_registry_free(odr_registry_ptr);
            fprintf(stderr, "خطأ: فشل كتابة ملف بدء التشغيل '%s'.\n", asm_path);
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }

        // نجمعه إلى كائن ثم نضيفه لقائمة الربط.
        BaaCompilerExitCode startup_assemble_rc =
            driver_toolchain_assemble_one(config, phase_times, asm_path, obj_path);
        if (startup_assemble_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            driver_odr_registry_free(odr_registry_ptr);
            fprintf(stderr, "خطأ: فشل تجميع كود بدء التشغيل.\n");
            if (!config->verbose) remove(asm_path);
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            return startup_assemble_rc;
        }

        if (!config->verbose)
        {
            remove(asm_path);
        }

        char* obj_dup = driver_strdup_alloc(obj_path);
        if (!obj_dup)
        {
            driver_odr_registry_free(odr_registry_ptr);
            fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
            if (!config->verbose) remove(obj_path);
            driver_free_obj_files(obj_files, obj_count, config->output_file);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }
        obj_files[obj_count++] = obj_dup;
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
