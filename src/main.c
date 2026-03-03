/**
 * @file main.c
 * @brief نقطة الدخول ومحرك سطر الأوامر (CLI Driver).
 * @version 0.3.4
 */

#include "baa.h"
#include "driver.h"
#include "driver_cli.h"
#include "driver_pipeline.h"
#include "driver_time.h"
#include "driver_toolchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



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

static int main_cleanup_and_return(DriverParseResult* cli,
                                   char** obj_files,
                                   int obj_count,
                                   char* output_file,
                                   bool output_file_owned,
                                   int rc)
{
    driver_free_obj_files(obj_files, obj_count, output_file);
    driver_parse_result_free(cli);
    if (output_file_owned) free(output_file);
    return rc;
}

// ============================================================================
// نقطة الدخول (Main Entry Point)
// ============================================================================

int main(int argc, char **argv)
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

    // البحث عن GCC المضمّن (إن وُجد)
    driver_toolchain_resolve_gcc_path();

    // تسجيل وقت البدء
    config.start_time = driver_time_seconds();

    // 0. التحقق من وجود معاملات
    if (argc < 2)
    {
        driver_print_help();
        return 1;
    }

    if (!driver_parse_cli(argc, argv, &config, &cli))
    {
        return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 1);
    }

    if (cli.cmd == DRIVER_CMD_HELP)
    {
        driver_print_help();
        return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 0);
    }
    if (cli.cmd == DRIVER_CMD_VERSION)
    {
        driver_print_version();
        return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 0);
    }
    if (cli.cmd == DRIVER_CMD_UPDATE)
    {
        run_updater();
        return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 0);
    }

    char **input_files = cli.input_files;
    int input_count = cli.input_count;

    if (input_count == 0)
    {
        fprintf(stderr, "Error: No input file specified\n");
        return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 1);
    }

    // تحديد اسم الملف المخرج الافتراضي
    if (!config.output_file)
    {
        if (config.assembly_only)
            config.output_file = NULL; // سيتم تحديده لكل ملف
        else if (config.compile_only)
            config.output_file = NULL; // سيتم تحديده لكل ملف
        else
        {
            const char *ext = (config.target && config.target->default_exe_ext) ? config.target->default_exe_ext : ".exe";
            size_t n = strlen("out") + strlen(ext) + 1;
            config.output_file = (char *)malloc(n);
            if (!config.output_file)
            {
                fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
                return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 1);
            }
            output_file_owned = true;
            (void)snprintf(config.output_file, n, "out%s", ext);
        }
    }

    CompilerPhaseTimes phase_times = {0};

    // v0.3.2.8.4: لا ندعم حالياً الربط/التجميع العابر للأهداف (cross-link/cross-assemble).
    // - نسمح بـ -S لتوليد assembly فقط لأي هدف.
    // - أما -c أو الربط النهائي فيتطلبان أن يطابق الهدف نظام المضيف.
    if (!config.assembly_only)
    {
        if (config.target && config.target->obj_format != driver_toolchain_host_object_format())
        {
            fprintf(stderr,
                    "خطأ: الهدف '%s' لا يطابق نظام المضيف لمرحلة التجميع/الربط.\n"
                    "ملاحظة: استخدم -S لتوليد ملف .s فقط. الدعم الكامل لـ cross-target مؤجل.\n",
                    config.target->name ? config.target->name : "<unknown>");
            return main_cleanup_and_return(&cli, NULL, 0, config.output_file, output_file_owned, 1);
        }
    }

    char **obj_files_to_link = NULL;
    int obj_count = 0;
    if (driver_compile_files(&config, input_files, input_count, &phase_times,
                             &obj_files_to_link, &obj_count) != 0)
    {
        return main_cleanup_and_return(&cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, 1);
    }

    // إذا طلب المستخدم -S أو -c، نتوقف هنا
    if (config.assembly_only || config.compile_only)
    {
        print_phase_times_and_mem(&config, &phase_times);
        return main_cleanup_and_return(&cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, 0);
    }

    // --- المرحلة النهائية: الربط (Linking) ---
    if (config.verbose)
        printf("\n[INFO] Linking %d object files...\n", obj_count);

    if (driver_toolchain_link(&config, &phase_times,
                              (const char **)obj_files_to_link, obj_count) != 0)
    {
        return main_cleanup_and_return(&cli, obj_files_to_link, obj_count, config.output_file,
                                       output_file_owned, 1);
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

    return main_cleanup_and_return(&cli, obj_files_to_link, obj_count, config.output_file,
                                   output_file_owned, 0);
}
