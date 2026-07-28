#include "source_tokens.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool baa_source_is_valid_utf8(const char *source)
{
    if (!source) return false;
    const unsigned char *cursor = (const unsigned char *)source;
    while (*cursor)
    {
        const unsigned char first = *cursor++;
        if (first < 0x80u) continue;

        uint32_t codepoint = 0u;
        int remaining = 0;
        uint32_t minimum = 0u;
        if ((first & 0xE0u) == 0xC0u)
        {
            codepoint = first & 0x1Fu;
            remaining = 1;
            minimum = 0x80u;
        }
        else if ((first & 0xF0u) == 0xE0u)
        {
            codepoint = first & 0x0Fu;
            remaining = 2;
            minimum = 0x800u;
        }
        else if ((first & 0xF8u) == 0xF0u)
        {
            codepoint = first & 0x07u;
            remaining = 3;
            minimum = 0x10000u;
        }
        else
        {
            return false;
        }

        for (int index = 0; index < remaining; ++index)
        {
            const unsigned char next = *cursor++;
            if (!next || (next & 0xC0u) != 0x80u) return false;
            codepoint = (codepoint << 6u) | (next & 0x3Fu);
        }
        if (codepoint < minimum || codepoint > 0x10FFFFu ||
            (codepoint >= 0xD800u && codepoint <= 0xDFFFu))
            return false;
    }
    return true;
}

static bool source_is_arabic_digit(const char *cursor)
{
    return cursor && (unsigned char)cursor[0] == 0xD9u &&
           (unsigned char)cursor[1] >= 0xA0u &&
           (unsigned char)cursor[1] <= 0xA9u;
}

static size_t source_utf8_length(const char *cursor)
{
    const unsigned char first = (unsigned char)*cursor;
    if (first < 0x80u) return 1u;
    if ((first & 0xE0u) == 0xC0u) return 2u;
    if ((first & 0xF0u) == 0xE0u) return 3u;
    return 4u;
}

