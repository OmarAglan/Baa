/**
 * @file driver_toolchain.c
 * @brief تشغيل أدوات التجميع/الربط واكتشاف GCC المضمّن.
 * @version 0.3.4
 */

#include "driver_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <unistd.h>
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
static char g_runtime_library_path[MAX_PATH] = "";

static bool runtime_library_exists(const char* path)
{
    if (!path || !path[0]) return false;
    FILE* file = baa_fopen_utf8(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static bool runtime_library_try_dir(const char* directory)
{
    if (!directory || !directory[0]) return false;
    const char* suffix = "/libbaa_runtime.a";
    size_t need = strlen(directory) + strlen(suffix) + 1u;
    if (need > sizeof(g_runtime_library_path)) return false;
    snprintf(g_runtime_library_path, sizeof(g_runtime_library_path), "%s%s", directory, suffix);
    if (runtime_library_exists(g_runtime_library_path)) return true;
    g_runtime_library_path[0] = '\0';
    return false;
}

static void runtime_library_resolve_from_exe_dir(const char* exe_dir)
{
    if (!exe_dir || !exe_dir[0] || g_runtime_library_path[0]) return;
    if (runtime_library_try_dir(exe_dir)) return;

    char candidate[MAX_PATH];
    int n = snprintf(candidate, sizeof(candidate), "%s/../lib/baa", exe_dir);
    if (n > 0 && (size_t)n < sizeof(candidate) && runtime_library_try_dir(candidate)) return;

    n = snprintf(candidate, sizeof(candidate), "%s/../lib64/baa", exe_dir);
    if (n > 0 && (size_t)n < sizeof(candidate)) (void)runtime_library_try_dir(candidate);
}

#ifdef _WIN32

#define BAA_PATH_SUFFIX_GCC_BIN_W L"\\gcc\\bin\\gcc.exe"
#define BAA_PATH_SUFFIX_GCC_BIN_DEV_W L"\\..\\gcc\\bin\\gcc.exe"

static unsigned long g_toolchain_file_counter = 0;

static wchar_t* win_utf8_to_wide_alloc(const char* text)
{
    if (!text) return NULL;

    int need = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    UINT code_page = CP_UTF8;
    if (need <= 0) {
        code_page = CP_ACP;
        need = MultiByteToWideChar(code_page, 0, text, -1, NULL, 0);
    }
    if (need <= 0) return NULL;

    wchar_t* out = (wchar_t*)calloc((size_t)need, sizeof(wchar_t));
    if (!out) return NULL;

    if (MultiByteToWideChar(code_page, 0, text, -1, out, need) <= 0) {
        free(out);
        return NULL;
    }

    return out;
}

static bool win_wide_to_utf8(const wchar_t* text, char* out, size_t out_cap)
{
    if (!text || !out || out_cap == 0) return false;

    int need = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (need <= 0) return false;
    if ((size_t)need > out_cap) return false;

    return WideCharToMultiByte(CP_UTF8, 0, text, -1, out, (int)out_cap, NULL, NULL) > 0;
}

static bool win_utf8_is_ascii(const char* text)
{
    if (!text) return false;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (*p >= 0x80u) return false;
    }
    return true;
}

static bool win_write_entry_response(const char* path, const char* symbol)
{
    if (!path || !symbol) return false;
    FILE* file = baa_fopen_utf8(path, "wb");
    if (!file) return false;
    int written = fprintf(file, "-u\n%s\n-e\n%s\n", symbol, symbol);
    bool ok = written > 0 && fclose(file) == 0;
    return ok;
}

static bool path_build_suffix_w(wchar_t* out, size_t out_cap, const wchar_t* base, const wchar_t* suffix)
{
    if (!out || out_cap == 0 || !base || !suffix) return false;

    size_t base_len = wcslen(base);
    size_t suffix_len = wcslen(suffix);
    if (base_len >= out_cap) return false;
    if (suffix_len > out_cap - 1 - base_len) return false;

    memcpy(out, base, base_len * sizeof(wchar_t));
    memcpy(out + base_len, suffix, (suffix_len + 1) * sizeof(wchar_t));
    return true;
}

static bool win_copy_path_text(const char* path, char* out, size_t out_cap)
{
    if (!path || !out || out_cap == 0) return false;
    size_t size = strlen(path) + 1u;
    if (size > out_cap) return false;
    memcpy(out, path, size);
    return true;
}

static bool win_prepare_output_file(const wchar_t* path)
{
    if (!path) return false;
    HANDLE file = CreateFileW(path,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}

/**
 * @brief تجهيز مسار الأداة إلى الملف الحقيقي بلا نسخ.
 *
 * GCC المرفق حالياً لا يفتح مسارات Unicode من argv. عند الحاجة نستخدم الاسم
 * القصير لنفس كيان NTFS؛ لذلك تكتب الأداة مباشرة في الملف العربي المطلوب.
 */
static bool win_prepare_toolchain_path(const char* path,
                                       bool prepare_output,
                                       char* out,
                                       size_t out_cap)
{
    if (!path || !out || out_cap == 0) return false;
    bool needs_alias = !win_utf8_is_ascii(path) ||
                       strlen(path) >= 240u ||
                       strpbrk(path, " \t") != NULL;
    if (!needs_alias) return win_copy_path_text(path, out, out_cap);

    wchar_t* path_w = win_utf8_to_wide_alloc(path);
    if (!path_w) return false;
    if (prepare_output && !win_prepare_output_file(path_w)) {
        free(path_w);
        return false;
    }

    wchar_t short_w[MAX_PATH];
    DWORD short_len = GetShortPathNameW(path_w, short_w, MAX_PATH);
    free(path_w);
    if (short_len == 0 || short_len >= MAX_PATH) return false;
    if (!win_wide_to_utf8(short_w, out, out_cap)) return false;
    return win_utf8_is_ascii(out);
}

static bool win_make_link_response_path(const char* output_path,
                                        char* out,
                                        size_t out_cap)
{
    if (!output_path || !out || out_cap == 0) return false;
    unsigned long process_id = (unsigned long)GetCurrentProcessId();
    unsigned long file_id = ++g_toolchain_file_counter;
    int count = snprintf(out,
                         out_cap,
                         "%s.baa_link_%lu_%lu.rsp",
                         output_path,
                         process_id,
                         file_id);
    return count > 0 && (size_t)count < out_cap;
}

/**
 * @brief بناء مسار (base + suffix) داخل مخزن مع ضمان عدم تجاوز السعة.
 * @return true عند النجاح، false عند تجاوز السعة.
 */
#endif

void driver_toolchain_resolve_gcc_path(void)
{
#ifdef _WIN32
    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_path, (DWORD)MAX_PATH);
    if (len == 0 || len >= (DWORD)MAX_PATH)
        return;

    // إزالة اسم الملف التنفيذي (الحصول على المجلد)
    wchar_t* last_sep = wcsrchr(exe_path, L'\\');
    if (!last_sep)
        return;
    *last_sep = L'\0';

    char exe_dir_utf8[MAX_PATH];
    if (win_wide_to_utf8(exe_path, exe_dir_utf8, sizeof(exe_dir_utf8))) {
        runtime_library_resolve_from_exe_dir(exe_dir_utf8);
    }

    // المسار 1: <baa_dir>\gcc\bin\gcc.exe
    wchar_t candidate[MAX_PATH];
    if (path_build_suffix_w(candidate, MAX_PATH, exe_path, BAA_PATH_SUFFIX_GCC_BIN_W) &&
        GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES &&
        win_wide_to_utf8(candidate, g_gcc_path, sizeof(g_gcc_path)))
    {
        return;
    }

    // المسار 2: <baa_dir>\..\gcc\bin\gcc.exe (هيكل التطوير)
    if (path_build_suffix_w(candidate, MAX_PATH, exe_path, BAA_PATH_SUFFIX_GCC_BIN_DEV_W) &&
        GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES &&
        win_wide_to_utf8(candidate, g_gcc_path, sizeof(g_gcc_path)))
    {
        return;
    }
#else
    char exe_path[MAX_PATH];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1u);
    if (len > 0 && (size_t)len < sizeof(exe_path)) {
        exe_path[len] = '\0';
        char* last_sep = strrchr(exe_path, '/');
        if (last_sep) {
            *last_sep = '\0';
            runtime_library_resolve_from_exe_dir(exe_path);
        }
    }
#endif
    // لم يُوجد gcc مضمّن ← سيُستخدم "gcc" من PATH
}

