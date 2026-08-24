/**
 * @file driver_toolchain_impl.inc
 * @brief تشغيل أدوات التجميع/الربط واكتشاف GCC المضمّن.
 * @version 0.3.4
 */

#include "driver_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
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
#ifdef _WIN32
static bool g_gcc_direct_unicode_paths = false;
static bool g_gcc_retain_pei386_runtime_relocator = false;
static bool g_gcc_directory_alias_failed = false;
static bool g_gcc_manifest_valid = false;
static char g_gcc_directory_alias[MAX_PATH] = "";
#define BAA_GCC_ALIAS_ARGUMENT_COUNT 5
static char g_gcc_manifest_target[64] = "";
static char g_gcc_manifest_version[32] = "";
static char g_gcc_alias_arguments[BAA_GCC_ALIAS_ARGUMENT_COUNT][MAX_PATH + 32] = {{0}};
static int g_gcc_alias_argument_count = 0;
#endif

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
#define BAA_PORTABLE_TOOLCHAIN_MANIFEST "BAA-TOOLCHAIN-MANIFEST.txt"

static unsigned long g_toolchain_file_counter = 0;

/**
 * @brief هل تستخدم صفحة محارف Windows النشطة UTF-8؟
 *
 * CreateProcessW يحفظ سطر الأمر كاملاً، لكن أدوات MinGW القديمة قد تعيد تحويل
 * argv إلى صفحة المحارف النشطة قبل أن يصل المسار إلى GCC/LD. قابلية تمثيل
 * العربية في صفحة ANSI مثل 1256 لا تكفي: بعض أدوات GNU تعيد تفسير البايتات
 * الضيقة على أنها UTF-8. لذلك يقتصر المسار المباشر على CP_UTF8.
 */
static bool win_active_code_page_is_utf8(void)
{
    return GetACP() == CP_UTF8;
}

static bool win_portable_toolchain_allows_direct_unicode(const char* gcc_path)
{
    if (!gcc_path || !gcc_path[0]) return false;

    char gcc_bin[MAX_PATH];
    size_t gcc_path_size = strlen(gcc_path) + 1u;
    if (gcc_path_size > sizeof(gcc_bin)) return false;
    memcpy(gcc_bin, gcc_path, gcc_path_size);

    char* forward = strrchr(gcc_bin, '/');
    char* backward = strrchr(gcc_bin, '\\');
    char* separator = forward;
    if (!separator || (backward && backward > separator)) separator = backward;
    if (!separator) return false;
    *separator = '\0';

    char manifest[MAX_PATH];
    int count = snprintf(manifest,
                         sizeof(manifest),
                         "%s/../%s",
                         gcc_bin,
                         BAA_PORTABLE_TOOLCHAIN_MANIFEST);
    if (count <= 0 || (size_t)count >= sizeof(manifest)) return false;

    FILE* file = baa_fopen_utf8(manifest, "rb");
    if (!file) return false;

    bool format_ok = false;
    bool unicode_ok = false;
    bool relocator_ok = false;
    char line[160];
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (strcmp(line, "format=baa-portable-toolchain-v1") == 0)
            format_ok = true;
        else if (strcmp(line, "unicode_paths=direct") == 0)
            unicode_ok = true;
        else if (strcmp(line, "pei386_runtime_relocator=retain") == 0)
            relocator_ok = true;
        else if (strncmp(line, "target=", 7) == 0) {
            size_t value_size = strlen(line + 7) + 1u;
            if (value_size <= sizeof(g_gcc_manifest_target))
                memcpy(g_gcc_manifest_target, line + 7, value_size);
        }
        else if (strncmp(line, "gcc_version=", 12) == 0) {
            size_t value_size = strlen(line + 12) + 1u;
            if (value_size <= sizeof(g_gcc_manifest_version))
                memcpy(g_gcc_manifest_version, line + 12, value_size);
        }
    }
    fclose(file);
    g_gcc_manifest_valid = format_ok;
    g_gcc_retain_pei386_runtime_relocator = format_ok && relocator_ok;
    return format_ok && unicode_ok && win_active_code_page_is_utf8();
}

