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

static char *main_nazm_shadow_output_path(const CompilerConfig *config)
{
    if (!config || !config->output_file) return NULL;
    const char *exe_suffix = config->target && config->target->default_exe_ext
        ? config->target->default_exe_ext
        : "";
    const char *marker = ".ظل-نظم";
    size_t output_len = strlen(config->output_file);
    size_t marker_len = strlen(marker);
    size_t suffix_len = strlen(exe_suffix);
    char *path = (char *)malloc(output_len + marker_len + suffix_len + 1u);
    if (!path) return NULL;
    memcpy(path, config->output_file, output_len);
    memcpy(path + output_len, marker, marker_len);
    memcpy(path + output_len + marker_len, exe_suffix, suffix_len + 1u);
    return path;
}

static void main_release_shadow_object(char **path, bool remove_file)
{
    if (!path || !*path) return;
    if (remove_file) (void)driver_toolchain_delete_file_utf8(*path);
    free(*path);
    *path = NULL;
}

static bool main_all_inputs_are_nazm(char **input_files, int input_count)
{
    if (!input_files || input_count <= 0) return false;
    for (int i = 0; i < input_count; ++i)
    {
        if (!driver_nazm_is_source_path(input_files[i])) return false;
    }
    return true;
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
    if (config) free(config->nazm_fingerprint);
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
    config.assembler = BAA_ASSEMBLER_NAZM;
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
    if (cli.cmd == DRIVER_CMD_COMPLETION_DATA)
    {
        bool ok = driver_completion_data_json_write(stdout, BAA_VERSION);
        return main_cleanup_and_return(
            &config, &cli, NULL, 0, config.output_file, output_file_owned,
            ok ? BAA_COMPILER_EXIT_SUCCESS : BAA_COMPILER_EXIT_INTERNAL_ERROR);
    }
    if (cli.cmd == DRIVER_CMD_FORMAT)
    {
        const char *logical_file =
            cli.input_count == 1 ? cli.input_files[0] : "";
        char *source = config.source_stdin_file
            ? read_stdin_source()
            : read_file(logical_file);
        if (!source)
        {
            fprintf(stderr, "خطأ: تعذر قراءة مصدر باء المطلوب تنسيقه.\n");
            return main_cleanup_and_return(
                &config, &cli, NULL, 0, config.output_file, output_file_owned,
                BAA_COMPILER_EXIT_INTERNAL_ERROR);
        }
        const BaaFormatStatus status =
            driver_format_json_write(stdout, BAA_VERSION, logical_file, source);
        free(source);
        if (status == BAA_FORMAT_INVALID_UTF8)
            fprintf(stderr, "خطأ: مصدر التنسيق ليس UTF-8 صالحاً.\n");
        else if (status != BAA_FORMAT_OK)
            fprintf(stderr, "خطأ: نفدت الذاكرة أثناء تنسيق مصدر باء.\n");
        return main_cleanup_and_return(
            &config, &cli, NULL, 0, config.output_file, output_file_owned,
            status == BAA_FORMAT_OK
                ? BAA_COMPILER_EXIT_SUCCESS
                : status == BAA_FORMAT_INVALID_UTF8
                    ? BAA_COMPILER_EXIT_SOURCE_ERROR
                    : BAA_COMPILER_EXIT_INTERNAL_ERROR);
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

#ifndef BAA_EMBEDDED_NAZM
    if (config.nazm_in_process)
    {
        fprintf(stderr,
                "غير مدعوم: هذا البناء لا يتضمن nazm-api-v1؛ أعد بناء باء "
                "مع BAA_ENABLE_EMBEDDED_NAZM=ON أو احذف "
                "--نظم-داخل-العملية.\n");
        return main_cleanup_and_return(&config, &cli, NULL, 0,
                                       config.output_file,
                                       output_file_owned,
                                       BAA_COMPILER_EXIT_UNSUPPORTED);
    }
#endif

    char **input_files = cli.input_files;
    int input_count = cli.input_count;

    if (input_count == 0)
    {
        fprintf(stderr, "Error: No input file specified\n");
        return main_cleanup_and_return(&config, &cli, NULL, 0, config.output_file,
                                       output_file_owned, BAA_COMPILER_EXIT_INVALID_INVOCATION);
    }
    if (config.nazm_shadow_executable && input_count != 1)
    {
        fprintf(stderr, "خطأ: --nazm-shadow يدعم ملف باء واحدا في الشريحة الأولى.\n");
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

    bool build_uses_nazm = config.assembler == BAA_ASSEMBLER_NAZM;
    for (int i = 0; i < input_count && !build_uses_nazm; ++i)
        build_uses_nazm = driver_nazm_is_source_path(input_files[i]);
    if (build_uses_nazm &&
        (config.incremental || config.build_manifest_file) &&
        !config.check_only && !config.header_check && !config.emit_nazm &&
        !config.assembly_only)
    {
        BaaCompilerExitCode fingerprint_rc =
            driver_nazm_resolve_fingerprint(&config);
        if (fingerprint_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            driver_build_manifest_free(&build_manifest);
            return main_cleanup_and_return(&config, &cli, NULL, 0,
                                           config.output_file,
                                           output_file_owned,
                                           fingerprint_rc);
        }
    }

    // v0.3.2.8.4+: الربط النهائي يبقى مقيدا بالمضيف.
    // - نسمح بـ -S لأي هدف.
    // - يسمح نظم أيضا بـ -c عابر للأهداف لأنه يكتب ELF64/COFF مباشرة.
    // - GAS -c والربط النهائي يتطلبان أن يطابق الهدف نظام المضيف.
    if (!config.assembly_only && !config.emit_nazm &&
        !config.check_only && !config.header_check)
    {
        if (config.target && config.target->obj_format != driver_toolchain_host_object_format())
        {
            bool nazm_cross_object =
                config.compile_only &&
                (config.assembler == BAA_ASSEMBLER_NAZM ||
                 main_all_inputs_are_nazm(input_files, input_count));
            if (!nazm_cross_object)
            {
                fprintf(stderr,
                        "خطأ: الهدف '%s' لا يطابق نظام المضيف لمرحلة التجميع/الربط.\n"
                        "ملاحظة: استخدم -S، أو -c --assembler=nazm لتوليد كائن عابر للأهداف. "
                        "الربط العابر للأهداف مؤجل.\n",
                        config.target->name ? config.target->name : "<unknown>");
                driver_build_manifest_free(&build_manifest);
                return main_cleanup_and_return(
                    &config,
                    &cli,
                    NULL,
                    0,
                    config.output_file,
                    output_file_owned,
                    BAA_COMPILER_EXIT_UNSUPPORTED);
            }
        }
    }

    char **obj_files_to_link = NULL;
    int obj_count = 0;
    char *nazm_shadow_object = NULL;
    BaaCompilerExitCode compile_rc =
        driver_compile_files(&config, input_files, input_count, &phase_times,
                             &build_manifest, &obj_files_to_link, &obj_count,
                             &nazm_shadow_object);
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
            main_release_shadow_object(&nazm_shadow_object, true);
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
        main_release_shadow_object(&nazm_shadow_object, true);
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
        main_release_shadow_object(&nazm_shadow_object, true);
        return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, link_rc);
    }

    if (config.nazm_shadow_executable)
    {
        if (!nazm_shadow_object)
        {
            fprintf(stderr, "خطأ: لم ينتج مسار ظل نظم ملفا كائنيا.\n");
            driver_build_manifest_free(&build_manifest);
            return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count,
                                           config.output_file, output_file_owned,
                                           BAA_COMPILER_EXIT_INTERNAL_ERROR);
        }

        char *shadow_output = main_nazm_shadow_output_path(&config);
        if (!shadow_output)
        {
            main_release_shadow_object(&nazm_shadow_object, true);
            driver_build_manifest_free(&build_manifest);
            return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count,
                                           config.output_file, output_file_owned,
                                           BAA_COMPILER_EXIT_INTERNAL_ERROR);
        }

        CompilerConfig shadow_config = config;
        shadow_config.output_file = shadow_output;
        shadow_config.nazm_shadow_executable = NULL;
        const char *shadow_objects[2] = {nazm_shadow_object, NULL};
        int shadow_object_count = 1;
        if (config.target && config.target->obj_format == BAA_OBJFORMAT_ELF)
        {
            if (obj_count < 1)
            {
                fprintf(stderr, "خطأ: كائن بدء التشغيل العربي مفقود من ربط ظل نظم.\n");
                free(shadow_output);
                main_release_shadow_object(&nazm_shadow_object, false);
                driver_build_manifest_free(&build_manifest);
                return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count,
                                               config.output_file, output_file_owned,
                                               BAA_COMPILER_EXIT_INTERNAL_ERROR);
            }
            // يضيف خط التجميع كائن `الرئيسية_بدء` أخيرا لكل ربط ELF تنفيذي.
            // يعيد ظل نظم استخدام الكائن نفسه حتى يمر عبر عقد بدء التشغيل المستضاف.
            shadow_objects[shadow_object_count++] = obj_files_to_link[obj_count - 1];
        }
        BaaCompilerExitCode shadow_link_rc =
            driver_toolchain_link(&shadow_config, &phase_times,
                                  shadow_objects, shadow_object_count);
        if (shadow_link_rc != BAA_COMPILER_EXIT_SUCCESS)
        {
            fprintf(stderr, "خطأ: فشل ربط ناتج ظل نظم؛ لا يعد نجاح GAS نجاحا للمسار الظلي.\n");
            free(shadow_output);
            main_release_shadow_object(&nazm_shadow_object, false);
            driver_build_manifest_free(&build_manifest);
            return main_cleanup_and_return(&config, &cli, obj_files_to_link, obj_count,
                                           config.output_file, output_file_owned,
                                           shadow_link_rc);
        }

        if (config.verbose)
            printf("[INFO] Linked Nazm shadow executable: %s\n", shadow_output);
        free(shadow_output);
        main_release_shadow_object(&nazm_shadow_object, false);
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
