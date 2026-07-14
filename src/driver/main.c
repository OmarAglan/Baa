/**
 * @file main.c
 * @brief نقطة الدخول ومحرك سطر الأوامر (CLI Driver).
 * @version 0.3.4
 */

#include "driver_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief طباعة ملخص قياس المراحل (زمن + ذاكرة ساحة IR) إلى stderr.
 */
static void print_phase_times_and_mem(const CompilerConfig* config,
                                      const CompilerPhaseTimes* phase_times)
{
    if (!config || !phase_times) return;
    if (!config->time_phases) return;

    double total = driver_time_seconds() - config->start_time;
    fprintf(stderr,
            "[TIME] read=%.6f parse=%.6f analyze=%.6f lower=%.6f opt=%.6f verify_ir=%.6f verify_ssa=%.6f outssa=%.6f unroll=%.6f isel=%.6f regalloc=%.6f emit=%.6f assemble=%.6f link=%.6f total=%.6f\n",
            phase_times->read_file_s,
            phase_times->parse_s,
            phase_times->analyze_s,
            phase_times->lower_ir_s,
            phase_times->optimize_s,
            phase_times->verify_ir_s,
            phase_times->verify_ssa_s,
            phase_times->outssa_s,
            phase_times->unroll_s,
            phase_times->isel_s,
            phase_times->regalloc_s,
            phase_times->emit_s,
            phase_times->assemble_s,
            phase_times->link_s,
            total);

    fprintf(stderr,
            "[MEM] ir_arena_used_max=%zu ir_arena_cap_max=%zu ir_arena_chunks_max=%zu\n",
            phase_times->ir_arena_used_max,
            phase_times->ir_arena_cap_max,
            phase_times->ir_arena_chunks_max);
}

static const char* main_mode_name(const CompilerConfig* config)
{
    if (!config) return "compile";
    if (config->check_only) return "check";
    if (config->header_check) return "check-header";
    if (config->emit_nazm) return "nazm-source";
    if (config->assembly_only) return "assembly";
    if (config->compile_only) return "compile";
    return "link";
}

static int main_cleanup_and_return(const CompilerConfig* config,
                                   DriverParseResult* cli,
                                   char** obj_files,
                                   int obj_count,
                                   char* output_file,
                                   bool output_file_owned,
                                   int rc)
{
    if (config && config->diagnostics_json && cli && cli->cmd == DRIVER_CMD_COMPILE)
    {
        diagnostics_json_write(stdout,
                               BAA_VERSION,
                               main_mode_name(config),
                               config->target ? config->target->name : "",
                               ".");
    }
    driver_free_obj_files(obj_files, obj_count, output_file);
    driver_parse_result_free(cli);
    if (output_file_owned) free(output_file);
    return rc;
}

// ============================================================================
// نقطة الدخول (Main Entry Point)
// ============================================================================

