/**
 * @file driver_nazm.c
 * @brief تنفيذ وضع --emit-nazm مع فشل ظاهر ومن دون رجوع صامت إلى GAS.
 */

#include "driver_internal.h"

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
