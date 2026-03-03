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
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define BAA_STAGE_BASENAME "baa_stage"
#define BAA_STAGE_BASENAME_W L"baa_stage"

// ============================================================================
// البحث عن GCC المرفق (اكتشاف GCC المضمّن)
// ============================================================================

/**
 * @brief مسار GCC المكتشف (فارغ = استخدام "gcc" من PATH).
 */
static char g_gcc_path[MAX_PATH] = "";

#ifdef _WIN32

#define BAA_PATH_SUFFIX_GCC_BIN_W L"\\gcc\\bin\\gcc.exe"
#define BAA_PATH_SUFFIX_GCC_BIN_DEV_W L"\\..\\gcc\\bin\\gcc.exe"

/**
 * @brief مسار تجهيزي ASCII لتشغيل أدوات GCC/LD بعيداً عن قيود Unicode.
 */
static char g_stage_dir[MAX_PATH] = "";
static unsigned long g_stage_counter = 0;

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

static bool win_join_path_w(wchar_t* out, size_t out_cap, const wchar_t* base, const wchar_t* leaf)
{
    if (!out || out_cap == 0 || !base || !leaf) return false;
    size_t base_len = wcslen(base);
    size_t leaf_len = wcslen(leaf);
    bool need_sep = (base_len > 0 && base[base_len - 1] != L'\\' && base[base_len - 1] != L'/');
    size_t total = base_len + (need_sep ? 1u : 0u) + leaf_len + 1u;
    if (total > out_cap) return false;

    memcpy(out, base, base_len * sizeof(wchar_t));
    size_t pos = base_len;
    if (need_sep) out[pos++] = L'\\';
    memcpy(out + pos, leaf, (leaf_len + 1) * sizeof(wchar_t));
    return true;
}

static bool win_try_prepare_stage_dir(const wchar_t* base_dir, char* out_utf8, size_t out_cap)
{
    if (!base_dir || !out_utf8 || out_cap == 0) return false;

    wchar_t candidate[MAX_PATH];
    if (!win_join_path_w(candidate, MAX_PATH, base_dir, BAA_STAGE_BASENAME_W)) {
        return false;
    }

    if (!CreateDirectoryW(candidate, NULL)) {
        DWORD e = GetLastError();
        if (e != ERROR_ALREADY_EXISTS) return false;
    }

    wchar_t resolved[MAX_PATH];
    DWORD short_len = GetShortPathNameW(candidate, resolved, MAX_PATH);
    const wchar_t* best = (short_len > 0 && short_len < MAX_PATH) ? resolved : candidate;

    char best_utf8[MAX_PATH];
    if (!win_wide_to_utf8(best, best_utf8, sizeof(best_utf8))) return false;
    if (!win_utf8_is_ascii(best_utf8)) return false;

    size_t n = strlen(best_utf8);
    if (n + 1 > out_cap) return false;
    memcpy(out_utf8, best_utf8, n + 1);
    return true;
}

static bool win_ensure_stage_dir(void)
{
    if (g_stage_dir[0]) return true;

    wchar_t temp_dir[MAX_PATH];
    DWORD temp_len = GetTempPathW(MAX_PATH, temp_dir);
    if (temp_len > 0 && temp_len < MAX_PATH) {
        if (win_try_prepare_stage_dir(temp_dir, g_stage_dir, sizeof(g_stage_dir))) return true;
    }

    wchar_t public_dir[MAX_PATH];
    DWORD pub_len = GetEnvironmentVariableW(L"PUBLIC", public_dir, MAX_PATH);
    if (pub_len > 0 && pub_len < MAX_PATH) {
        if (win_try_prepare_stage_dir(public_dir, g_stage_dir, sizeof(g_stage_dir))) return true;
    }

    wchar_t exe_path[MAX_PATH];
    DWORD exe_len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (exe_len > 0 && exe_len < MAX_PATH) {
        wchar_t* last_sep = wcsrchr(exe_path, L'\\');
        if (last_sep) {
            *last_sep = L'\0';
            if (win_try_prepare_stage_dir(exe_path, g_stage_dir, sizeof(g_stage_dir))) return true;
        }
    }

    return false;
}

