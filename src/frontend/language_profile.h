/**
 * @file language_profile.h
 * @brief المصدر المركزي لكلمات باء وبيانات الإكمال التي تستهلكها الأدوات.
 */

#ifndef BAA_FRONTEND_LANGUAGE_PROFILE_H
#define BAA_FRONTEND_LANGUAGE_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

#include "lexer.h"

typedef struct
{
    const char *label;
    BaaTokenType token_type;
    const char *completion_kind;
    const char *detail;
    bool lexical_keyword;
} BaaLanguageKeyword;

typedef struct
{
    const char *label;
    const char *completion_kind;
    const char *detail;
    const char *filter_text;
    const char *insert_text;
    bool snippet;
} BaaLanguageCompletionEntry;

const BaaLanguageKeyword *baa_language_keywords(size_t *count);
bool baa_language_keyword_token(const char *word, BaaTokenType *token_type);

const BaaLanguageCompletionEntry *baa_language_directives(size_t *count);
const BaaLanguageCompletionEntry *baa_language_snippets(size_t *count);

#endif