static bool win_wide_to_utf8(const wchar_t* text, char* out, size_t out_cap)
{
    if (!text || !out || out_cap == 0) return false;

    int need = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (need <= 0) return false;
    if ((size_t)need > out_cap) return false;

    return WideCharToMultiByte(CP_UTF8, 0, text, -1, out, (int)out_cap, NULL, NULL) > 0;
}

static bool win_extended_wide_to_utf8(const wchar_t* text,
                                      char* out,
                                      size_t out_cap)
{
    if (!text) return false;
    if (wcsncmp(text, L"\\\\?\\UNC\\", 8) == 0)
    {
        size_t suffix_len = wcslen(text + 8);
        wchar_t* unc =
            (wchar_t*)malloc((suffix_len + 3u) * sizeof(wchar_t));
        if (!unc) return false;
        unc[0] = L'\\';
        unc[1] = L'\\';
        memcpy(unc + 2,
               text + 8,
               (suffix_len + 1u) * sizeof(wchar_t));
        bool ok = win_wide_to_utf8(unc, out, out_cap);
        free(unc);
        return ok;
    }
    if (wcsncmp(text, L"\\\\?\\", 4) == 0)
        text += 4;
    return win_wide_to_utf8(text, out, out_cap);
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

#define BAA_WINDOWS_WIDE_PATH_CAP 32768u

static bool win_join_wide_path(wchar_t* out,
                               size_t out_cap,
                               const wchar_t* directory,
                               const wchar_t* leaf)
{
    if (!out || out_cap == 0 || !directory || !leaf) return false;
    size_t directory_len = wcslen(directory);
    size_t leaf_len = wcslen(leaf);
    bool needs_separator = directory_len > 0 &&
                           directory[directory_len - 1] != L'\\' &&
                           directory[directory_len - 1] != L'/';
    size_t need = directory_len + (needs_separator ? 1u : 0u) + leaf_len + 1u;
    if (need > out_cap) return false;

    memcpy(out, directory, directory_len * sizeof(wchar_t));
    size_t offset = directory_len;
    if (needs_separator) out[offset++] = L'\\';
    memcpy(out + offset, leaf, (leaf_len + 1u) * sizeof(wchar_t));
    return true;
}

static void win_toolchain_extension_w(const char* path,
                                      wchar_t* out,
                                      size_t out_cap)
{
    static const wchar_t fallback[] = L".tmp";
    if (!out || out_cap < sizeof(fallback) / sizeof(fallback[0])) return;
    memcpy(out, fallback, sizeof(fallback));
    if (!path) return;

    const char* leaf = strrchr(path, '/');
    const char* backward = strrchr(path, '\\');
    if (!leaf || (backward && backward > leaf)) leaf = backward;
    leaf = leaf ? leaf + 1 : path;
    const char* dot = strrchr(leaf, '.');
    if (!dot || !dot[1]) return;

    size_t length = strlen(dot);
    if (length + 1u > out_cap) return;
    for (size_t i = 0; i < length; i++) {
        unsigned char byte = (unsigned char)dot[i];
        if (byte >= 0x80u || (i > 0 && !((byte >= 'a' && byte <= 'z') ||
                                         (byte >= 'A' && byte <= 'Z') ||
                                         (byte >= '0' && byte <= '9'))))
            return;
        out[i] = (wchar_t)byte;
    }
    out[length] = L'\0';
}

static bool win_try_hardlink_alias_in_directory(const wchar_t* directory,
                                                const wchar_t* real_path,
                                                const wchar_t* alias_directory_name,
                                                const wchar_t* alias_file_name,
                                                char* out,
                                                size_t out_cap)
{
    wchar_t alias_directory[BAA_WINDOWS_WIDE_PATH_CAP];
    wchar_t alias_path[BAA_WINDOWS_WIDE_PATH_CAP];
    if (!win_join_wide_path(alias_directory,
                            BAA_WINDOWS_WIDE_PATH_CAP,
                            directory,
                            alias_directory_name) ||
        !CreateDirectoryW(alias_directory, NULL))
        return false;

    if (!win_join_wide_path(alias_path,
                            BAA_WINDOWS_WIDE_PATH_CAP,
                            alias_directory,
                            alias_file_name) ||
        !CreateHardLinkW(alias_path, real_path, NULL))
    {
        (void)RemoveDirectoryW(alias_directory);
        return false;
    }

    bool converted = win_extended_wide_to_utf8(alias_path, out, out_cap) &&
                     win_utf8_is_ascii(out);
    if (!converted) {
        (void)DeleteFileW(alias_path);
        (void)RemoveDirectoryW(alias_directory);
    }
    return converted;
}

/**
 * @brief إنشاء اسم ASCII صلب لنفس كيان الملف عندما يغيب اسم 8.3.
 */
static bool win_create_hardlink_alias(const wchar_t* real_path,
                                      const char* original_path,
                                      char* out,
                                      size_t out_cap)
{
    if (!real_path || !original_path || !out || out_cap == 0) return false;

    unsigned long process_id = (unsigned long)GetCurrentProcessId();
    unsigned long alias_id = ++g_toolchain_file_counter;
    wchar_t extension[16];
    win_toolchain_extension_w(original_path,
                              extension,
                              sizeof(extension) / sizeof(extension[0]));

    wchar_t alias_directory_name[80];
    wchar_t alias_file_name[32];
    int directory_count = swprintf(alias_directory_name,
                                   sizeof(alias_directory_name) /
                                       sizeof(alias_directory_name[0]),
                                   L"baa_alias_%lu_%lu",
                                   process_id,
                                   alias_id);
    int file_count = swprintf(alias_file_name,
                              sizeof(alias_file_name) / sizeof(alias_file_name[0]),
                              L"artifact%ls",
                              extension);
    if (directory_count <= 0 || file_count <= 0) return false;

    wchar_t temp_directory[BAA_WINDOWS_WIDE_PATH_CAP];
    DWORD temp_length = GetTempPathW(BAA_WINDOWS_WIDE_PATH_CAP, temp_directory);
    if (temp_length > 0 && temp_length < BAA_WINDOWS_WIDE_PATH_CAP &&
        win_try_hardlink_alias_in_directory(temp_directory,
                                            real_path,
                                            alias_directory_name,
                                            alias_file_name,
                                            out,
                                            out_cap))
        return true;

    size_t ancestor_bytes = (wcslen(real_path) + 1u) * sizeof(wchar_t);
    wchar_t* ancestor = (wchar_t*)malloc(ancestor_bytes);
    if (!ancestor) return false;
    memcpy(ancestor, real_path, ancestor_bytes);
    wchar_t* separator = wcsrchr(ancestor, L'\\');
    if (separator) *separator = L'\0';

    bool created = false;
    while (ancestor[0]) {
        char ancestor_utf8[MAX_PATH];
        if (win_extended_wide_to_utf8(ancestor,
                                      ancestor_utf8,
                                      sizeof(ancestor_utf8)) &&
            win_utf8_is_ascii(ancestor_utf8) &&
            win_try_hardlink_alias_in_directory(ancestor,
                                                real_path,
                                                alias_directory_name,
                                                alias_file_name,
                                                out,
                                                out_cap))
        {
            created = true;
            break;
        }

        separator = wcsrchr(ancestor, L'\\');
        if (!separator || separator <= ancestor + 6) break;
        *separator = L'\0';
    }
    free(ancestor);
    return created;
}

static void win_cleanup_hardlink_alias(const char* path, bool created)
{
    if (!created || !path || !path[0]) return;
    wchar_t* path_w = baa_windows_extended_path_utf8(path);
    if (!path_w) return;
    (void)DeleteFileW(path_w);
    wchar_t* separator = wcsrchr(path_w, L'\\');
    if (separator) {
        *separator = L'\0';
        (void)RemoveDirectoryW(path_w);
    }
    free(path_w);
}

typedef struct BaaMountPointReparseData {
    ULONG reparse_tag;
    USHORT reparse_data_length;
    USHORT reserved;
    USHORT substitute_name_offset;
    USHORT substitute_name_length;
    USHORT print_name_offset;
    USHORT print_name_length;
    wchar_t path_buffer[1];
} BaaMountPointReparseData;

static bool win_set_directory_junction(const wchar_t* junction,
                                       const wchar_t* target)
{
    if (!junction || !target) return false;
    const wchar_t* target_body = target;
    const wchar_t* substitute_prefix = L"\\??\\";
    if (wcsncmp(target, L"\\\\?\\UNC\\", 8) == 0) {
        target_body = target + 8;
        substitute_prefix = L"\\??\\UNC\\";
    } else if (wcsncmp(target, L"\\\\?\\", 4) == 0) {
        target_body = target + 4;
    } else if (wcsncmp(target, L"\\\\", 2) == 0) {
        target_body = target + 2;
        substitute_prefix = L"\\??\\UNC\\";
    }

    size_t prefix_len = wcslen(substitute_prefix);
    size_t target_len = wcslen(target_body);
    size_t substitute_len = prefix_len + target_len;
    size_t print_len = wcslen(target);
    size_t path_chars = substitute_len + 1u + print_len + 1u;
    size_t buffer_size = offsetof(BaaMountPointReparseData, path_buffer) +
                         path_chars * sizeof(wchar_t);
    if (buffer_size > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) return false;

    BaaMountPointReparseData* data =
        (BaaMountPointReparseData*)calloc(1u, buffer_size);
    if (!data) return false;
    data->reparse_tag = IO_REPARSE_TAG_MOUNT_POINT;
    data->substitute_name_offset = 0;
    data->substitute_name_length = (USHORT)(substitute_len * sizeof(wchar_t));
    data->print_name_offset =
        (USHORT)((substitute_len + 1u) * sizeof(wchar_t));
    data->print_name_length = (USHORT)(print_len * sizeof(wchar_t));
    data->reparse_data_length =
        (USHORT)(8u + path_chars * sizeof(wchar_t));
    memcpy(data->path_buffer,
           substitute_prefix,
           prefix_len * sizeof(wchar_t));
    memcpy(data->path_buffer + prefix_len,
           target_body,
           (target_len + 1u) * sizeof(wchar_t));
    memcpy(data->path_buffer + substitute_len + 1u,
           target,
           (print_len + 1u) * sizeof(wchar_t));

    HANDLE directory = CreateFileW(junction,
                                   GENERIC_WRITE,
                                   0,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_FLAG_OPEN_REPARSE_POINT |
                                       FILE_FLAG_BACKUP_SEMANTICS,
                                   NULL);
    if (directory == INVALID_HANDLE_VALUE) {
        free(data);
        return false;
    }
    DWORD returned = 0;
    BOOL ok = DeviceIoControl(directory,
                              FSCTL_SET_REPARSE_POINT,
                              data,
                              (DWORD)buffer_size,
                              NULL,
                              0,
                              &returned,
                              NULL);
    CloseHandle(directory);
    free(data);
    return ok ? true : false;
}

static bool win_try_gcc_junction_in_directory(const wchar_t* directory,
                                              const wchar_t* target,
                                              const wchar_t* alias_name,
                                              char* out,
                                              size_t out_cap)
{
    wchar_t alias_path[BAA_WINDOWS_WIDE_PATH_CAP];
    if (!win_join_wide_path(alias_path,
                            BAA_WINDOWS_WIDE_PATH_CAP,
                            directory,
                            alias_name) ||
        !CreateDirectoryW(alias_path, NULL))
        return false;

    bool created = win_set_directory_junction(alias_path, target) &&
                   win_extended_wide_to_utf8(alias_path, out, out_cap) &&
                   win_utf8_is_ascii(out);
    if (!created) (void)RemoveDirectoryW(alias_path);
    return created;
}

static void win_cleanup_gcc_directory_alias(void)
{
    if (!g_gcc_directory_alias[0]) return;
    wchar_t* alias_w = baa_windows_extended_path_utf8(g_gcc_directory_alias);
    if (alias_w) {
        (void)RemoveDirectoryW(alias_w);
        free(alias_w);
    }
    g_gcc_directory_alias[0] = '\0';
}

/**
 * @brief تقديم جذر GCC ذي المسار Unicode عبر Junction ASCII بلا نسخ.
 */
static bool win_create_gcc_directory_alias(const wchar_t* gcc_path,
                                           char* out,
                                           size_t out_cap)
{
    if (!gcc_path || !out || out_cap == 0) return false;
    wchar_t target[BAA_WINDOWS_WIDE_PATH_CAP];
    size_t gcc_length = wcslen(gcc_path);
    if (gcc_length + 1u > BAA_WINDOWS_WIDE_PATH_CAP) return false;
    memcpy(target, gcc_path, (gcc_length + 1u) * sizeof(wchar_t));

    wchar_t* separator = wcsrchr(target, L'\\');
    if (!separator) return false;
    *separator = L'\0';
    separator = wcsrchr(target, L'\\');
    if (!separator) return false;
    *separator = L'\0';

    unsigned long process_id = (unsigned long)GetCurrentProcessId();
    unsigned long alias_id = ++g_toolchain_file_counter;
    wchar_t alias_name[80];
    int alias_count = swprintf(alias_name,
                               sizeof(alias_name) / sizeof(alias_name[0]),
                               L"baa_gcc_%lu_%lu",
                               process_id,
                               alias_id);
    if (alias_count <= 0) return false;

    wchar_t* ancestor = (wchar_t*)malloc((wcslen(target) + 1u) * sizeof(wchar_t));
    if (!ancestor) return false;
    wcscpy(ancestor, target);
    separator = wcsrchr(ancestor, L'\\');
    if (separator) *separator = L'\0';

    bool created = false;
    while (ancestor[0]) {
        char ancestor_utf8[MAX_PATH];
        if (win_extended_wide_to_utf8(ancestor,
                                      ancestor_utf8,
                                      sizeof(ancestor_utf8)) &&
            win_utf8_is_ascii(ancestor_utf8) &&
            win_try_gcc_junction_in_directory(ancestor,
                                              target,
                                              alias_name,
                                              out,
                                              out_cap))
        {
            created = true;
            break;
        }
        separator = wcsrchr(ancestor, L'\\');
        if (!separator || separator <= ancestor + 6) break;
        *separator = L'\0';
    }
    free(ancestor);

    if (!created) {
        wchar_t temp_directory[BAA_WINDOWS_WIDE_PATH_CAP];
        DWORD temp_length = GetTempPathW(BAA_WINDOWS_WIDE_PATH_CAP,
                                         temp_directory);
        char temp_utf8[MAX_PATH];
        if (temp_length > 0 && temp_length < BAA_WINDOWS_WIDE_PATH_CAP &&
            win_extended_wide_to_utf8(temp_directory,
                                      temp_utf8,
                                      sizeof(temp_utf8)) &&
            win_utf8_is_ascii(temp_utf8))
        {
            created = win_try_gcc_junction_in_directory(temp_directory,
                                                        target,
                                                        alias_name,
                                                        out,
                                                        out_cap);
        }
    }
    if (!created) return false;

    size_t alias_length = strlen(out);
    static const char suffix[] = "/bin/gcc.exe";
    if (alias_length + sizeof(suffix) > out_cap) {
        wchar_t* alias_w = baa_windows_extended_path_utf8(out);
        if (alias_w) {
            (void)RemoveDirectoryW(alias_w);
            free(alias_w);
        }
        return false;
    }
    memcpy(g_gcc_directory_alias, out, alias_length + 1u);
    memcpy(out + alias_length, suffix, sizeof(suffix));
    (void)atexit(win_cleanup_gcc_directory_alias);
    return true;
}

static bool win_prepare_gcc_alias_arguments(void)
{
    if (!g_gcc_manifest_valid ||
        !g_gcc_directory_alias[0] ||
        !g_gcc_manifest_target[0] ||
        !g_gcc_manifest_version[0])
        return false;

    for (int i = 0; i < BAA_GCC_ALIAS_ARGUMENT_COUNT; i++) {
        int count = -1;
        if (i == 0) {
            count = snprintf(g_gcc_alias_arguments[i],
                             sizeof(g_gcc_alias_arguments[i]),
                             "-B%s/libexec/gcc/%s/%s/",
                             g_gcc_directory_alias,
                             g_gcc_manifest_target,
                             g_gcc_manifest_version);
        } else if (i == 1) {
            count = snprintf(g_gcc_alias_arguments[i],
                             sizeof(g_gcc_alias_arguments[i]),
                             "-B%s/%s/bin/",
                             g_gcc_directory_alias,
                             g_gcc_manifest_target);
        } else if (i == 2) {
            count = snprintf(g_gcc_alias_arguments[i],
                             sizeof(g_gcc_alias_arguments[i]),
                             "-L%s/lib/gcc/%s/%s",
                             g_gcc_directory_alias,
                             g_gcc_manifest_target,
                             g_gcc_manifest_version);
        } else if (i == 3) {
            count = snprintf(g_gcc_alias_arguments[i],
                             sizeof(g_gcc_alias_arguments[i]),
                             "-L%s/%s/lib",
                             g_gcc_directory_alias,
                             g_gcc_manifest_target);
        } else {
            count = snprintf(g_gcc_alias_arguments[i],
                             sizeof(g_gcc_alias_arguments[i]),
                             "-L%s/lib",
                             g_gcc_directory_alias);
        }
        if (count <= 0 ||
            (size_t)count >= sizeof(g_gcc_alias_arguments[i]))
        {
            g_gcc_alias_argument_count = 0;
            return false;
        }
    }
    g_gcc_alias_argument_count = BAA_GCC_ALIAS_ARGUMENT_COUNT;
    return true;
}

static bool win_select_gcc_candidate(const wchar_t* candidate)
{
    char candidate_utf8[MAX_PATH];
    if (!candidate ||
        !win_wide_to_utf8(candidate, candidate_utf8, sizeof(candidate_utf8)))
        return false;

    g_gcc_direct_unicode_paths =
        win_portable_toolchain_allows_direct_unicode(candidate_utf8);
    if (!g_gcc_direct_unicode_paths && !win_utf8_is_ascii(candidate_utf8)) {
        if (win_create_gcc_directory_alias(candidate,
                                           g_gcc_path,
                                           sizeof(g_gcc_path))) {
            if (win_prepare_gcc_alias_arguments()) return true;
            win_cleanup_gcc_directory_alias();
        }
        g_gcc_directory_alias_failed = true;
    }
    return win_copy_path_text(candidate_utf8,
                              g_gcc_path,
                              sizeof(g_gcc_path));
}

static bool win_toolchain_path_is_long(const char* path)
{
    wchar_t* absolute = baa_windows_extended_path_utf8(path);
    if (!absolute) return true;
    const wchar_t* measured = absolute;
    if (wcsncmp(absolute, L"\\\\?\\UNC\\", 8) == 0)
        measured = absolute + 6;
    else if (wcsncmp(absolute, L"\\\\?\\", 4) == 0)
        measured = absolute + 4;
    bool is_long = wcslen(measured) >= 240u;
    free(absolute);
    return is_long;
}

/**
 * @brief تجهيز مسار الأداة إلى الملف الحقيقي بلا نسخ.
 *
 * GCC المرفق حالياً لا يفتح مسارات Unicode من argv. عند الحاجة نستخدم الاسم
 * القصير لنفس كيان NTFS، أو رابطاً صلباً مؤقتاً للكيان نفسه إذا غاب اسم 8.3؛
 * لذلك تقرأ الأداة الملف العربي الحقيقي أو تكتب فيه مباشرة بلا نسخ.
 */
static bool win_prepare_toolchain_path(const char* path,
                                       bool prepare_output,
                                       char* out,
                                       size_t out_cap,
                                       bool* created_hardlink)
{
    if (!path || !out || out_cap == 0) return false;
    if (created_hardlink) *created_hardlink = false;
    bool is_long = win_toolchain_path_is_long(path);
    if (g_gcc_direct_unicode_paths && !is_long)
        return win_copy_path_text(path, out, out_cap);

    bool needs_alias = !win_utf8_is_ascii(path) ||
                       is_long ||
                       strpbrk(path, " \t") != NULL;
    if (!needs_alias) return win_copy_path_text(path, out, out_cap);

    wchar_t* path_w = baa_windows_extended_path_utf8(path);
    if (!path_w) return false;
    if (prepare_output && !win_prepare_output_file(path_w)) {
        free(path_w);
        return false;
    }

    DWORD short_need = GetShortPathNameW(path_w, NULL, 0);
    if (short_need > 0) {
        wchar_t* short_w =
            (wchar_t*)calloc((size_t)short_need + 1u, sizeof(wchar_t));
        if (short_w) {
            DWORD short_len = GetShortPathNameW(path_w, short_w, short_need);
            bool converted = short_len > 0 && short_len < short_need &&
                             win_extended_wide_to_utf8(short_w, out, out_cap) &&
                             win_utf8_is_ascii(out);
            free(short_w);
            if (converted) {
                free(path_w);
                return true;
            }
        }
    }

    bool created = win_create_hardlink_alias(path_w, path, out, out_cap);
    free(path_w);
    if (created_hardlink) *created_hardlink = created;
    return created;
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
    win_cleanup_gcc_directory_alias();
    g_gcc_direct_unicode_paths = false;
    g_gcc_retain_pei386_runtime_relocator = false;
    g_gcc_directory_alias_failed = false;
    g_gcc_manifest_valid = false;
    g_gcc_manifest_target[0] = '\0';
    g_gcc_manifest_version[0] = '\0';
    g_gcc_alias_argument_count = 0;
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
        win_select_gcc_candidate(candidate))
    {
        return;
    }

    // المسار 2: <baa_dir>\..\gcc\bin\gcc.exe (هيكل التطوير)
    if (path_build_suffix_w(candidate, MAX_PATH, exe_path, BAA_PATH_SUFFIX_GCC_BIN_DEV_W) &&
        GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES &&
        win_select_gcc_candidate(candidate))
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
    wchar_t* src_w = baa_windows_extended_path_utf8(src_path);
    wchar_t* dst_w = baa_windows_extended_path_utf8(dst_path);
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
    wchar_t* path_w = baa_windows_extended_path_utf8(path);
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
#ifdef _WIN32
    if (g_gcc_directory_alias_failed) {
        fprintf(stderr,
                "خطأ: تعذر تقديم جذر GCC ذي المسار Unicode عبر alias بلا نسخ.\n");
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
#endif
    BaaCompilerExitCode rc = BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;

#ifdef _WIN32
    char tool_asm[MAX_PATH] = "";
    char tool_obj[MAX_PATH] = "";
    bool tool_asm_hardlink = false;
    bool tool_obj_hardlink = false;
    (void)driver_toolchain_delete_file_utf8(obj_file);
    if (!win_prepare_toolchain_path(asm_file,
                                    false,
                                    tool_asm,
                                    sizeof(tool_asm),
                                    &tool_asm_hardlink) ||
        !win_prepare_toolchain_path(obj_file,
                                    true,
                                    tool_obj,
                                    sizeof(tool_obj),
                                    &tool_obj_hardlink))
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

    const char *argv[16];
    int k = 0;
    argv[k++] = driver_toolchain_get_gcc_command();
#ifdef _WIN32
    for (int i = 0; i < g_gcc_alias_argument_count; i++)
        argv[k++] = g_gcc_alias_arguments[i];
#endif
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
    win_cleanup_hardlink_alias(tool_asm, tool_asm_hardlink);
    win_cleanup_hardlink_alias(tool_obj, tool_obj_hardlink);
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
#ifdef _WIN32
    if (g_gcc_directory_alias_failed) {
        fprintf(stderr,
                "خطأ: تعذر تقديم جذر GCC ذي المسار Unicode عبر alias بلا نسخ.\n");
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
#endif
    BaaCompilerExitCode rc = BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;

    // مساحة إضافية للأعلام الاختيارية (debug/pie/startup/runtime/-lm) + -o + output + NULL
    int argv_cap = obj_count + 15;
#ifdef _WIN32
    argv_cap += g_gcc_alias_argument_count;
#endif
    const char **argv_link = (const char **)calloc((size_t)argv_cap, sizeof(char *));
    if (!argv_link)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

#ifdef _WIN32
    char **tool_objects = (char **)calloc((size_t)obj_count, sizeof(char *));
    bool *tool_object_hardlinks =
        (bool *)calloc((size_t)obj_count, sizeof(bool));
    if (!tool_objects || !tool_object_hardlinks)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        free(tool_objects);
        free(tool_object_hardlinks);
        free(argv_link);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }
    char tool_output[MAX_PATH] = "";
    bool tool_output_ready = false;
    bool tool_output_hardlink = false;
    char tool_runtime[MAX_PATH] = "";
    bool tool_runtime_hardlink = false;
    char entry_response_path[MAX_PATH] = "";
    char tool_entry_response[MAX_PATH] = "";
    bool tool_entry_response_hardlink = false;
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
                                        MAX_PATH,
                                        &tool_object_hardlinks[i]))
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
                                    sizeof(tool_output),
                                    &tool_output_hardlink))
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
                                    sizeof(tool_runtime),
                                    &tool_runtime_hardlink)) {
        fprintf(stderr,
                "خطأ: تعذر تمرير مكتبة وقت التشغيل الحقيقية إلى الرابط بلا نسخ.\n");
        goto cleanup;
    }
