/**
 * @file read_file.c
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 * @version 0.3.4
 */

#include "support_internal.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
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
    FILE *f = baa_fopen_utf8(path, "rb");
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

char *read_stdin_source(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
    {
        return NULL;
    }
#endif

    size_t capacity = 4096u;
    size_t length = 0u;
    char *buffer = (char *)malloc(capacity + 1u);
    if (!buffer) return NULL;

    for (;;)
    {
        if (length == capacity)
        {
            if (capacity > (SIZE_MAX / 2u) - 1u)
            {
                free(buffer);
                return NULL;
            }
            size_t new_capacity = capacity * 2u;
            char *grown = (char *)realloc(buffer, new_capacity + 1u);
            if (!grown)
            {
                free(buffer);
                return NULL;
            }
            buffer = grown;
            capacity = new_capacity;
        }

        size_t got = fread(buffer + length, 1u, capacity - length, stdin);
        length += got;
        if (got == 0u)
        {
            if (ferror(stdin))
            {
                free(buffer);
                return NULL;
            }
            break;
        }
    }

    buffer[length] = '\0';
    return buffer;
}
