/**
 * @file read_file.c
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 * @version 0.3.4
 */

#include "support_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void read_file_die(FILE* f, char* buffer, const char* msg)
{
    if (f) fclose(f);
    free(buffer);
    printf("%s\n", msg);
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
            printf("Error: Could not open input file '%s'\n", path);
            break;
        case READ_FILE_ERR_SEEK:
            printf("Error: Could not seek input file '%s'\n", path);
            break;
        case READ_FILE_ERR_SIZE:
            printf("Error: Could not read size of input file '%s'\n", path);
            break;
        case READ_FILE_ERR_READ:
            printf("Error: Could not read input file '%s'\n", path);
            break;
        default:
            printf("Error: Could not read input file '%s'\n", path);
            break;
    }
    exit(1);
}

char *read_file(const char *path)
{
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
        read_file_die(f, NULL, "Error: Memory allocation failed");
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

static char* support_raw_strdup(const char* text)
{
    if (!text) return NULL;
    size_t len = strlen(text);
    char* out = (char*)malloc(len + 1u);
    if (!out) return NULL;
    if (len) memcpy(out, text, len);
    out[len] = '\0';
    return out;
}

static void support_normalize_path_text(char* path)
{
    if (!path) return;
    for (size_t i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '\\') path[i] = '/';
    }
#ifdef _WIN32
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        path[0] = (char)tolower((unsigned char)path[0]);
    }
#endif
}

/**
 * @brief نسخ نص خام منتهي بصفر لاستخدام وحدات باء المرتبطة بالمترجم.
 */
char* دعمالخامنسخ(const char* text)
{
    return support_raw_strdup(text ? text : "");
}

/**
 * @brief نسخ مدى خام وإضافة صفر نهائي.
 */
char* دعمالخامنسخبطول(const char* start, int64_t length)
{
    if (!start || length < 0) return NULL;
    char* out = (char*)malloc((size_t)length + 1u);
    if (!out) return NULL;
    if (length > 0) memcpy(out, start, (size_t)length);
    out[length] = '\0';
    return out;
}

/**
 * @brief قراءة ملف خام إن كان موجوداً، وإرجاع NULL عند فشل الفتح فقط.
 */
char* دعمالملفجربقراءة(const char* path)
{
    if (!path || !path[0]) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long length = ftell(f);
    if (length < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char* buffer = (char*)malloc((size_t)length + 1u);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t got = fread(buffer, 1u, (size_t)length, f);
    if (got != (size_t)length && ferror(f)) {
        free(buffer);
        fclose(f);
        return NULL;
    }
    buffer[got] = '\0';
    fclose(f);
    return buffer;
}

/**
 * @brief تطبيع مسار موجود إلى نص مملوك، مع fallback إلى النص الأصلي.
 */
char* دعمالمسارطبع(const char* path)
{
    if (!path || !path[0]) return support_raw_strdup("");

    char resolved[PATH_MAX];
    char* out = NULL;
#ifdef _WIN32
    if (_fullpath(resolved, path, sizeof(resolved)) != NULL) {
        out = support_raw_strdup(resolved);
    }
#else
    if (realpath(path, resolved) != NULL) {
        out = support_raw_strdup(resolved);
    }
#endif
    if (!out) out = support_raw_strdup(path);
    support_normalize_path_text(out);
    return out;
}

/**
 * @brief نسخ قيمة متغير بيئة كنص خام مملوك.
 */
char* دعمالبيئة(const char* name)
{
    if (!name || !name[0]) return NULL;
    const char* value = getenv(name);
    return value ? support_raw_strdup(value) : NULL;
}

/**
 * @brief مقارنة مسارين بعد التطبيع.
 */
int64_t دعمالمسارمتساوي(const char* lhs, const char* rhs)
{
    char* a = دعمالمسارطبع(lhs);
    char* b = دعمالمسارطبع(rhs);
    if (!a || !b) {
        free(a);
        free(b);
        return 0;
    }
#ifdef _WIN32
    int same = (_stricmp(a, b) == 0);
#else
    int same = (strcmp(a, b) == 0);
#endif
    free(a);
    free(b);
    return same ? 1 : 0;
}

/**
 * @brief تسجيل مصدر خام في نظام التشخيص المركزي.
 */
void دعمالمصدرسجل(const char* filename, const char* source)
{
    error_register_source(filename, source);
}

/**
 * @brief إصدار تشخيص موقعي ثم إنهاء العملية مثل أخطاء lexer القديمة.
 */
void دعمالخطأ(const char* filename, int64_t line, int64_t col, const char* message)
{
    error_report_loc(filename ? filename : "unknown",
                     (int)(line > 0 ? line : 1),
                     (int)(col > 0 ? col : 1),
                     "%s",
                     message ? message : "خطأ لفظي.");
    exit(1);
}
