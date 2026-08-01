#include "driver_structure.h"

#include "../frontend/source_tokens.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char *kind;
    size_t start_byte;
    size_t end_byte;
    int start_line;
    int start_column;
    int end_line;
    int end_column;
} StructureRange;

typedef struct
{
    StructureRange *items;
    size_t count;
    size_t capacity;
} StructureRanges;

typedef struct
{
    const BaaSourceToken *token;
    char delimiter;
} DelimiterEntry;

static void structure_json_escape(FILE *out, const char *text)
{
    fputc('"', out);
    for (const unsigned char *cursor =
             (const unsigned char *)(text ? text : "");
         *cursor; ++cursor)
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
                    fputc(*cursor, out);
                break;
        }
    }
    fputc('"', out);
}

static bool structure_ranges_push(StructureRanges *ranges,
                                  StructureRange range)
{
    if (!ranges || range.end_byte <= range.start_byte) return true;
    if (ranges->count == ranges->capacity)
    {
        size_t capacity = ranges->capacity ? ranges->capacity * 2u : 128u;
        if (capacity < ranges->count ||
            capacity > SIZE_MAX / sizeof(StructureRange))
            return false;
        StructureRange *grown = (StructureRange *)realloc(
            ranges->items, capacity * sizeof(StructureRange));
        if (!grown) return false;
        ranges->items = grown;
        ranges->capacity = capacity;
    }
    ranges->items[ranges->count++] = range;
    return true;
}

static size_t structure_utf8_width(const char *cursor)
{
    const unsigned char first = (unsigned char)*cursor;
    if (first < 0x80u) return 1u;
    if ((first & 0xE0u) == 0xC0u) return 2u;
    if ((first & 0xF0u) == 0xE0u) return 3u;
    return 4u;
}

static void structure_end_location(const char *source,
                                   size_t end_byte,
                                   int *line,
                                   int *column)
{
    int current_line = 1;
    int current_column = 1;
    size_t byte = 0u;
    while (byte < end_byte)
    {
        if (source[byte] == '\r')
        {
            if (byte + 1u < end_byte && source[byte + 1u] == '\n') ++byte;
            ++current_line;
            current_column = 1;
            ++byte;
            continue;
        }
        if (source[byte] == '\n')
        {
            ++current_line;
            current_column = 1;
            ++byte;
            continue;
        }
        const size_t width = structure_utf8_width(source + byte);
        current_column += (int)width;
        byte += width;
    }
    if (line) *line = current_line;
    if (column) *column = current_column;
}

static bool structure_is_horizontal_space(char value)
{
    return value == ' ' || value == '\t' || value == '\f' || value == '\v';
}

static size_t structure_raw_line_start(const char *source, size_t byte)
{
    while (byte > 0u && source[byte - 1u] != '\n' &&
           source[byte - 1u] != '\r')
        --byte;
    return byte;
}

static size_t structure_line_start(const char *source, size_t byte)
{
    byte = structure_raw_line_start(source, byte);
    while (source[byte] && structure_is_horizontal_space(source[byte])) ++byte;
    return byte;
}

static bool structure_add_lines(const char *source,
                                StructureRanges *selections)
{
    const size_t source_size = strlen(source);
    size_t line_start = 0u;
    int line = 1;
    while (line_start < source_size)
    {
        size_t line_end = line_start;
        while (line_end < source_size && source[line_end] != '\r' &&
               source[line_end] != '\n')
            ++line_end;
        size_t content_start = line_start;
        while (content_start < line_end &&
               structure_is_horizontal_space(source[content_start]))
            ++content_start;
        size_t content_end = line_end;
        while (content_end > content_start &&
               structure_is_horizontal_space(source[content_end - 1u]))
            --content_end;
        if (content_end > content_start &&
            !structure_ranges_push(selections, (StructureRange){
                "line",
                content_start,
                content_end,
                line,
                1 + (int)(content_start - line_start),
                line,
                1 + (int)(content_end - line_start),
            }))
            return false;

        if (line_end == source_size) break;
        if (source[line_end] == '\r' && line_end + 1u < source_size &&
            source[line_end + 1u] == '\n')
            line_start = line_end + 2u;
        else
            line_start = line_end + 1u;
        ++line;
    }
    return true;
}

static char structure_open_delimiter(const BaaSourceToken *token)
{
    if (!token || token->kind != BAA_SOURCE_TOKEN_SYMBOL || token->length != 1u)
        return '\0';
    const char value = token->start[0];
    return value == '(' || value == '[' || value == '{' ? value : '\0';
}

