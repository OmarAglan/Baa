/**
 * @file driver_nazm.c
 * @brief تنفيذ وضع --emit-nazm مع فشل ظاهر ومن دون رجوع صامت إلى GAS.
 */

#include "driver_internal.h"

static char *driver_nazm_artifact_path(const char *base, const char *suffix)
{
    if (!base || !suffix) return NULL;
    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);
    char *path = (char *)malloc(base_len + suffix_len + 1u);
    if (!path) return NULL;
    memcpy(path, base, base_len);
    memcpy(path + base_len, suffix, suffix_len + 1u);
    return path;
}

BaaCompilerExitCode driver_emit_nazm_source(const CompilerConfig *config,
                                            MachineModule *module,
                                            const char *current_input,
                                            const char *output_path)
{
    if (!config || !module || !output_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    if (config->custom_startup || config->debug_info ||
        config->codegen_opts.stack_protector != BAA_STACKPROT_OFF)
    {
        fprintf(stderr,
                "خطأ: شريحة نظم الأولى لا تدعم بدء التشغيل المخصص أو معلومات التنقيح أو حماية المكدس.\n");
        return BAA_COMPILER_EXIT_UNSUPPORTED;
    }

    FILE *out = baa_fopen_utf8(output_path, "wb");
    if (!out)
    {
        fprintf(stderr, "خطأ: تعذرت كتابة مصدر نظم '%s'.\n", output_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    BaaNazmEmitResult result = emit_nazm_module(module, out, config->target);
    int close_result = fclose(out);
    if (result.status == BAA_NAZM_EMIT_OK && close_result == 0)
        return BAA_COMPILER_EXIT_SUCCESS;

    (void)driver_toolchain_delete_file_utf8(output_path);
    if (result.status == BAA_NAZM_EMIT_UNSUPPORTED)
    {
        fprintf(stderr, "خطأ: %s",
                result.reason ? result.reason : "صيغة نظم غير مدعومة.");
        if (result.source_line > 0)
        {
            fprintf(stderr, " (%s:%d:%d)",
                    result.source_file ? result.source_file : current_input,
                    result.source_line,
                    result.source_col);
        }
        fputc('\n', stderr);
        return BAA_COMPILER_EXIT_UNSUPPORTED;
    }

    fprintf(stderr, "خطأ: فشلت كتابة مصدر نظم '%s'.\n", output_path);
    return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
}

BaaCompilerExitCode driver_emit_nazm_shadow_object(const CompilerConfig *config,
                                                   MachineModule *module,
                                                   const char *current_input,
                                                   char **out_object_path)
{
    if (out_object_path) *out_object_path = NULL;
    if (!config || !module || !config->nazm_shadow_executable ||
        !config->output_file || !out_object_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    const char *object_suffix = config->target &&
                                config->target->obj_format == BAA_OBJFORMAT_COFF
        ? ".ظل-نظم.obj"
        : ".ظل-نظم.o";
    const char *format = config->target &&
                         config->target->obj_format == BAA_OBJFORMAT_COFF
        ? "كوف"
        : "إلف64";

    char *source_path = driver_nazm_artifact_path(config->output_file,
                                                  ".ظل-نظم.نظم");
    char *object_path = driver_nazm_artifact_path(config->output_file,
                                                  object_suffix);
    if (!source_path || !object_path)
    {
        free(source_path);
        free(object_path);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    BaaCompilerExitCode emit_rc =
        driver_emit_nazm_source(config, module, current_input, source_path);
    if (emit_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        free(source_path);
        free(object_path);
        return emit_rc;
    }

    const char *argv[] = {
        config->nazm_shadow_executable,
        "-ص",
        format,
        "-خ",
        object_path,
        source_path,
        NULL,
    };
    BaaProcessResult process = {0};
    if (!baa_process_run(argv, NULL, &process) || !process.started ||
        process.exit_code != 0)
    {
        fprintf(stderr,
                "خطأ: فشل مجمّع نظم في مسار الظل%s%d.\n",
                process.started ? " برمز خروج " : " قبل بدء العملية؛ الرمز ",
                process.exit_code);
        (void)driver_toolchain_delete_file_utf8(source_path);
        (void)driver_toolchain_delete_file_utf8(object_path);
        free(source_path);
        free(object_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    if (config->verbose)
    {
        printf("[INFO] Generated Nazm shadow source: %s\n", source_path);
        printf("[INFO] Generated Nazm shadow object: %s\n", object_path);
    }

    free(source_path);
    *out_object_path = object_path;
    return BAA_COMPILER_EXIT_SUCCESS;
}
