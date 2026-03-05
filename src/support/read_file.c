/**
 * @file read_file.c
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 * @version 0.3.4
 */

#include "baa.h"

#include <stdio.h>
#include <stdlib.h>

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