static int baa_main(int argc, char **argv)
{
    CompilerConfig config = {0};
    bool output_file_owned = false;
    config.output_file = NULL;
    config.opt_level = OPT_LEVEL_1; // Default optimization level
    config.funroll_loops = false;
    config.target = baa_target_host_default();
    config.codegen_opts = baa_codegen_options_default();

    DriverParseResult cli = {0};

    // تهيئة نظام التحذيرات
    warning_init();
    diagnostics_json_reset();
    diagnostics_set_json_enabled(false);

    // البحث عن GCC المضمّن (إن وُجد)
    driver_toolchain_resolve_gcc_path();

    // تسجيل وقت البدء
    config.start_time = driver_time_seconds();

    // 0. التحقق من وجود معاملات
    if (argc < 2)
    {
        driver_print_help();
        return BAA_COMPILER_EXIT_INVALID_INVOCATION;
    }

    if (!driver_parse_cli(argc, argv, &config, &cli))
    {
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_INVALID_INVOCATION);
    }
    diagnostics_set_json_enabled(config.diagnostics_json);
    if (config.diagnostics_json)
        g_warning_config.colored_output = false;

    if (cli.cmd == DRIVER_CMD_HELP)
    {
        driver_print_help();
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_SUCCESS);
    }
    if (cli.cmd == DRIVER_CMD_VERSION)
    {
        driver_print_version();
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_SUCCESS);
    }
    if (cli.cmd == DRIVER_CMD_TARGET_INFO)
    {
        driver_print_target_info_json(config.target);
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_SUCCESS);
    }
    if (cli.cmd == DRIVER_CMD_EXPLAIN)
    {
        bool ok = driver_print_diagnostic_explain(cli.explain_code);
        return main_cleanup_and_return(
            &config, &cli, NULL, 0, config.output_file, output_file_owned,
            ok ? BAA_COMPILER_EXIT_SUCCESS : BAA_COMPILER_EXIT_INVALID_INVOCATION);
    }
    if (cli.cmd == DRIVER_CMD_UPDATE)
    {
        run_updater();
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_SUCCESS);
    }

    char **input_files = cli.input_files;
    int input_count = cli.input_count;

    if (input_count == 0)
    {
        fprintf(stderr, "Error: No input file specified\n");
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_INVALID_INVOCATION);
    }

    // تحديد اسم الملف المخرج الافتراضي
    if (!config.output_file)
    {
        if (config.assembly_only || config.emit_nazm)
            config.output_file = NULL; // سيتم تحديده لكل ملف
        else if (config.compile_only)
            config.output_file = NULL; // سيتم تحديده لكل ملف
        else if (config.check_only)
            config.output_file = NULL;
        else if (config.header_check)
            config.output_file = NULL;
        else
        {
            const char *ext = (config.target && config.target->default_exe_ext) ? config.target->default_exe_ext : ".exe";
            size_t n = strlen("out") + strlen(ext) + 1;
            config.output_file = (char *)malloc(n);
            if (!config.output_file)
            {
                fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
                return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                               output_file_owned, BAA_COMPILER_EXIT_INTERNAL_ERROR);
            }
            output_file_owned = true;
            (void)snprintf(config.output_file, n, "out%s", ext);
        }
    }

    CompilerPhaseTimes phase_times = {0};
    DriverBuildManifest build_manifest;
    driver_build_manifest_init(&build_manifest);

    // v0.3.2.8.4: لا ندعم حالياً الربط/التجميع العابر للأهداف (cross-link/cross-assemble).
    // - نسمح بـ -S لتوليد assembly فقط لأي هدف.
    // - أما -c أو الربط النهائي فيتطلبان أن يطابق الهدف نظام المضيف.
    if (!config.assembly_only && !config.emit_nazm &&
        !config.check_only && !config.header_check)
    {
        if (config.target && config.target->obj_format != driver_toolchain_host_object_format())
        {
            fprintf(stderr,
                    "خطأ: الهدف '%s' لا يطابق نظام المضيف لمرحلة التجميع/الربط.\n"
                    "ملاحظة: استخدم -S لتوليد ملف .s فقط. الدعم الكامل لـ cross-target مؤجل.\n",
                    config.target->name ? config.target->name : "<unknown>");
            driver_build_manifest_free(&build_manifest);
            return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                           output_file_owned, BAA_COMPILER_EXIT_UNSUPPORTED);
        }
    }

    char **obj_files_to_link = NULL;
    int obj_count = 0;
    BaaCompilerExitCode compile_rc =
        driver_compile_files(&config, input_files, input_count, &phase_times,
                             &build_manifest, &obj_files_to_link, &obj_count);
    if (compile_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        driver_build_manifest_free(&build_manifest);
        return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, compile_rc);
    }

    if (config.build_manifest_file)
    {
        if (!driver_build_write_manifest(&config, &build_manifest, config.build_manifest_file))
        {
            fprintf(stderr, "خطأ: فشل كتابة بيان البناء '%s'.\n", config.build_manifest_file);
            driver_build_manifest_free(&build_manifest);
            return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count, config.output_file,
                                           output_file_owned, BAA_COMPILER_EXIT_TOOLCHAIN_ERROR);
        }
    }

    // إذا طلب المستخدم نمطاً يتوقف قبل الربط النهائي، نتوقف هنا
    if (config.assembly_only || config.emit_nazm || config.compile_only ||
        config.check_only || config.header_check)
    {
        print_phase_times_and_mem(&config, &phase_times);
        driver_build_manifest_free(&build_manifest);
        return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_SUCCESS);
    }

    // --- المرحلة النهائية: الربط (Linking) ---
    if (config.verbose)
        printf("\n[INFO] Linking %d object files...\n", obj_count);

    BaaCompilerExitCode link_rc =
        driver_toolchain_link(&config, &phase_times,
                              (const char **)obj_files_to_link, obj_count);
    if (link_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        driver_build_manifest_free(&build_manifest);
        return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, link_rc);
    }

    // تنظيف ملفات الكائنات المؤقتة
    if (!config.verbose)
    {
        for (int i = 0; i < obj_count; i++)
        {
            (void)driver_toolchain_delete_file_utf8(obj_files_to_link[i]);
        }
    }

    // ملخص التحذيرات
    int warn_count = warning_get_count();
    if (warn_count > 0 && config.verbose)
    {
        printf("[INFO] Compilation completed with %d warning(s).\n", warn_count);
    }

    // عرض وقت الترجمة
    if (config.verbose)
    {
        double elapsed = driver_time_seconds() - config.start_time;
        printf("[INFO] Build successful: %s\n", config.output_file);
        printf("[INFO] Compilation time: %.3f seconds\n", elapsed);
    }

    print_phase_times_and_mem(&config, &phase_times);

    driver_build_manifest_free(&build_manifest);
    return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count, config.output_file,
                                   output_file_owned, BAA_COMPILER_EXIT_SUCCESS);
}

#ifdef _WIN32
/**
 * @brief تحويل وسيط UTF-16 من سطر أوامر ويندوز إلى UTF-8 مملوك.
 */
static char* main_utf8_from_wide(const wchar_t* value)
{
    if (!value) return NULL;

    int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                                     NULL, 0, NULL, NULL);
    if (needed <= 0) return NULL;

    char* utf8 = (char*)malloc((size_t)needed);
    if (!utf8) return NULL;

    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            utf8, needed, NULL, NULL) <= 0)
    {
        free(utf8);
        return NULL;
    }
    return utf8;
}

/**
 * @brief نقطة دخول ويندوز الواسعة؛ تمنع فساد أسماء الملفات العربية في argv.
 */
int wmain(int argc, wchar_t** wide_argv)
{
    if (argc < 0 || !wide_argv) return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    char** utf8_argv = (char**)calloc((size_t)argc + 1, sizeof(char*));
    if (!utf8_argv)
    {
        fputs("خطأ داخلي: تعذر تخصيص وسائط سطر الأوامر.\n", stderr);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    for (int i = 0; i < argc; ++i)
    {
        utf8_argv[i] = main_utf8_from_wide(wide_argv[i]);
        if (!utf8_argv[i])
        {
            fputs("خطأ داخلي: تعذر تحويل وسيط سطر الأوامر إلى UTF-8.\n", stderr);
            for (int j = 0; j < i; ++j) free(utf8_argv[j]);
            free(utf8_argv);
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }
    }

    int rc = baa_main(argc, utf8_argv);
    for (int i = 0; i < argc; ++i) free(utf8_argv[i]);
    free(utf8_argv);
    return rc;
}
#else
int main(int argc, char** argv)
{
    return baa_main(argc, argv);
}
#endif
