/**
 * @file read_file.c
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 * @version 0.3.4
 */

#include "support_internal.h"

#include <stdio.h>
#include <stdlib.h>

static void read_file_die(FILE* f, char* buffer, const char* msg)
{
    if (f) fclose(f);
    free(buffer);
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

typedef enum
{
    READ_FILE_ERR_OPEN,
    READ_FILE_ERR_SEEK,
    READ_FILE_ERR_SIZE,
    READ_FILE_ERR_READ,
} ReadFilePathError;

static void read_file_die_path(FILE* f, char* buffer, const char* path, ReadFilePathError err)
{
    if (f) fclose(f);
    free(buffer);
    switch (err)
    {
        case READ_FILE_ERR_OPEN:
            fprintf(stderr, "خطأ: تعذّر فتح ملف الإدخال '%s'\n", path);
            break;
        case READ_FILE_ERR_SEEK:
            fprintf(stderr, "خطأ: تعذّر تحريك مؤشر ملف الإدخال '%s'\n", path);
            break;
        case READ_FILE_ERR_SIZE:
            fprintf(stderr, "خطأ: تعذّر قراءة حجم ملف الإدخال '%s'\n", path);
            break;
        case READ_FILE_ERR_READ:
            fprintf(stderr, "خطأ: تعذّرت قراءة ملف الإدخال '%s'\n", path);
            break;
        default:
            fprintf(stderr, "خطأ: تعذّرت قراءة ملف الإدخال '%s'\n", path);
            break;
    }
    exit(1);
}

char *read_file(const char *path)
{
    if (!path || path[0] == '\0')
    {
        read_file_die(NULL, NULL, "خطأ: مسار ملف الإدخال غير صالح");
    }

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        read_file_die_path(NULL, NULL, path, READ_FILE_ERR_OPEN);
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        read_file_die_path(f, NULL, path, READ_FILE_ERR_SEEK);
    }

    long length = ftell(f);
    if (length < 0)
    {
        read_file_die_path(f, NULL, path, READ_FILE_ERR_SIZE);
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        read_file_die_path(f, NULL, path, READ_FILE_ERR_SEEK);
    }

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer)
    {
        read_file_die(f, NULL, "خطأ: فشل حجز الذاكرة لقراءة الملف");
    }

    size_t got = fread(buffer, 1, (size_t)length, f);
    if (got != (size_t)length)
    {
        if (ferror(f))
        {
            read_file_die_path(f, buffer, path, READ_FILE_ERR_READ);
        }
        length = (long)got;
    }

    buffer[length] = '\0';
    fclose(f);
    return buffer;
}
