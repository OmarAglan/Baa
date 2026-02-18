/**
 * @file read_file.c
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 * @version 0.3.4
 */

#include "baa.h"

#include <stdio.h>
#include <stdlib.h>

char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Error: Could not open input file '%s'\n", path);
        exit(1);
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        printf("Error: Could not seek input file '%s'\n", path);
        exit(1);
    }

    long length = ftell(f);
    if (length < 0)
    {
        printf("Error: Could not read size of input file '%s'\n", path);
        exit(1);
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        printf("Error: Could not seek input file '%s'\n", path);
        exit(1);
    }

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer)
    {
        printf("Error: Memory allocation failed\n");
        exit(1);
    }

    size_t got = fread(buffer, 1, (size_t)length, f);
    if (got != (size_t)length)
    {
        if (ferror(f))
        {
            printf("Error: Could not read input file '%s'\n", path);
            exit(1);
        }
        length = (long)got;
    }

    buffer[length] = '\0';
    fclose(f);
    return buffer;
}