static char structure_expected_open(const BaaSourceToken *token)
{
    if (!token || token->kind != BAA_SOURCE_TOKEN_SYMBOL || token->length != 1u)
        return '\0';
    if (token->start[0] == ')') return '(';
    if (token->start[0] == ']') return '[';
    if (token->start[0] == '}') return '{';
    return '\0';
}

static bool structure_literal_closed(const BaaSourceToken *token)
{
    if (!token || token->kind != BAA_SOURCE_TOKEN_LITERAL ||
        token->length < 2u)
        return false;
    const char quote = token->start[0];
    bool escaped = false;
    for (size_t index = 1u; index < token->length; ++index)
    {
        const char value = token->start[index];
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
        if (value == quote) return index + 1u == token->length;
    }
    return false;
}

static int structure_range_compare(const void *left_ptr, const void *right_ptr)
{
    const StructureRange *left = (const StructureRange *)left_ptr;
    const StructureRange *right = (const StructureRange *)right_ptr;
    if (left->start_byte < right->start_byte) return -1;
    if (left->start_byte > right->start_byte) return 1;
    if (left->end_byte > right->end_byte) return -1;
    if (left->end_byte < right->end_byte) return 1;
    return strcmp(left->kind, right->kind);
}

static void structure_print_span(FILE *out, const StructureRange *range)
{
    fputs("{\"start\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d,\"byte\":%zu",
            range->start_line, range->start_column, range->start_byte);
    fputs("},\"end\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d,\"byte\":%zu",
            range->end_line, range->end_column, range->end_byte);
    fputs("}}", out);
}

static void structure_print_ranges(FILE *out, StructureRanges *ranges)
{
    if (ranges->count > 1u)
        qsort(ranges->items, ranges->count, sizeof(StructureRange),
              structure_range_compare);
    fputc('[', out);
    bool first = true;
    const StructureRange *previous = NULL;
    for (size_t index = 0u; index < ranges->count; ++index)
    {
        const StructureRange *range = &ranges->items[index];
        if (previous && previous->start_byte == range->start_byte &&
            previous->end_byte == range->end_byte &&
            strcmp(previous->kind, range->kind) == 0)
            continue;
        if (!first) fputc(',', out);
        fputs("{\"kind\":", out);
        structure_json_escape(out, range->kind);
        fputs(",\"span\":", out);
        structure_print_span(out, range);
        fputc('}', out);
        first = false;
        previous = range;
    }
    fputc(']', out);
}

static bool structure_add_comment_run(StructureRanges *folds,
                                      const BaaSourceToken *start,
                                      const BaaSourceToken *end)
{
    if (!start || !end || start == end) return true;
    return structure_ranges_push(folds, (StructureRange){
        "comment",
        start->byte_start,
        end->byte_end,
        start->line,
        start->column,
        end->end_line,
        end->end_column,
    });
}

