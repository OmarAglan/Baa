/**
 * @file file_io.c
 * @brief تنفيذ حد فتح الملفات UTF-8 على ويندوز والأنظمة الشبيهة بـ POSIX.
 */

#include "file_io.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <wchar.h>

static wchar_t* file_io_utf8_to_wide(const char* value)
{
    if (!value) return NULL;

    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    if (needed <= 0) return NULL;

    wchar_t* wide = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
    if (!wide) return NULL;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wide, needed) <= 0)
    {
        free(wide);
        return NULL;
    }
    return wide;
}

static char* file_io_wide_to_utf8(const wchar_t* value)
{
    if (!value) return NULL;

    int needed = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        NULL,
        0,
        NULL,
        NULL);
    if (needed <= 0) return NULL;

    char* utf8 = (char*)malloc((size_t)needed);
    if (!utf8) return NULL;
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            -1,
            utf8,
            needed,
            NULL,
            NULL) <= 0)
    {
        free(utf8);
        return NULL;
    }
    return utf8;
}

static wchar_t* file_io_absolute_wide_utf8(const char* path)
{
    wchar_t* path_w = file_io_utf8_to_wide(path);
    if (!path_w) return NULL;

    if (wcsncmp(path_w, L"\\\\?\\", 4) == 0 ||
        wcsncmp(path_w, L"\\\\.\\", 4) == 0)
        return path_w;

    DWORD needed = GetFullPathNameW(path_w, 0, NULL, NULL);
    if (needed == 0)
    {
        free(path_w);
        return NULL;
    }

    wchar_t* absolute =
        (wchar_t*)calloc((size_t)needed + 1u, sizeof(wchar_t));
    if (!absolute)
    {
        free(path_w);
        return NULL;
    }

    DWORD written = GetFullPathNameW(path_w, needed, absolute, NULL);
    free(path_w);
    if (written == 0 || written >= needed)
    {
        free(absolute);
        return NULL;
    }
    for (wchar_t* cursor = absolute; *cursor; ++cursor)
    {
        if (*cursor == L'/') *cursor = L'\\';
    }
    return absolute;
}

wchar_t* baa_windows_extended_path_utf8(const char* path)
{
    if (!path || !path[0]) return NULL;
    wchar_t* absolute = file_io_absolute_wide_utf8(path);
    if (!absolute) return NULL;
    if (wcsncmp(absolute, L"\\\\?\\", 4) == 0 ||
        wcsncmp(absolute, L"\\\\.\\", 4) == 0)
        return absolute;

    bool unc = absolute[0] == L'\\' && absolute[1] == L'\\';
    const wchar_t* prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
    const wchar_t* suffix = unc ? absolute + 2 : absolute;
    size_t prefix_len = wcslen(prefix);
    size_t suffix_len = wcslen(suffix);
    wchar_t* extended = (wchar_t*)malloc(
        (prefix_len + suffix_len + 1u) * sizeof(wchar_t));
    if (!extended)
    {
        free(absolute);
        return NULL;
    }
    memcpy(extended, prefix, prefix_len * sizeof(wchar_t));
    memcpy(extended + prefix_len,
           suffix,
           (suffix_len + 1u) * sizeof(wchar_t));
    free(absolute);
    return extended;
}
#else
#include <limits.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

FILE* baa_fopen_utf8(const char* path, const char* mode)
{
    if (!path || !mode) return NULL;

#ifdef _WIN32
    wchar_t* path_w = baa_windows_extended_path_utf8(path);
    wchar_t* mode_w = file_io_utf8_to_wide(mode);
    if (!path_w || !mode_w)
    {
        free(path_w);
        free(mode_w);
        return NULL;
    }

    FILE* file = _wfopen(path_w, mode_w);
    free(path_w);
    free(mode_w);
    return file;
#else
    return fopen(path, mode);
#endif
}

int baa_mkdir_utf8(const char* path)
{
    if (!path) return -1;

#ifdef _WIN32
    wchar_t* path_w = baa_windows_extended_path_utf8(path);
    if (!path_w) return -1;
    int result = _wmkdir(path_w);
    free(path_w);
    return result;
#else
    return mkdir(path, 0777);
#endif
}

char* baa_fullpath_utf8(const char* path)
{
    if (!path || !path[0]) return NULL;

#ifdef _WIN32
    wchar_t* resolved = file_io_absolute_wide_utf8(path);
    if (!resolved) return NULL;

    const wchar_t* canonical = resolved;
    wchar_t* unc = NULL;
    if (wcsncmp(resolved, L"\\\\?\\UNC\\", 8) == 0)
    {
        size_t suffix_len = wcslen(resolved + 8);
        unc = (wchar_t*)malloc((suffix_len + 3u) * sizeof(wchar_t));
        if (!unc)
        {
            free(resolved);
            return NULL;
        }
        unc[0] = L'\\';
        unc[1] = L'\\';
        memcpy(unc + 2,
               resolved + 8,
               (suffix_len + 1u) * sizeof(wchar_t));
        canonical = unc;
    }
    else if (wcsncmp(resolved, L"\\\\?\\", 4) == 0)
    {
        canonical = resolved + 4;
    }

    char* utf8 = file_io_wide_to_utf8(canonical);
    free(unc);
    free(resolved);
    return utf8;
#else
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) return NULL;
    size_t size = strlen(resolved) + 1u;
    char* copy = (char*)malloc(size);
    if (copy) memcpy(copy, resolved, size);
    return copy;
#endif
}