const char* driver_toolchain_get_runtime_library(void)
{
    return g_runtime_library_path[0] ? g_runtime_library_path : NULL;
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

bool driver_toolchain_copy_file_utf8(const char* src_path, const char* dst_path)
{
    if (!src_path || !dst_path) return false;

#ifdef _WIN32
    wchar_t* src_w = win_utf8_to_wide_alloc(src_path);
    wchar_t* dst_w = win_utf8_to_wide_alloc(dst_path);
    if (!src_w || !dst_w) {
        free(src_w);
        free(dst_w);
        return false;
    }

    BOOL ok = CopyFileW(src_w, dst_w, FALSE);
    free(src_w);
    free(dst_w);
    return ok ? true : false;
#else
    FILE* in_f = baa_fopen_utf8(src_path, "rb");
    if (!in_f) return false;

    FILE* out_f = baa_fopen_utf8(dst_path, "wb");
    if (!out_f) {
        fclose(in_f);
        return false;
    }

    char buffer[8192];
    bool ok = true;
    while (!feof(in_f)) {
        size_t got = fread(buffer, 1, sizeof(buffer), in_f);
        if (got > 0) {
            if (fwrite(buffer, 1, got, out_f) != got) {
                ok = false;
                break;
            }
        }
        if (ferror(in_f)) {
            ok = false;
            break;
        }
    }

    fclose(out_f);
    fclose(in_f);
    return ok;
#endif
}

bool driver_toolchain_delete_file_utf8(const char* path)
{
    if (!path) return false;

#ifdef _WIN32
    wchar_t* path_w = win_utf8_to_wide_alloc(path);
    if (!path_w) return false;

    BOOL ok = DeleteFileW(path_w);
    if (!ok) {
        DWORD e = GetLastError();
        free(path_w);
        return (e == ERROR_FILE_NOT_FOUND) || (e == ERROR_PATH_NOT_FOUND);
    }

    free(path_w);
    return true;
#else
    if (remove(path) == 0) return true;
    return (errno == ENOENT);
#endif
}

#ifdef _WIN32
static void win_free_tool_paths(char** paths, int path_count)
{
    if (!paths) return;
    for (int i = 0; i < path_count; i++)
    {
        free(paths[i]);
    }
}
#endif

BaaCompilerExitCode driver_toolchain_assemble_one(const CompilerConfig *config,
                                                  CompilerPhaseTimes *times,
                                                  const char *asm_file,
                                                  const char *obj_file)
{
    if (!config || !asm_file || !obj_file) return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    BaaCompilerExitCode rc = BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;

#ifdef _WIN32
    char tool_asm[MAX_PATH] = "";
    char tool_obj[MAX_PATH] = "";
    (void)driver_toolchain_delete_file_utf8(obj_file);
    if (!win_prepare_toolchain_path(asm_file, false, tool_asm, sizeof(tool_asm)) ||
        !win_prepare_toolchain_path(obj_file, true, tool_obj, sizeof(tool_obj)))
    {
        fprintf(stderr,
                "خطأ: أداة التجميع لا تدعم المسار Unicode مباشرة، "
                "ولم يوفر نظام الملفات اسماً قصيراً للملف الحقيقي: %s\n",
                obj_file);
        (void)driver_toolchain_delete_file_utf8(obj_file);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
#else
    const char* tool_asm = asm_file;
    const char* tool_obj = obj_file;
#endif

    const char *argv[8];
    int k = 0;
    argv[k++] = driver_toolchain_get_gcc_command();
    if (config->debug_info) argv[k++] = "-g";
    argv[k++] = "-c";
    argv[k++] = tool_asm;
    argv[k++] = "-o";
    argv[k++] = tool_obj;
    argv[k] = NULL;

    double t0 = 0.0;
    if (times && config->time_phases) t0 = driver_time_seconds();

    BaaProcessResult pr;
    if (!baa_process_run(argv, NULL, &pr) || pr.exit_code != 0)
    {
        fprintf(stderr, "خطأ: فشلت أداة التجميع للملف الحقيقي: %s\n", asm_file);
        goto cleanup;
    }

    if (times && config->time_phases) times->assemble_s += (driver_time_seconds() - t0);
    rc = BAA_COMPILER_EXIT_SUCCESS;

cleanup:
#ifdef _WIN32
    if (rc != BAA_COMPILER_EXIT_SUCCESS)
        (void)driver_toolchain_delete_file_utf8(obj_file);
#endif
    return rc;
}

BaaCompilerExitCode driver_toolchain_link(const CompilerConfig *config,
                                          CompilerPhaseTimes *times,
                                          const char **obj_files,
                                          int obj_count)
{
    if (!config || !config->output_file) return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    if (!obj_files || obj_count <= 0) return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    BaaCompilerExitCode rc = BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;

    // مساحة إضافية للأعلام الاختيارية (debug/pie/startup/runtime/-lm) + -o + output + NULL
    int argv_cap = obj_count + 14;
    const char **argv_link = (const char **)calloc((size_t)argv_cap, sizeof(char *));
    if (!argv_link)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

#ifdef _WIN32
    char **tool_objects = (char **)calloc((size_t)obj_count, sizeof(char *));
    if (!tool_objects)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        free(argv_link);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }
    char tool_output[MAX_PATH] = "";
    bool tool_output_ready = false;
    char tool_runtime[MAX_PATH] = "";
    char entry_response_path[MAX_PATH] = "";
    char tool_entry_response[MAX_PATH] = "";
    char entry_response_argument[MAX_PATH + 8] = "";
    bool entry_response_ready = false;

    bool paths_ok = true;
    for (int i = 0; i < obj_count; i++)
    {
        tool_objects[i] = (char *)calloc((size_t)MAX_PATH, sizeof(char));
        if (!tool_objects[i]) {
            paths_ok = false;
            break;
        }
        if (!win_prepare_toolchain_path(obj_files[i],
                                        false,
                                        tool_objects[i],
                                        MAX_PATH))
        {
            paths_ok = false;
            break;
        }
    }

    (void)driver_toolchain_delete_file_utf8(config->output_file);
    if (!paths_ok ||
        !win_prepare_toolchain_path(config->output_file,
                                    true,
                                    tool_output,
                                    sizeof(tool_output)))
    {
        fprintf(stderr,
                "خطأ: الرابط لا يدعم أحد مسارات Unicode مباشرة، "
                "ولم يوفر نظام الملفات اسماً قصيراً للملف الحقيقي.\n");
        (void)driver_toolchain_delete_file_utf8(config->output_file);
        goto cleanup;
    }
    tool_output_ready = true;
#endif

    const char* runtime_library = driver_toolchain_get_runtime_library();
    if (!runtime_library) {
        fprintf(stderr, "خطأ: لم يتم العثور على مكتبة وقت تشغيل باء libbaa_runtime.a بجانب المصرّف أو في lib/baa.\n");
        goto cleanup;
    }

#ifdef _WIN32
    if (!win_prepare_toolchain_path(runtime_library,
                                    false,
                                    tool_runtime,
                                    sizeof(tool_runtime))) {
        fprintf(stderr,
                "خطأ: تعذر تمرير مكتبة وقت التشغيل الحقيقية إلى الرابط بلا نسخ.\n");
        goto cleanup;
    }
#endif

    int lk = 0;
    argv_link[lk++] = driver_toolchain_get_gcc_command();
    if (config->debug_info) argv_link[lk++] = "-g";
    if (config->codegen_opts.pie && config->target && config->target->obj_format == BAA_OBJFORMAT_ELF)
        argv_link[lk++] = "-pie";
#ifdef _WIN32
    if (!win_make_link_response_path(config->output_file,
                                     entry_response_path,
                                     sizeof(entry_response_path)) ||
        !win_write_entry_response(entry_response_path, "الرئيسية_بدء"))
    {
        fprintf(stderr, "خطأ: فشل تجهيز وسيط نقطة الدخول العربية للرابط.\n");
        goto cleanup;
    }
    entry_response_ready = true;
    if (!win_prepare_toolchain_path(entry_response_path,
                                    false,
                                    tool_entry_response,
                                    sizeof(tool_entry_response)))
    {
        fprintf(stderr,
                "خطأ: تعذر تمرير ملف استجابة نقطة الدخول العربية إلى الرابط.\n");
        goto cleanup;
    }
    int response_chars = snprintf(entry_response_argument,
                                  sizeof(entry_response_argument),
                                  "-Wl,@%s",
                                  tool_entry_response);
    if (response_chars <= 1 ||
        (size_t)response_chars >= sizeof(entry_response_argument))
        goto cleanup;
    argv_link[lk++] = "-nostartfiles";
    argv_link[lk++] = entry_response_argument;
#else
    argv_link[lk++] = "-nostartfiles";
    argv_link[lk++] = "-Wl,-e,الرئيسية_بدء";
#endif

    for (int i = 0; i < obj_count; i++)
    {
#ifdef _WIN32
        argv_link[lk++] = tool_objects[i];
#else
        argv_link[lk++] = obj_files[i];
#endif
    }

#ifdef _WIN32
    argv_link[lk++] = tool_runtime;
#else
    argv_link[lk++] = runtime_library;
#endif

    // ربط libm لدعم دوال الرياضيات القياسية (sqrt/pow) المستخدمة في stdlib v0.4.1.
    argv_link[lk++] = "-lm";
    if (config->target && config->target->obj_format == BAA_OBJFORMAT_COFF)
        argv_link[lk++] = "-lshell32";

    argv_link[lk++] = "-o";
#ifdef _WIN32
    argv_link[lk++] = tool_output;
#else
    argv_link[lk++] = config->output_file;
#endif
    argv_link[lk] = NULL;

    double t0 = 0.0;
    if (times && config->time_phases) t0 = driver_time_seconds();

    BaaProcessResult pr;
    if (!baa_process_run(argv_link, NULL, &pr) || pr.exit_code != 0)
    {
        printf("Error: Linker failed.\n");
        goto cleanup;
    }

    if (times && config->time_phases) times->link_s += (driver_time_seconds() - t0);
    rc = BAA_COMPILER_EXIT_SUCCESS;

cleanup:
#ifdef _WIN32
    win_free_tool_paths(tool_objects, obj_count);
    free(tool_objects);
    if (entry_response_ready)
        (void)driver_toolchain_delete_file_utf8(entry_response_path);
    if (tool_output_ready && rc != BAA_COMPILER_EXIT_SUCCESS)
        (void)driver_toolchain_delete_file_utf8(config->output_file);
#endif
    free(argv_link);
    return rc;
}
