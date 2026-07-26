/**
 * @file formatter.c
 * @brief منسق لفظي محافظ لمصدر باء.
 *
 * لا يشغل هذا المسار المعالج القبلي حتى لا يوسع #تضمين أو الماكرو، ولا يعيد
 * بناء المصدر من AST حتى تبقى التعليقات والنصوص كما كتبها المستخدم.
 */

#include "formatter.h"
#include "language_profile.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
} FormatBuffer;

typedef enum
{
    FORMAT_TOKEN_WORD,
    FORMAT_TOKEN_LITERAL,
    FORMAT_TOKEN_SYMBOL,
    FORMAT_TOKEN_COMMENT,
    FORMAT_TOKEN_DIRECTIVE,
} FormatTokenKind;

typedef struct
{
    FormatTokenKind kind;
    const char *start;
    size_t length;
    int newlines_before;
} FormatToken;

typedef struct
{
    FormatToken *items;
    size_t count;
    size_t capacity;
} FormatTokens;

typedef struct
{
    FormatBuffer output;
    FormatBuffer line;
    int indent;
    bool pending_blank;
} FormatWriter;

static bool format_buffer_reserve(FormatBuffer *buffer, size_t extra)
{
    if (!buffer) return false;
    if (extra > SIZE_MAX - buffer->length - 1u) return false;
    const size_t needed = buffer->length + extra + 1u;
    if (needed <= buffer->capacity) return true;

    size_t capacity = buffer->capacity ? buffer->capacity : 128u;
    while (capacity < needed)
    {
        if (capacity > SIZE_MAX / 2u)
        {
            capacity = needed;
            break;
        }
        capacity *= 2u;
    }
    char *grown = (char *)realloc(buffer->data, capacity);
    if (!grown) return false;
    buffer->data = grown;
    buffer->capacity = capacity;
    return true;
}

