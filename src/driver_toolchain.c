/**
 * @file driver_toolchain.c
 * @brief تشغيل أدوات التجميع/الربط واكتشاف GCC المضمّن.
 * @version 0.3.4
 */

#include "driver_toolchain.h"

#include "process.h"
#include "driver_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

// ============================================================================
// البحث عن GCC المرفق (اكتشاف GCC المضمّن)
// ============================================================================

/**
 * @brief مسار GCC المكتشف (فارغ = استخدام "gcc" من PATH).
 */
static char g_gcc_path[MAX_PATH] = "";

#ifdef _WIN32

#define BAA_PATH_SUFFIX_GCC_BIN "\\gcc\\bin\\gcc.exe"
#define BAA_PATH_SUFFIX_GCC_BIN_DEV "\\..\\gcc\\bin\\gcc.exe"

/**
 * @brief بناء مسار (base + suffix) داخل مخزن مع ضمان عدم تجاوز السعة.
 * @return true عند النجاح، false عند تجاوز السعة.
 */
static bool path_build_suffix(char *out, size_t out_cap, const char *base, const char *suffix)
{
    if (!out || out_cap == 0 || !base || !suffix) return false;

    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);

    if (base_len >= out_cap) return false;
    if (suffix_len > out_cap - 1 - base_len) return false;

    memcpy(out, base, base_len);
    memcpy(out + base_len, suffix, suffix_len + 1);
    return true;
}

#endif

void driver_toolchain_resolve_gcc_path(void)
{
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof(exe_path));
    if (len == 0 || len >= (DWORD)sizeof(exe_path))
        return;

    // إزالة اسم الملف التنفيذي (الحصول على المجلد)
    char *last_sep = strrchr(exe_path, '\\');
    if (!last_sep)
        return;
    *last_sep = '\0';

    // المسار 1: <baa_dir>\gcc\bin\gcc.exe
    char candidate[MAX_PATH];
    if (path_build_suffix(candidate, sizeof(candidate), exe_path, BAA_PATH_SUFFIX_GCC_BIN) &&
        GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
    {
        (void)snprintf(g_gcc_path, sizeof(g_gcc_path), "%s", candidate);
        return;
    }

    // المسار 2: <baa_dir>\..\gcc\bin\gcc.exe (هيكل التطوير)
    if (path_build_suffix(candidate, sizeof(candidate), exe_path, BAA_PATH_SUFFIX_GCC_BIN_DEV) &&
        GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
    {
        (void)snprintf(g_gcc_path, sizeof(g_gcc_path), "%s", candidate);
        return;
    }
#endif
    // لم يُوجد gcc مضمّن ← سيُستخدم "gcc" من PATH
}

const char *driver_toolchain_get_gcc_command(void)
{
    return g_gcc_path[0] ? g_gcc_path : "gcc";
}

BaaObjectFormat driver_toolchain_host_object_format(void)
{
#ifdef _WIN32
    return BAA_OBJFORMAT_COFF;
#else
    return BAA_OBJFORMAT_ELF;
#endif
}

int driver_toolchain_assemble_one(const CompilerConfig *config,
                                 CompilerPhaseTimes *times,
                                 const char *asm_file,
                                 const char *obj_file)
{
    if (!config || !asm_file || !obj_file) return 1;

    const char *argv[8];
    int k = 0;
    argv[k++] = driver_toolchain_get_gcc_command();
    if (config->debug_info) argv[k++] = "-g";
    argv[k++] = "-c";
    argv[k++] = asm_file;
    argv[k++] = "-o";
    argv[k++] = obj_file;
    argv[k] = NULL;

    double t0 = 0.0;
    if (times && config->time_phases) t0 = driver_time_seconds();

    BaaProcessResult pr;
    if (!baa_process_run(argv, NULL, &pr) || pr.exit_code != 0)
    {
        printf("Error: Assembler failed for %s\n", asm_file);
        return 1;
    }

    if (times && config->time_phases) times->assemble_s += (driver_time_seconds() - t0);
    return 0;
}

int driver_toolchain_link(const CompilerConfig *config,
                          CompilerPhaseTimes *times,
                          const char **obj_files,
                          int obj_count)
{
    if (!config || !config->output_file) return 1;
    if (!obj_files || obj_count <= 0) return 1;

    int argv_cap = obj_count + 8;
    const char **argv_link = (const char **)calloc((size_t)argv_cap, sizeof(char *));
    if (!argv_link)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        return 1;
    }

    int lk = 0;
    argv_link[lk++] = driver_toolchain_get_gcc_command();
    if (config->debug_info) argv_link[lk++] = "-g";
    if (config->codegen_opts.pie && config->target && config->target->obj_format == BAA_OBJFORMAT_ELF)
        argv_link[lk++] = "-pie";

    for (int i = 0; i < obj_count; i++)
        argv_link[lk++] = obj_files[i];

    argv_link[lk++] = "-o";
    argv_link[lk++] = config->output_file;
    argv_link[lk] = NULL;

    double t0 = 0.0;
    if (times && config->time_phases) t0 = driver_time_seconds();

    BaaProcessResult pr;
    if (!baa_process_run(argv_link, NULL, &pr) || pr.exit_code != 0)
    {
        printf("Error: Linker failed.\n");
        free(argv_link);
        return 1;
    }

    if (times && config->time_phases) times->link_s += (driver_time_seconds() - t0);

    free(argv_link);
    return 0;
}
