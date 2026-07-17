/**
 * @file file_io.c
 * @brief تنفيذ حد فتح الملفات UTF-8 على ويندوز والأنظمة الشبيهة بـ POSIX.
 */

#include "file_io.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>

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
    wchar_t* path_w = file_io_utf8_to_wide(path);
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
    wchar_t* path_w = file_io_utf8_to_wide(path);
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
    wchar_t* path_w = file_io_utf8_to_wide(path);
    if (!path_w) return NULL;

    wchar_t resolved_w[32768];
    wchar_t* resolved = _wfullpath(
        resolved_w,
        path_w,
        sizeof(resolved_w) / sizeof(resolved_w[0]));
    free(path_w);
    return resolved ? file_io_wide_to_utf8(resolved) : NULL;
#else
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) return NULL;
    size_t size = strlen(resolved) + 1u;
    char* copy = (char*)malloc(size);
    if (copy) memcpy(copy, resolved, size);
    return copy;
#endif
}