static bool format_buffer_append(FormatBuffer *buffer,
                                 const char *text,
                                 size_t length)
{
    if (!buffer || (!text && length != 0u)) return false;
    if (!format_buffer_reserve(buffer, length)) return false;
    if (length) memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool format_buffer_append_char(FormatBuffer *buffer, char value)
{
    return format_buffer_append(buffer, &value, 1u);
}

static void format_buffer_trim_spaces(FormatBuffer *buffer)
{
    if (!buffer) return;
    while (buffer->length > 0u)
    {
        const char value = buffer->data[buffer->length - 1u];
        if (value != ' ' && value != '\t') break;
        buffer->length--;
    }
    if (buffer->data) buffer->data[buffer->length] = '\0';
}

static bool format_buffer_ends_with(const FormatBuffer *buffer,
                                    const char *suffix)
{
    if (!buffer || !suffix) return false;
    const size_t suffix_length = strlen(suffix);
    return suffix_length <= buffer->length &&
           memcmp(buffer->data + buffer->length - suffix_length,
                  suffix,
                  suffix_length) == 0;
}

static void format_buffer_free(FormatBuffer *buffer)
{
    if (!buffer) return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

static bool format_valid_utf8(const char *text)
{
    if (!text) return false;
    const unsigned char *cursor = (const unsigned char *)text;
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

static bool format_is_arabic_digit(const char *cursor)
{
    return cursor && (unsigned char)cursor[0] == 0xD9u &&
           (unsigned char)cursor[1] >= 0xA0u &&
           (unsigned char)cursor[1] <= 0xA9u;
}

static size_t format_utf8_length(const char *cursor)
{
    const unsigned char first = (unsigned char)*cursor;
    if (first < 0x80u) return 1u;
    if ((first & 0xE0u) == 0xC0u) return 2u;
    if ((first & 0xF0u) == 0xE0u) return 3u;
    return 4u;
}

static bool format_is_ascii_space(char value)
{
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

static size_t format_symbol_length(const char *cursor)
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
        return 2u; /* ، */
    if ((unsigned char)cursor[0] == 0xD8u &&
        (unsigned char)cursor[1] == 0x9Bu)
        return 2u; /* ؛ */
    return strchr("=.,:+-*/%!&|^~<>(){}[]", cursor[0]) ? 1u : 0u;
}

static bool format_tokens_push(FormatTokens *tokens, FormatToken token)
{
    if (!tokens) return false;
    if (tokens->count == tokens->capacity)
    {
        size_t capacity = tokens->capacity ? tokens->capacity * 2u : 128u;
        if (capacity < tokens->count ||
            capacity > SIZE_MAX / sizeof(FormatToken))
            return false;
        FormatToken *grown = (FormatToken *)realloc(
            tokens->items, capacity * sizeof(FormatToken));
        if (!grown) return false;
        tokens->items = grown;
        tokens->capacity = capacity;
    }
    tokens->items[tokens->count++] = token;
    return true;
}

static bool format_scan_tokens(const char *source, FormatTokens *tokens)
{
    if (!source || !tokens) return false;
    const char *cursor = source;
    if ((unsigned char)cursor[0] == 0xEFu &&
        (unsigned char)cursor[1] == 0xBBu &&
        (unsigned char)cursor[2] == 0xBFu)
        cursor += 3;

    bool source_line_start = true;
    int newlines = 0;
    while (*cursor)
    {
        while (*cursor && format_is_ascii_space(*cursor))
        {
            if (*cursor == '\r')
            {
                if (cursor[1] == '\n') cursor++;
                newlines++;
                source_line_start = true;
            }
            else if (*cursor == '\n')
            {
                newlines++;
                source_line_start = true;
            }
            cursor++;
        }
        if (!*cursor) break;

        const char *start = cursor;
        FormatTokenKind kind = FORMAT_TOKEN_WORD;
        if (source_line_start && *cursor == '#')
        {
            kind = FORMAT_TOKEN_DIRECTIVE;
            while (*cursor && *cursor != '\r' && *cursor != '\n') cursor++;
        }
        else if (cursor[0] == '/' && cursor[1] == '/')
        {
            kind = FORMAT_TOKEN_COMMENT;
            cursor += 2;
            while (*cursor && *cursor != '\r' && *cursor != '\n') cursor++;
        }
        else if (*cursor == '"' || *cursor == '\'')
        {
            kind = FORMAT_TOKEN_LITERAL;
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
            size_t symbol_length = format_symbol_length(cursor);
            if (symbol_length)
            {
                kind = FORMAT_TOKEN_SYMBOL;
                cursor += symbol_length;
            }
            else
            {
                const bool numeric =
                    (*cursor >= '0' && *cursor <= '9') ||
                    format_is_arabic_digit(cursor);
                bool decimal_seen = false;
                while (*cursor && !format_is_ascii_space(*cursor))
                {
                    symbol_length = format_symbol_length(cursor);
                    if (symbol_length)
                    {
                        if (numeric && !decimal_seen && *cursor == '.' &&
                            ((cursor[1] >= '0' && cursor[1] <= '9') ||
                             format_is_arabic_digit(cursor + 1)))
                        {
                            decimal_seen = true;
                            cursor++;
                            continue;
                        }
                        break;
                    }
                    cursor += format_utf8_length(cursor);
                }
            }
        }

        if (!format_tokens_push(tokens,
                                (FormatToken){
                                    kind,
                                    start,
                                    (size_t)(cursor - start),
                                    newlines
                                }))
            return false;
        newlines = 0;
        source_line_start = false;
    }
    return true;
}

static bool format_token_equals(const FormatToken *token, const char *text)
{
    if (!token || !text) return false;
    const size_t length = strlen(text);
    return token->length == length &&
           memcmp(token->start, text, length) == 0;
}

static bool format_current_initializer(const bool *brace_initializers,
                                       size_t brace_count)
{
    return brace_count > 0u && brace_initializers[brace_count - 1u];
}

static bool format_writer_append_indent(FormatWriter *writer)
{
    if (!writer || writer->line.length != 0u) return true;
    const int width = writer->indent > 0 ? writer->indent * 4 : 0;
    for (int index = 0; index < width; ++index)
    {
        if (!format_buffer_append_char(&writer->line, ' ')) return false;
    }
    return true;
}

static bool format_writer_append(FormatWriter *writer,
                                 const char *text,
                                 size_t length)
{
    return format_writer_append_indent(writer) &&
           format_buffer_append(&writer->line, text, length);
}

static bool format_writer_space(FormatWriter *writer)
{
    if (!writer || writer->line.length == 0u) return true;
    const char last = writer->line.data[writer->line.length - 1u];
    return last == ' ' || last == '\t'
        ? true
        : format_buffer_append_char(&writer->line, ' ');
}

static bool format_writer_flush(FormatWriter *writer)
{
    if (!writer) return false;
    format_buffer_trim_spaces(&writer->line);
    if (writer->line.length == 0u) return true;
    if (writer->pending_blank && writer->output.length > 0u &&
        !format_buffer_ends_with(&writer->output, "\n\n"))
    {
        if (!format_buffer_append_char(&writer->output, '\n')) return false;
    }
    writer->pending_blank = false;
    if (!format_buffer_append(&writer->output,
                              writer->line.data,
                              writer->line.length) ||
        !format_buffer_append_char(&writer->output, '\n'))
        return false;
    writer->line.length = 0u;
    if (writer->line.data) writer->line.data[0] = '\0';
    return true;
}

static bool format_append_normalized_directive(FormatWriter *writer,
                                               const FormatToken *token)
{
    if (!writer || !token) return false;
    if (!format_writer_flush(writer)) return false;
    if (writer->pending_blank && writer->output.length > 0u &&
        !format_buffer_ends_with(&writer->output, "\n\n"))
    {
        if (!format_buffer_append_char(&writer->output, '\n')) return false;
    }
    writer->pending_blank = false;

    FormatBuffer directive = {0};
    bool quoted = false;
    bool escaped = false;
    bool pending_space = false;
    for (size_t index = 0u; index < token->length; ++index)
    {
        const char value = token->start[index];
        if (!quoted && (value == ' ' || value == '\t'))
        {
            pending_space = directive.length > 0u;
            continue;
        }
        if (pending_space)
        {
            if (!format_buffer_append_char(&directive, ' '))
            {
                format_buffer_free(&directive);
                return false;
            }
            pending_space = false;
        }
        if (!format_buffer_append_char(&directive, value))
        {
            format_buffer_free(&directive);
            return false;
        }
        if (quoted && escaped)
        {
            escaped = false;
        }
        else if (quoted && value == '\\')
        {
            escaped = true;
        }
        else if (value == '"')
        {
            quoted = !quoted;
        }
    }

    const bool ok = format_buffer_append(
        &writer->output, directive.data, directive.length) &&
        format_buffer_append_char(&writer->output, '\n');
    format_buffer_free(&directive);
    writer->pending_blank = false;
    return ok;
}

static bool format_control_word(const FormatToken *token)
{
    return format_token_equals(token, "إذا") ||
           format_token_equals(token, "طالما") ||
           format_token_equals(token, "لكل") ||
           format_token_equals(token, "اختر");
}

static bool format_type_word(const FormatToken *token)
{
    if (!token || token->kind != FORMAT_TOKEN_WORD) return false;
    char small[64];
    if (token->length >= sizeof(small)) return false;
    memcpy(small, token->start, token->length);
    small[token->length] = '\0';
    BaaTokenType type = TOKEN_INVALID;
    if (!baa_language_keyword_token(small, &type)) return false;
    return type == TOKEN_KEYWORD_INT || type == TOKEN_KEYWORD_I8 ||
           type == TOKEN_KEYWORD_I16 || type == TOKEN_KEYWORD_I32 ||
           type == TOKEN_KEYWORD_I64 || type == TOKEN_KEYWORD_U8 ||
           type == TOKEN_KEYWORD_U16 || type == TOKEN_KEYWORD_U32 ||
           type == TOKEN_KEYWORD_U64 || type == TOKEN_KEYWORD_STRING ||
           type == TOKEN_KEYWORD_BOOL || type == TOKEN_KEYWORD_CHAR ||
           type == TOKEN_KEYWORD_FLOAT || type == TOKEN_KEYWORD_FLOAT32 ||
           type == TOKEN_KEYWORD_VOID || type == TOKEN_STRUCT ||
           type == TOKEN_UNION || type == TOKEN_ENUM;
}

static bool format_binary_operator(const FormatToken *token)
{
    return format_token_equals(token, "=") ||
           format_token_equals(token, "==") ||
           format_token_equals(token, "!=") ||
           format_token_equals(token, "<") ||
           format_token_equals(token, ">") ||
           format_token_equals(token, "<=") ||
           format_token_equals(token, ">=") ||
           format_token_equals(token, "&&") ||
           format_token_equals(token, "||") ||
           format_token_equals(token, "|") ||
           format_token_equals(token, "^") ||
           format_token_equals(token, "<<") ||
           format_token_equals(token, ">>") ||
           format_token_equals(token, "/") ||
           format_token_equals(token, "%");
}

static bool format_unary_context(const FormatToken *previous)
{
    if (!previous) return true;
    if (previous->kind == FORMAT_TOKEN_WORD)
    {
        return format_token_equals(previous, "إرجع") ||
               format_token_equals(previous, "حالة");
    }
    if (previous->kind != FORMAT_TOKEN_SYMBOL) return false;
    return format_token_equals(previous, "(") ||
           format_token_equals(previous, "[") ||
           format_token_equals(previous, "{") ||
           format_token_equals(previous, "=") ||
           format_token_equals(previous, ",") ||
           format_token_equals(previous, "،") ||
           format_token_equals(previous, "؛") ||
           format_token_equals(previous, ":") ||
           format_binary_operator(previous) ||
           format_token_equals(previous, "+") ||
           format_token_equals(previous, "-") ||
           format_token_equals(previous, "*") ||
           format_token_equals(previous, "&") ||
           format_token_equals(previous, "!") ||
           format_token_equals(previous, "~");
}

static bool format_word_needs_space(const FormatToken *previous,
                                    bool previous_binary,
                                    bool previous_unary,
                                    bool initializer)
{
    if (!previous) return false;
    if (previous_binary) return true;
    if (previous_unary) return false;
    if (previous->kind == FORMAT_TOKEN_WORD ||
        previous->kind == FORMAT_TOKEN_LITERAL)
        return true;
    if (previous->kind != FORMAT_TOKEN_SYMBOL) return true;
    if (format_token_equals(previous, "(") ||
        format_token_equals(previous, "[") ||
        format_token_equals(previous, "<"))
        return false;
    if (format_token_equals(previous, ":")) return initializer;
    return format_token_equals(previous, ",") ||
           format_token_equals(previous, "،") ||
           format_token_equals(previous, "؛") ||
           format_token_equals(previous, ")") ||
           format_token_equals(previous, "]") ||
           format_token_equals(previous, "}") ||
           format_token_equals(previous, "...");
}

static bool format_source_tokens(const FormatTokens *tokens, FormatBuffer *output)
{
    if (!tokens || !output) return false;
    FormatWriter writer = {0};
    bool *brace_initializers = (bool *)calloc(
        tokens->count + 1u, sizeof(bool));
    if (!brace_initializers) return false;

    size_t brace_count = 0u;
    int paren_depth = 0;
    int bracket_depth = 0;
    int type_angle_depth = 0;
    bool declaration_type_seen = false;
    bool statement_assignment_seen = false;
    bool previous_binary = false;
    bool previous_unary = false;
    const FormatToken *previous = NULL;

    for (size_t index = 0u; index < tokens->count; ++index)
    {
        const FormatToken *token = &tokens->items[index];
        const FormatToken *next =
            index + 1u < tokens->count ? &tokens->items[index + 1u] : NULL;
        if (token->newlines_before >= 2) writer.pending_blank = true;

        if (token->kind == FORMAT_TOKEN_DIRECTIVE)
        {
            if (!format_append_normalized_directive(&writer, token)) goto fail;
            previous = token;
            previous_binary = false;
            previous_unary = false;
            declaration_type_seen = false;
            statement_assignment_seen = false;
            continue;
        }
        if (token->kind == FORMAT_TOKEN_COMMENT)
        {
            if (writer.line.length > 0u && !format_writer_space(&writer))
                goto fail;
            if (!format_writer_append(&writer, token->start, token->length) ||
                !format_writer_flush(&writer))
                goto fail;
            previous = token;
            previous_binary = false;
            previous_unary = false;
            continue;
        }

        const bool initializer =
            format_current_initializer(brace_initializers, brace_count);
        if (token->kind == FORMAT_TOKEN_WORD ||
            token->kind == FORMAT_TOKEN_LITERAL)
        {
            if (format_word_needs_space(previous,
                                        previous_binary,
                                        previous_unary,
                                        initializer) &&
                !format_writer_space(&writer))
                goto fail;
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            if (format_type_word(token)) declaration_type_seen = true;
            previous = token;
            previous_binary = false;
            previous_unary = false;
            continue;
        }

        if (format_token_equals(token, "("))
        {
            if (previous && format_control_word(previous) &&
                !format_writer_space(&writer))
                goto fail;
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            paren_depth++;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, ")"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            if (paren_depth > 0) paren_depth--;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "["))
        {
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            bracket_depth++;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "]"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            if (bracket_depth > 0) bracket_depth--;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "{"))
        {
            const bool parent_initializer =
                format_current_initializer(brace_initializers, brace_count);
            const bool new_initializer =
                (previous && format_token_equals(previous, "=")) ||
                (parent_initializer &&
                 previous &&
                 (format_token_equals(previous, "{") ||
                  format_token_equals(previous, ",") ||
                  format_token_equals(previous, "،")));
            brace_initializers[brace_count++] = new_initializer;
            if (new_initializer)
            {
                if (writer.line.length > 0u &&
                    !format_writer_space(&writer))
                    goto fail;
                if (!format_writer_append(&writer, "{", 1u)) goto fail;
                if (next && !format_token_equals(next, "}") &&
                    !format_writer_space(&writer))
                    goto fail;
            }
            else
            {
                if (writer.line.length > 0u &&
                    !format_writer_space(&writer))
                    goto fail;
                if (!format_writer_append(&writer, "{", 1u) ||
                    !format_writer_flush(&writer))
                    goto fail;
                writer.indent++;
            }
            declaration_type_seen = false;
            statement_assignment_seen = false;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "}"))
        {
            const bool closing_initializer =
                format_current_initializer(brace_initializers, brace_count);
            if (brace_count > 0u) brace_count--;
            if (closing_initializer)
            {
                format_buffer_trim_spaces(&writer.line);
                if (previous && !format_token_equals(previous, "{") &&
                    !format_writer_space(&writer))
                    goto fail;
                if (!format_writer_append(&writer, "}", 1u)) goto fail;
            }
            else
            {
                if (!format_writer_flush(&writer)) goto fail;
                if (writer.indent > 0) writer.indent--;
                if (!format_writer_append(&writer, "}", 1u)) goto fail;
                if (!next ||
                    (!format_token_equals(next, "وإلا") &&
                     next->kind != FORMAT_TOKEN_COMMENT))
                {
                    if (!format_writer_flush(&writer)) goto fail;
                }
            }
            declaration_type_seen = false;
            statement_assignment_seen = false;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "."))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, ".", 1u)) goto fail;
            if (paren_depth == 0 && bracket_depth == 0 &&
                !format_current_initializer(brace_initializers, brace_count) &&
                !format_writer_flush(&writer))
                goto fail;
            declaration_type_seen = false;
            statement_assignment_seen = false;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, ",") ||
                 format_token_equals(token, "،"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            if (paren_depth > 0 || bracket_depth > 0 || initializer)
            {
                if (!format_writer_space(&writer)) goto fail;
            }
            else if (!format_writer_flush(&writer))
            {
                goto fail;
            }
            declaration_type_seen = false;
            statement_assignment_seen = false;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "؛"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            if (paren_depth > 0)
            {
                if (!format_writer_space(&writer)) goto fail;
            }
            else if (!format_writer_flush(&writer))
            {
                goto fail;
            }
            declaration_type_seen = false;
            statement_assignment_seen = false;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, ":"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, ":", 1u)) goto fail;
            if (initializer && !format_writer_space(&writer)) goto fail;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "<") &&
                 previous && format_token_equals(previous, "كـ"))
        {
            if (!format_writer_append(&writer, "<", 1u)) goto fail;
            type_angle_depth++;
            previous_binary = false;
            previous_unary = false;
        }
        else if (type_angle_depth > 0 && format_token_equals(token, ">"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, ">", 1u)) goto fail;
            type_angle_depth--;
            previous_binary = false;
            previous_unary = false;
        }
        else if (type_angle_depth > 0 && format_token_equals(token, "*"))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, "*", 1u)) goto fail;
            previous_binary = false;
            previous_unary = false;
        }
        else if (format_token_equals(token, "!") ||
                 format_token_equals(token, "~"))
        {
            if (previous && (previous->kind == FORMAT_TOKEN_WORD ||
                             previous->kind == FORMAT_TOKEN_LITERAL) &&
                !format_writer_space(&writer))
                goto fail;
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            previous_binary = false;
            previous_unary = true;
        }
        else if (format_token_equals(token, "++") ||
                 format_token_equals(token, "--"))
        {
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            previous_binary = false;
            previous_unary = format_unary_context(previous);
        }
        else if (format_token_equals(token, "*") &&
                 declaration_type_seen && !statement_assignment_seen &&
                 previous &&
                 (previous->kind == FORMAT_TOKEN_WORD ||
                  format_token_equals(previous, "*")) &&
                 next &&
                 (next->kind == FORMAT_TOKEN_WORD ||
                  format_token_equals(next, "*")))
        {
            format_buffer_trim_spaces(&writer.line);
            if (!format_writer_append(&writer, "*", 1u)) goto fail;
            previous_binary = next->kind == FORMAT_TOKEN_WORD;
            previous_unary = false;
        }
        else if (format_token_equals(token, "+") ||
                 format_token_equals(token, "-") ||
                 format_token_equals(token, "*") ||
                 format_token_equals(token, "&"))
        {
            const bool unary = format_unary_context(previous);
            if (!unary && writer.line.length > 0u &&
                !format_writer_space(&writer))
                goto fail;
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            previous_binary = !unary;
            previous_unary = unary;
        }
        else if (format_binary_operator(token))
        {
            if (writer.line.length > 0u && !format_writer_space(&writer))
                goto fail;
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            if (format_token_equals(token, "="))
                statement_assignment_seen = true;
            previous_binary = true;
            previous_unary = false;
        }
        else
        {
            if (!format_writer_append(&writer, token->start, token->length))
                goto fail;
            previous_binary = false;
            previous_unary = false;
        }
        previous = token;
    }

    if (!format_writer_flush(&writer)) goto fail;
    while (writer.output.length > 0u &&
           (writer.output.data[writer.output.length - 1u] == '\n' ||
            writer.output.data[writer.output.length - 1u] == '\r' ||
            writer.output.data[writer.output.length - 1u] == ' ' ||
            writer.output.data[writer.output.length - 1u] == '\t'))
        writer.output.length--;
    if (writer.output.length > 0u &&
        !format_buffer_append_char(&writer.output, '\n'))
        goto fail;
    if (writer.output.data) writer.output.data[writer.output.length] = '\0';

    free(brace_initializers);
    format_buffer_free(&writer.line);
    *output = writer.output;
    return true;