static bool win_make_stage_file_path(const char* prefix, const char* ext, char* out, size_t out_cap)
{
    if (!prefix || !ext || !out || out_cap == 0) return false;
    if (!win_ensure_stage_dir()) return false;

    unsigned long id = ++g_stage_counter;
    int n = snprintf(out, out_cap, "%s\\%s_%lu%s", g_stage_dir, prefix, id, ext);
    return n > 0 && (size_t)n < out_cap;
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
    FILE* in_f = fopen(src_path, "rb");
    if (!in_f) return false;

    FILE* out_f = fopen(dst_path, "wb");
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
static void win_cleanup_stage_pair(const char* stage_asm, const char* stage_obj)
{
    if (stage_asm && stage_asm[0]) (void)driver_toolchain_delete_file_utf8(stage_asm);
    if (stage_obj && stage_obj[0]) (void)driver_toolchain_delete_file_utf8(stage_obj);
}

static void win_cleanup_staged_objects(char** staged_objects, int obj_count)
{
    if (!staged_objects) return;
    for (int i = 0; i < obj_count; i++)
    {
        if (!staged_objects[i]) continue;
        (void)driver_toolchain_delete_file_utf8(staged_objects[i]);
        free(staged_objects[i]);
    }
}
#endif

int driver_toolchain_assemble_one(const CompilerConfig *config,
                                 CompilerPhaseTimes *times,
                                 const char *asm_file,
                                 const char *obj_file)
{
    if (!config || !asm_file || !obj_file) return 1;
    int rc = 1;

#ifdef _WIN32
    char stage_asm[MAX_PATH] = "";
    char stage_obj[MAX_PATH] = "";
    bool stage_paths_ready = false;
    if (!win_make_stage_file_path("unit", ".s", stage_asm, sizeof(stage_asm)) ||
        !win_make_stage_file_path("unit", ".o", stage_obj, sizeof(stage_obj)))
    {
        fprintf(stderr, "خطأ: فشل إنشاء مسار staging ASCII للتجميع.\n");
        return 1;
    }
    stage_paths_ready = true;

    if (!driver_toolchain_copy_file_utf8(asm_file, stage_asm)) {
        fprintf(stderr, "خطأ: فشل نسخ ملف التجميع إلى staging: %s\n", asm_file);
        goto cleanup;
    }
#else
    const char* stage_asm = asm_file;
    const char* stage_obj = obj_file;
#endif

    const char *argv[8];
    int k = 0;
    argv[k++] = driver_toolchain_get_gcc_command();
    if (config->debug_info) argv[k++] = "-g";
    argv[k++] = "-c";
    argv[k++] = stage_asm;
    argv[k++] = "-o";
    argv[k++] = stage_obj;
    argv[k] = NULL;

    double t0 = 0.0;
    if (times && config->time_phases) t0 = driver_time_seconds();

    BaaProcessResult pr;
    if (!baa_process_run(argv, NULL, &pr) || pr.exit_code != 0)
    {
        printf("Error: Assembler failed for %s\n", stage_asm);
        goto cleanup;
    }

#ifdef _WIN32
    if (!driver_toolchain_copy_file_utf8(stage_obj, obj_file)) {
        fprintf(stderr, "خطأ: فشل نسخ ناتج الكائن من staging إلى المسار الهدف: %s\n", obj_file);
        goto cleanup;
    }
#endif

    if (times && config->time_phases) times->assemble_s += (driver_time_seconds() - t0);
    rc = 0;

cleanup:
#ifdef _WIN32
    if (stage_paths_ready) win_cleanup_stage_pair(stage_asm, stage_obj);
#endif
    return rc;
}

int driver_toolchain_link(const CompilerConfig *config,
                          CompilerPhaseTimes *times,
                          const char **obj_files,
                          int obj_count)
{
    if (!config || !config->output_file) return 1;
    if (!obj_files || obj_count <= 0) return 1;
    int rc = 1;

    // مساحة إضافية للأعلام الاختيارية (debug/pie/startup/-lm) + -o + output + NULL
    int argv_cap = obj_count + 13;
    const char **argv_link = (const char **)calloc((size_t)argv_cap, sizeof(char *));
    if (!argv_link)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        return 1;
    }

#ifdef _WIN32
    char **staged_objects = (char **)calloc((size_t)obj_count, sizeof(char *));
    if (!staged_objects)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        free(argv_link);
        return 1;
    }
    char staged_output[MAX_PATH] = "";
    bool staged_output_ready = false;

    bool staged_ok = true;
    for (int i = 0; i < obj_count; i++)
    {
        staged_objects[i] = (char *)calloc((size_t)MAX_PATH, sizeof(char));
        if (!staged_objects[i]) {
            staged_ok = false;
            break;
        }
        if (!win_make_stage_file_path("link_obj", ".o", staged_objects[i], MAX_PATH) ||
            !driver_toolchain_copy_file_utf8(obj_files[i], staged_objects[i]))
        {
            staged_ok = false;
            break;
        }
    }

    if (!staged_ok || !win_make_stage_file_path("linked", ".exe", staged_output, sizeof(staged_output)))
    {
        fprintf(stderr, "خطأ: فشل تجهيز مسارات staging ASCII لعملية الربط.\n");
        goto cleanup;
    }
    staged_output_ready = true;
#endif

    int lk = 0;
    argv_link[lk++] = driver_toolchain_get_gcc_command();
    if (config->debug_info) argv_link[lk++] = "-g";
    if (config->codegen_opts.pie && config->target && config->target->obj_format == BAA_OBJFORMAT_ELF)
        argv_link[lk++] = "-pie";
    if (config->custom_startup)
        argv_link[lk++] = "-Wl,-e,__baa_start";

    for (int i = 0; i < obj_count; i++)
    {
#ifdef _WIN32
        argv_link[lk++] = staged_objects[i];
#else
        argv_link[lk++] = obj_files[i];
#endif
    }

    // ربط libm لدعم دوال الرياضيات القياسية (sqrt/pow) المستخدمة في stdlib v0.4.1.
    argv_link[lk++] = "-lm";

    argv_link[lk++] = "-o";
#ifdef _WIN32
    argv_link[lk++] = staged_output;
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

#ifdef _WIN32
    if (!driver_toolchain_copy_file_utf8(staged_output, config->output_file))
    {
        fprintf(stderr, "خطأ: فشل نسخ الملف التنفيذي من staging إلى المسار الهدف: %s\n", config->output_file);
        goto cleanup;
    }
#endif

    if (times && config->time_phases) times->link_s += (driver_time_seconds() - t0);
    rc = 0;

cleanup:
#ifdef _WIN32
    win_cleanup_staged_objects(staged_objects, obj_count);
    free(staged_objects);
    if (staged_output_ready) (void)driver_toolchain_delete_file_utf8(staged_output);
#endif
    free(argv_link);
    return rc;
}
