#include "driver_tokens.h"

#include "../frontend/language_profile.h"
#include "../frontend/source_tokens.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void tokens_json_escape(FILE *out, const char *text)
{
    fputc('"', out);
    if (text)
    {
        for (const unsigned char *cursor =
                 (const unsigned char *)text;
             *cursor;
             ++cursor)
        {
            switch (*cursor)
            {
                case '"': fputs("\\\"", out); break;
                case '\\': fputs("\\\\", out); break;
                case '\b': fputs("\\b", out); break;
                case '\f': fputs("\\f", out); break;
                case '\n': fputs("\\n", out); break;
                case '\r': fputs("\\r", out); break;
                case '\t': fputs("\\t", out); break;
                default:
                    if (*cursor < 0x20u)
                        fprintf(out, "\\u%04x", (unsigned int)*cursor);
                    else
                        fputc((int)*cursor, out);
                    break;
            }
        }
    }
    fputc('"', out);
}

static bool tokens_is_arabic_digit(const char *text)
{
    return text && (unsigned char)text[0] == 0xD9u &&
           (unsigned char)text[1] >= 0xA0u &&
           (unsigned char)text[1] <= 0xA9u;
}

static bool tokens_is_type_keyword(BaaTokenType type)
{
    return type == TOKEN_KEYWORD_INT || type == TOKEN_KEYWORD_I8 ||
           type == TOKEN_KEYWORD_I16 || type == TOKEN_KEYWORD_I32 ||
           type == TOKEN_KEYWORD_I64 || type == TOKEN_KEYWORD_U8 ||
           type == TOKEN_KEYWORD_U16 || type == TOKEN_KEYWORD_U32 ||
           type == TOKEN_KEYWORD_U64 || type == TOKEN_KEYWORD_STRING ||
           type == TOKEN_KEYWORD_BOOL || type == TOKEN_KEYWORD_CHAR ||
           type == TOKEN_KEYWORD_FLOAT || type == TOKEN_KEYWORD_FLOAT32 ||
           type == TOKEN_KEYWORD_VOID;
}

static bool tokens_is_modifier(BaaTokenType type)
{
    return type == TOKEN_CONST || type == TOKEN_STATIC ||
           type == TOKEN_EXTERN;
}

static const char *tokens_word_kind(const BaaSourceToken *token,
                                    bool *out_of_memory)
{
    if (!token || !token->start || token->length == 0u)
        return "identifier";
    const unsigned char first = (unsigned char)token->start[0];
    if ((first >= (unsigned char)'0' && first <= (unsigned char)'9') ||
        tokens_is_arabic_digit(token->start))
        return "number";

    char *word = (char *)malloc(token->length + 1u);
    if (!word)
    {
        if (out_of_memory) *out_of_memory = true;
        return "identifier";
    }
    memcpy(word, token->start, token->length);
    word[token->length] = '\0';
    BaaTokenType type = TOKEN_INVALID;
    const bool keyword = baa_language_keyword_token(word, &type);
    free(word);
    if (!keyword) return "identifier";
    if (tokens_is_type_keyword(type)) return "type";
    if (tokens_is_modifier(type)) return "modifier";
    return "keyword";
}

static const char *tokens_kind(const BaaSourceToken *token,
                               bool *out_of_memory)
{
    switch (token->kind)
    {
        case BAA_SOURCE_TOKEN_WORD:
            return tokens_word_kind(token, out_of_memory);
        case BAA_SOURCE_TOKEN_LITERAL:
            return token->length > 0u && token->start[0] == '\''
                ? "character"
                : "string";
        case BAA_SOURCE_TOKEN_SYMBOL:
            return "operator";
        case BAA_SOURCE_TOKEN_COMMENT:
            return "comment";
        case BAA_SOURCE_TOKEN_DIRECTIVE:
            return "directive";
        default:
            return "identifier";
    }
}

BaaTokensStatus driver_tokens_json_write(FILE *out,
                                         const char *compiler_version,
                                         const char *logical_file,
                                         const char *source)
{
    if (!out || !source) return BAA_TOKENS_OUT_OF_MEMORY;

    BaaSourceTokens tokens = {0};
    const BaaSourceTokensStatus scan_status =
        baa_source_tokens_scan(source, &tokens);
    if (scan_status == BAA_SOURCE_TOKENS_INVALID_UTF8)
        return BAA_TOKENS_INVALID_UTF8;
    if (scan_status != BAA_SOURCE_TOKENS_OK)
        return BAA_TOKENS_OUT_OF_MEMORY;

    fputs("{\"schema_version\":\"tokens-json-v1\",\"compiler_version\":", out);
    tokens_json_escape(out, compiler_version ? compiler_version : "");
    fputs(",\"language\":\"baa\",\"file\":", out);
    tokens_json_escape(out, logical_file ? logical_file : "");
    fprintf(out,
            ",\"position_encoding\":\"utf-8-bytes\","
            "\"source_bytes\":%zu,\"tokens\":[",
            strlen(source));

    bool out_of_memory = false;
    for (size_t index = 0u; index < tokens.count; ++index)
    {
        const BaaSourceToken *token = &tokens.items[index];
        const char *kind = tokens_kind(token, &out_of_memory);
        if (out_of_memory)
        {
            baa_source_tokens_free(&tokens);
            return BAA_TOKENS_OUT_OF_MEMORY;
        }
        if (index != 0u) fputc(',', out);
        fputs("{\"kind\":", out);
        tokens_json_escape(out, kind);
        fprintf(out,
                ",\"span\":{\"start\":{\"line\":%d,\"column\":%d,"
                "\"byte\":%zu},\"end\":{\"line\":%d,\"column\":%d,"
                "\"byte\":%zu}}}",
                token->line,
                token->column,
                token->byte_start,
                token->end_line,
                token->end_column,
                token->byte_end);
    }
    fputs("]}\n", out);
    baa_source_tokens_free(&tokens);
    return ferror(out) ? BAA_TOKENS_OUT_OF_MEMORY : BAA_TOKENS_OK;
}