fail:
    free(brace_initializers);
    format_buffer_free(&writer.output);
    format_buffer_free(&writer.line);
    return false;
}

BaaFormatStatus baa_format_source(const char *source, BaaFormatOutput *output)
{
    if (!source || !output) return BAA_FORMAT_OUT_OF_MEMORY;
    memset(output, 0, sizeof(*output));
    if (!format_valid_utf8(source)) return BAA_FORMAT_INVALID_UTF8;

    FormatTokens tokens = {0};
    if (!format_scan_tokens(source, &tokens))
    {
        free(tokens.items);
        return BAA_FORMAT_OUT_OF_MEMORY;
    }

    FormatBuffer formatted = {0};
    if (!format_source_tokens(&tokens, &formatted))
    {
        free(tokens.items);
        return BAA_FORMAT_OUT_OF_MEMORY;
    }
    free(tokens.items);

    if (!formatted.data)
    {
        formatted.data = (char *)malloc(1u);
        if (!formatted.data) return BAA_FORMAT_OUT_OF_MEMORY;
        formatted.data[0] = '\0';
    }
    output->changed = strcmp(source, formatted.data) != 0;
    output->text = formatted.data;
    return BAA_FORMAT_OK;
}

void baa_format_output_free(BaaFormatOutput *output)
{
    if (!output) return;
    free(output->text);
    memset(output, 0, sizeof(*output));
}
