#ifndef BAA_FRONTEND_SOURCE_TOKENS_H
#define BAA_FRONTEND_SOURCE_TOKENS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    BAA_SOURCE_TOKEN_WORD,
    BAA_SOURCE_TOKEN_LITERAL,
    BAA_SOURCE_TOKEN_SYMBOL,
    BAA_SOURCE_TOKEN_COMMENT,
    BAA_SOURCE_TOKEN_DIRECTIVE,
} BaaSourceTokenKind;

typedef struct
{
    BaaSourceTokenKind kind;
    const char *start;
    size_t length;
    size_t byte_start;
    size_t byte_end;
    int line;
    int column;
    int end_line;
    int end_column;
} BaaSourceToken;

typedef struct
{
    BaaSourceToken *items;
    size_t count;
    size_t capacity;
} BaaSourceTokens;

typedef enum
{
    BAA_SOURCE_TOKENS_OK = 0,
    BAA_SOURCE_TOKENS_INVALID_UTF8 = 1,
    BAA_SOURCE_TOKENS_OUT_OF_MEMORY = 2,
} BaaSourceTokensStatus;

bool baa_source_is_valid_utf8(const char *source);
BaaSourceTokensStatus baa_source_tokens_scan(const char *source,
                                             BaaSourceTokens *tokens);
void baa_source_tokens_free(BaaSourceTokens *tokens);

#endif