BaaStructureStatus driver_structure_json_write(FILE *out,
                                                const char *compiler_version,
                                                const char *logical_file,
                                                const char *source)
{
    if (!out || !source) return BAA_STRUCTURE_OUT_OF_MEMORY;

    BaaSourceTokens tokens;
    const BaaSourceTokensStatus token_status =
        baa_source_tokens_scan(source, &tokens);
    if (token_status == BAA_SOURCE_TOKENS_INVALID_UTF8)
        return BAA_STRUCTURE_INVALID_UTF8;
    if (token_status != BAA_SOURCE_TOKENS_OK)
        return BAA_STRUCTURE_OUT_OF_MEMORY;

    StructureRanges folds = {0};
    StructureRanges selections = {0};
    DelimiterEntry *stack = tokens.count
        ? (DelimiterEntry *)calloc(tokens.count, sizeof(DelimiterEntry)) : NULL;
    if (tokens.count && !stack)
    {
        baa_source_tokens_free(&tokens);
        return BAA_STRUCTURE_OUT_OF_MEMORY;
    }

    bool complete = true;
    bool ok = structure_add_lines(source, &selections);
    size_t stack_count = 0u;
    const BaaSourceToken *comment_run_start = NULL;
    const BaaSourceToken *comment_run_end = NULL;

    for (size_t index = 0u; ok && index < tokens.count; ++index)
    {
        const BaaSourceToken *token = &tokens.items[index];
        ok = structure_ranges_push(&selections, (StructureRange){
            "token",
            token->byte_start,
            token->byte_end,
            token->line,
            token->column,
            token->end_line,
            token->end_column,
        });
        if (!ok) break;

        if (token->kind == BAA_SOURCE_TOKEN_COMMENT &&
            token->length >= 2u && token->start[0] == '/' &&
            token->start[1] == '/')
        {
            if (!comment_run_start || !comment_run_end ||
                token->line != comment_run_end->line + 1)
            {
                ok = structure_add_comment_run(
                    &folds, comment_run_start, comment_run_end);
                comment_run_start = token;
            }
            comment_run_end = token;
        }
        else
        {
            ok = structure_add_comment_run(
                &folds, comment_run_start, comment_run_end);
            comment_run_start = NULL;
            comment_run_end = NULL;
        }
        if (!ok) break;

        if (token->kind == BAA_SOURCE_TOKEN_COMMENT && token->length >= 2u &&
            token->start[0] == '/' && token->start[1] == '*')
        {
            const bool closed = token->length >= 4u &&
                token->start[token->length - 2u] == '*' &&
                token->start[token->length - 1u] == '/';
            if (!closed) complete = false;
            if (token->line < token->end_line)
                ok = structure_ranges_push(&folds, (StructureRange){
                    "comment",
                    token->byte_start,
                    token->byte_end,
                    token->line,
                    token->column,
                    token->end_line,
                    token->end_column,
                });
        }
        if (!ok) break;

        if (token->kind == BAA_SOURCE_TOKEN_LITERAL && token->length > 0u)
        {
            if (!structure_literal_closed(token))
                complete = false;
        }

        const char open = structure_open_delimiter(token);
        if (open)
        {
            stack[stack_count++] = (DelimiterEntry){token, open};
            continue;
        }
        const char expected = structure_expected_open(token);
        if (!expected) continue;

        size_t match = stack_count;
        while (match > 0u && stack[match - 1u].delimiter != expected) --match;
        if (match == 0u)
        {
            complete = false;
            continue;
        }
        if (match != stack_count) complete = false;
        const BaaSourceToken *opening = stack[match - 1u].token;
        stack_count = match - 1u;

        if (token->byte_start > opening->byte_end)
            ok = structure_ranges_push(&selections, (StructureRange){
                "content",
                opening->byte_end,
                token->byte_start,
                opening->end_line,
                opening->end_column,
                token->line,
                token->column,
            });
        if (ok)
            ok = structure_ranges_push(&selections, (StructureRange){
                "group",
                opening->byte_start,
                token->byte_end,
                opening->line,
                opening->column,
                token->end_line,
                token->end_column,
            });
        if (ok && expected == '{')
        {
            const size_t construct_start =
                structure_line_start(source, opening->byte_start);
            ok = structure_ranges_push(&selections, (StructureRange){
                "construct",
                construct_start,
                token->byte_end,
                opening->line,
                1 + (int)(construct_start -
                    structure_raw_line_start(source, construct_start)),
                token->end_line,
                token->end_column,
            });
        }
        if (ok && opening->line < token->line)
            ok = structure_ranges_push(&folds, (StructureRange){
                "region",
                opening->byte_start,
                token->byte_end,
                opening->line,
                opening->column,
                token->end_line,
                token->end_column,
            });
    }

    if (ok)
        ok = structure_add_comment_run(
            &folds, comment_run_start, comment_run_end);
    if (stack_count != 0u) complete = false;

    const size_t source_size = strlen(source);
    if (ok && source_size > 0u)
    {
        int end_line = 1;
        int end_column = 1;
        structure_end_location(source, source_size, &end_line, &end_column);
        ok = structure_ranges_push(&selections, (StructureRange){
            "document", 0u, source_size, 1, 1, end_line, end_column,
        });
    }

    if (ok)
    {
        fputs("{\"schema_version\":\"structure-json-v1\",\"compiler_version\":", out);
        structure_json_escape(out, compiler_version ? compiler_version : "");
        fputs(",\"language\":\"baa\",\"file\":", out);
        structure_json_escape(out, logical_file ? logical_file : "");
        fprintf(out,
                ",\"position_encoding\":\"utf-8-bytes\",\"source_bytes\":%zu,\"complete\":%s,\"folding_ranges\":",
                source_size, complete ? "true" : "false");
        structure_print_ranges(out, &folds);
        fputs(",\"selection_ranges\":", out);
        structure_print_ranges(out, &selections);
        fputs("}\n", out);
        ok = ferror(out) == 0;
    }

    free(stack);
    free(folds.items);
    free(selections.items);
    baa_source_tokens_free(&tokens);
    return ok ? BAA_STRUCTURE_OK : BAA_STRUCTURE_OUT_OF_MEMORY;
}
