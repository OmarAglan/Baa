/**
 * @file read_file.c
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 * @version 0.3.4
 */

#include "support_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* read_file_safe_path(const char* path)
{
    return (path && path[0] != '\0') ? path : "<unknown>";
}

static void read_file_die(FILE* f, char* buffer, const char* msg)
{
    if (f) fclose(f);
    free(buffer);
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

typedef enum
{
    READ_FILE_ERR_OPEN,
    READ_FILE_ERR_SEEK,
    READ_FILE_ERR_SIZE,
    READ_FILE_ERR_READ,
} ReadFilePathError;

static void read_file_die_path(FILE* f, char* buffer, const char* path,
                               ReadFilePathError err, int sys_errno)
{
    const char* safe_path = read_file_safe_path(path);
    const char* reason = (sys_errno != 0) ? strerror(sys_errno) : "سبب نظام غير متاح";

    if (f) fclose(f);
    free(buffer);

    switch (err)
    {
        case READ_FILE_ERR_OPEN:
            fprintf(stderr, "خطأ: تعذّر فتح ملف الإدخال '%s' (%s)\n", safe_path, reason);
            break;
        case READ_FILE_ERR_SEEK:
            fprintf(stderr, "خطأ: تعذّر تحريك مؤشر ملف الإدخال '%s' (%s)\n", safe_path, reason);
            break;
        case READ_FILE_ERR_SIZE:
            fprintf(stderr, "خطأ: تعذّر قراءة حجم ملف الإدخال '%s' (%s)\n", safe_path, reason);
            break;
        case READ_FILE_ERR_READ:
            fprintf(stderr, "خطأ: تعذّرت قراءة ملف الإدخال '%s' (%s)\n", safe_path, reason);
            break;
        default:
            fprintf(stderr, "خطأ: تعذّرت قراءة ملف الإدخال '%s' (%s)\n", safe_path, reason);
            break;
    }

    exit(EXIT_FAILURE);
}

char* read_file(const char* path)
{
    if (!path || path[0] == '\0')
    {
        read_file_die(NULL, NULL, "خطأ: مسار ملف الإدخال غير صالح");
    }

    errno = 0;
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        read_file_die_path(NULL, NULL, path, READ_FILE_ERR_OPEN, errno);
    }

    errno = 0;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        read_file_die_path(f, NULL, path, READ_FILE_ERR_SEEK, errno);
    }

    errno = 0;
    long length = ftell(f);
    if (length < 0)
    {
        read_file_die_path(f, NULL, path, READ_FILE_ERR_SIZE, errno);
    }

    errno = 0;
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        read_file_die_path(f, NULL, path, READ_FILE_ERR_SEEK, errno);
    }

    char* buffer = (char*)malloc((size_t)length + 1u);
    if (!buffer)
    {
        read_file_die(f, NULL, "خطأ: فشل حجز الذاكرة لقراءة الملف");
    }

    errno = 0;
    size_t got = fread(buffer, 1, (size_t)length, f);
    if (got != (size_t)length)
    {
        if (ferror(f))
        {
            read_file_die_path(f, buffer, path, READ_FILE_ERR_READ, errno);
        }
        length = (long)got;
    }

    buffer[length] = '\0';
    fclose(f);
    return buffer;
}