#endif

    int lk = 0;
    argv_link[lk++] = driver_toolchain_get_gcc_command();
#ifdef _WIN32
    for (int i = 0; i < g_gcc_alias_argument_count; i++)
        argv_link[lk++] = g_gcc_alias_arguments[i];
#endif
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
                                    sizeof(tool_entry_response),
                                    &tool_entry_response_hardlink))
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
    // يضمن سحب داعم إعادة التمركز من libmingw32 عند حذف ملفات
    // البدء الاعتيادية؛ نطلبه فقط للحزمة المقبولة التي تصرح به لأن
    // بعض حزم UCRT الخارجية لا تحتاجه مع مدخل باء المخصص.
    if (g_gcc_retain_pei386_runtime_relocator)
        argv_link[lk++] = "-Wl,-u,_pei386_runtime_relocator";
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
    for (int i = 0; i < obj_count; i++)
        win_cleanup_hardlink_alias(tool_objects[i], tool_object_hardlinks[i]);
    win_cleanup_hardlink_alias(tool_runtime, tool_runtime_hardlink);
    win_cleanup_hardlink_alias(tool_entry_response,
                               tool_entry_response_hardlink);
    win_cleanup_hardlink_alias(tool_output, tool_output_hardlink);
    win_free_tool_paths(tool_objects, obj_count);
    free(tool_objects);
    free(tool_object_hardlinks);
    if (entry_response_ready)
        (void)driver_toolchain_delete_file_utf8(entry_response_path);
    if (tool_output_ready && rc != BAA_COMPILER_EXIT_SUCCESS)
        (void)driver_toolchain_delete_file_utf8(config->output_file);
#endif
    free(argv_link);
    return rc;
}
