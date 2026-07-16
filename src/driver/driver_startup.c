/**
 * @file driver_startup.c
 * @brief Hosted Arabic startup source and object construction.
 */

#include "driver_startup.h"

#include "driver_nazm.h"
#include "driver_toolchain.h"
#include "../support/file_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAA_CUSTOM_START_SYMBOL "الرئيسية_بدء"

static char *driver_startup_artifact_path(const char *base, const char *suffix)
{
    if (!base || !suffix) return NULL;
    size_t base_size = strlen(base);
    size_t suffix_size = strlen(suffix);
    char *path = (char *)malloc(base_size + suffix_size + 1u);
    if (!path) return NULL;
    memcpy(path, base, base_size);
    memcpy(path + base_size, suffix, suffix_size + 1u);
    return path;
}

static bool driver_startup_write_text(const char *path, const char *text)
{
    if (!path || !text) return false;
    FILE *out = baa_fopen_utf8(path, "wb");
    if (!out) return false;
    size_t size = strlen(text);
    bool ok = fwrite(text, 1u, size, out) == size;
    if (fclose(out) != 0) ok = false;
    return ok;
}

const char *driver_startup_gas_source(const BaaTarget *target)
{
    if (target && target->obj_format == BAA_OBJFORMAT_ELF)
    {
        return
            ".text\n"
            ".globl " BAA_CUSTOM_START_SYMBOL "\n"
            BAA_CUSTOM_START_SYMBOL ":\n"
            "    movq %rsp, %r10\n"
            "    movq (%rsp), %rsi\n"
            "    leaq 8(%rsp), %rdx\n"
            "    leaq الرئيسية(%rip), %rdi\n"
            "    xorl %ecx, %ecx\n"
            "    xorl %r8d, %r8d\n"
            "    xorl %r9d, %r9d\n"
            "    subq $8, %rsp\n"
            "    pushq %r10\n"
            "    call __libc_start_main\n"
            "    hlt\n"
            ".section .note.GNU-stack,\"\",@progbits\n";
    }

    return
        ".text\n"
        ".globl " BAA_CUSTOM_START_SYMBOL "\n"
        BAA_CUSTOM_START_SYMBOL ":\n"
        "    andq $-16, %rsp\n"
        "    subq $32, %rsp\n"
        "    call بدء_ويندوز\n"
        "    hlt\n";
}

BaaCompilerExitCode driver_build_startup_object(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    char **out_object_path)
{
    if (out_object_path) *out_object_path = NULL;
    if (!config || !times || !out_object_path || !config->output_file)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    bool use_nazm = config->assembler == BAA_ASSEMBLER_NAZM;
    const char *source_suffix = use_nazm ? ".بدء-نظم.نظم" : ".بدء-غاز.s";
    const char *object_suffix = use_nazm ? ".بدء-نظم.o" : ".بدء-غاز.o";
    char *source_path =
        driver_startup_artifact_path(config->output_file, source_suffix);
    char *object_path =
        driver_startup_artifact_path(config->output_file, object_suffix);
    if (!source_path || !object_path)
    {
        free(source_path);
        free(object_path);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    (void)driver_toolchain_delete_file_utf8(source_path);
    (void)driver_toolchain_delete_file_utf8(object_path);
    BaaCompilerExitCode rc = BAA_COMPILER_EXIT_INTERNAL_ERROR;
    if (use_nazm)
    {
        rc = driver_assemble_nazm_startup(
            config,
            times,
            source_path,
            object_path,
            config->verbose);
    }
    else
    {
        const char *source = driver_startup_gas_source(config->target);
        if (!source || !driver_startup_write_text(source_path, source))
        {
            fprintf(stderr,
                    "خطأ: فشلت كتابة ملف بدء التشغيل '%s'.\n",
                    source_path);
            rc = BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
        }
        else
        {
            rc = driver_toolchain_assemble_one(
                config,
                times,
                source_path,
                object_path);
        }
        if (!config->verbose)
            (void)driver_toolchain_delete_file_utf8(source_path);
    }

    if (rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        (void)driver_toolchain_delete_file_utf8(object_path);
        free(source_path);
        free(object_path);
        return rc;
    }

    free(source_path);
    *out_object_path = object_path;
    return BAA_COMPILER_EXIT_SUCCESS;
}
