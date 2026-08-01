#ifndef BAA_DRIVER_STRUCTURE_H
#define BAA_DRIVER_STRUCTURE_H

#include <stdio.h>

typedef enum
{
    BAA_STRUCTURE_OK = 0,
    BAA_STRUCTURE_INVALID_UTF8 = 1,
    BAA_STRUCTURE_OUT_OF_MEMORY = 2,
} BaaStructureStatus;

BaaStructureStatus driver_structure_json_write(FILE *out,
                                                const char *compiler_version,
                                                const char *logical_file,
                                                const char *source);

#endif