static bool source_is_ascii_space(char value)
{
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

static size_t source_symbol_length(const char *cursor)
{
    if (!cursor || !cursor[0]) return 0u;
    if (strncmp(cursor, "...", 3u) == 0) return 3u;
    static const char *double_symbols[] = {
        "++", "--", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>"
    };
    for (size_t index = 0u;
         index < sizeof(double_symbols) / sizeof(double_symbols[0]);
         ++index)
    {
        if (strncmp(cursor, double_symbols[index], 2u) == 0) return 2u;
    }
    if ((unsigned char)cursor[0] == 0xD8u &&
        (unsigned char)cursor[1] == 0x8Cu)
        return 2u;
    if ((unsigned char)cursor[0] == 0xD8u &&
        (unsigned char)cursor[1] == 0x9Bu)
        return 2u;
    return strchr("=.,:+-*/%!&|^~<>(){}[]", cursor[0]) ? 1u : 0u;
}

static bool source_tokens_push(BaaSourceTokens *tokens,
                               BaaSourceToken token)
{
    if (!tokens) return false;
    if (tokens->count == tokens->capacity)
    {
        size_t capacity = tokens->capacity ? tokens->capacity * 2u : 128u;
        if (capacity < tokens->count ||
            capacity > SIZE_MAX / sizeof(BaaSourceToken))
            return false;
        BaaSourceToken *grown = (BaaSourceToken *)realloc(
            tokens->items, capacity * sizeof(BaaSourceToken));
        if (!grown) return false;
        tokens->items = grown;
        tokens->capacity = capacity;
    }
    tokens->items[tokens->count++] = token;
    return true;
}

static void source_advance_location(const char *text,
                                    size_t length,
                                    int *line,
                                    int *column)
{
    size_t index = 0u;
    while (index < length)
    {
        if (text[index] == '\r')
        {
            if (index + 1u < length && text[index + 1u] == '\n') index++;
            (*line)++;
            *column = 1;
            index++;
            continue;
        }
        if (text[index] == '\n')
        {
            (*line)++;
            *column = 1;
            index++;
            continue;
        }
        const size_t width = source_utf8_length(text + index);
        *column += (int)width;
        index += width;
    }
}

BaaSourceTokensStatus baa_source_tokens_scan(const char *source,
                                             BaaSourceTokens *tokens)
{
    if (!source || !tokens) return BAA_SOURCE_TOKENS_OUT_OF_MEMORY;
    memset(tokens, 0, sizeof(*tokens));
    if (!baa_source_is_valid_utf8(source))
        return BAA_SOURCE_TOKENS_INVALID_UTF8;

    const char *cursor = source;
    int line = 1;
    int column = 1;
    if ((unsigned char)cursor[0] == 0xEFu &&
        (unsigned char)cursor[1] == 0xBBu &&
        (unsigned char)cursor[2] == 0xBFu)
    {
        cursor += 3;
        column += 3;
    }

    bool source_line_start = true;
    while (*cursor)
    {
        const char *whitespace_start = cursor;
        while (*cursor && source_is_ascii_space(*cursor))
        {
            if (*cursor == '\r')
            {
                if (cursor[1] == '\n') cursor++;
                source_line_start = true;
            }
            else if (*cursor == '\n')
            {
                source_line_start = true;
            }
            cursor++;
        }
        source_advance_location(
            whitespace_start,
            (size_t)(cursor - whitespace_start),
            &line,
            &column);
        if (!*cursor) break;

        const char *start = cursor;
        const int start_line = line;
        const int start_column = column;
        BaaSourceTokenKind kind = BAA_SOURCE_TOKEN_WORD;
        if (source_line_start && *cursor == '#')
        {
            kind = BAA_SOURCE_TOKEN_DIRECTIVE;
            while (*cursor && *cursor != '\r' && *cursor != '\n') cursor++;
        }
        else if (cursor[0] == '/' && cursor[1] == '/')
        {
            kind = BAA_SOURCE_TOKEN_COMMENT;
            cursor += 2;
            while (*cursor && *cursor != '\r' && *cursor != '\n') cursor++;
        }
        else if (cursor[0] == '/' && cursor[1] == '*')
        {
            kind = BAA_SOURCE_TOKEN_COMMENT;
            cursor += 2;
            while (*cursor)
            {
                if (cursor[0] == '*' && cursor[1] == '/')
                {
                    cursor += 2;
                    break;
                }
                cursor += source_utf8_length(cursor);
            }
        }
        else if (*cursor == '"' || *cursor == '\'')
        {
            kind = BAA_SOURCE_TOKEN_LITERAL;
            const char quote = *cursor++;
            bool escaped = false;
            while (*cursor)
            {
                const char value = *cursor++;
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (value == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (value == quote) break;
            }
        }
        else
        {
            size_t symbol_length = source_symbol_length(cursor);
            if (symbol_length)
            {
                kind = BAA_SOURCE_TOKEN_SYMBOL;
                cursor += symbol_length;
            }
            else
            {
                const bool numeric =
                    (*cursor >= '0' && *cursor <= '9') ||
                    source_is_arabic_digit(cursor);
                bool decimal_seen = false;
                while (*cursor && !source_is_ascii_space(*cursor))
                {
                    symbol_length = source_symbol_length(cursor);
                    if (symbol_length)
                    {
                        if (numeric && !decimal_seen && *cursor == '.' &&
                            ((cursor[1] >= '0' && cursor[1] <= '9') ||
                             source_is_arabic_digit(cursor + 1)))
                        {
                            decimal_seen = true;
                            cursor++;
                            continue;
                        }
                        break;
                    }
                    cursor += source_utf8_length(cursor);
                }
            }
        }

        const size_t token_length = (size_t)(cursor - start);
        const bool comment_preserves_line_start =
            kind == BAA_SOURCE_TOKEN_COMMENT &&
            (source_line_start ||
             memchr(start, '\r', token_length) != NULL ||
             memchr(start, '\n', token_length) != NULL);
        source_advance_location(start, token_length, &line, &column);
        const BaaSourceToken token = {
            kind,
            start,
            token_length,
            (size_t)(start - source),
            (size_t)(cursor - source),
            start_line,
            start_column,
            line,
            column,
        };
        if (!source_tokens_push(tokens, token))
        {
            baa_source_tokens_free(tokens);
            return BAA_SOURCE_TOKENS_OUT_OF_MEMORY;
        }
        source_line_start = comment_preserves_line_start;
    }
    return BAA_SOURCE_TOKENS_OK;
}

void baa_source_tokens_free(BaaSourceTokens *tokens)
{
    if (!tokens) return;
    free(tokens->items);
    memset(tokens, 0, sizeof(*tokens));
}
