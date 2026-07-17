/**
 * @file driver_artifacts.c
 * @brief تسمية آثار البناء المؤقتة بلا تعارض بين عمليات المصرّف.
 */

#include "driver_artifacts.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

char* driver_make_temp_artifact_path(const char* base,
                                     const char* prefix,
                                     const char* extension)
{
    static unsigned long artifact_counter = 0;
    if (!base || !prefix || !extension) return NULL;

    unsigned long artifact_id = ++artifact_counter;
#ifdef _WIN32
    unsigned long process_id = (unsigned long)_getpid();
#else
    unsigned long process_id = (unsigned long)getpid();
#endif

    int count = snprintf(NULL,
                         0,
                         "%s.baa_%s_%lu_%lu%s",
                         base,
                         prefix,
                         process_id,
                         artifact_id,
                         extension);
    if (count <= 0) return NULL;

    size_t size = (size_t)count + 1u;
    char* path = (char*)malloc(size);
    if (!path) return NULL;

    int written = snprintf(path,
                           size,
                           "%s.baa_%s_%lu_%lu%s",
                           base,
                           prefix,
                           process_id,
                           artifact_id,
                           extension);
    if (written != count)
    {
        free(path);
        return NULL;
    }
    return path;
}
