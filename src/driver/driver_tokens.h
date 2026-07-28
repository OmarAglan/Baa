#ifndef BAA_DRIVER_TOKENS_H
#define BAA_DRIVER_TOKENS_H

#include <stdio.h>

typedef enum
{
    BAA_TOKENS_OK = 0,
    BAA_TOKENS_INVALID_UTF8 = 1,
    BAA_TOKENS_OUT_OF_MEMORY = 2,
} BaaTokensStatus;

BaaTokensStatus driver_tokens_json_write(FILE *out,
                                         const char *compiler_version,
                                         const char *logical_file,
                                         const char *source);

#endif
