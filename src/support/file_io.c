/**
 * @file file_io.c
 * @brief تنفيذ حد فتح الملفات UTF-8 على ويندوز والأنظمة الشبيهة بـ POSIX.
 */

#include "file_io.h"

#include <stdlib.h>

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
#else
#include <sys/stat.h>
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
