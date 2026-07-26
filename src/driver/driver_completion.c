/**
 * @file driver_completion.c
 * @brief محوّل مفردات باء المركزية إلى completion-data-json-v1.
 */

#include "driver_completion.h"

#include "../frontend/language_profile.h"

static void completion_json_escape(FILE *out, const char *text)
{
    fputc('"', out);
    if (text)
    {
        for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        {
            switch (*p)
            {
                case '"': fputs("\\\"", out); break;
                case '\\': fputs("\\\\", out); break;
                case '\b': fputs("\\b", out); break;
                case '\f': fputs("\\f", out); break;
                case '\n': fputs("\\n", out); break;
                case '\r': fputs("\\r", out); break;
                case '\t': fputs("\\t", out); break;
                default:
                    if (*p < 0x20u) fprintf(out, "\\u%04x", (unsigned int)*p);
                    else fputc((int)*p, out);
                    break;
            }
        }
    }
    fputc('"', out);
}

static void completion_print_item(FILE *out,
                                  const char *label,
                                  const char *kind,
                                  const char *detail,
                                  const char *filter_text,
                                  const char *insert_text,
                                  bool snippet,
                                  bool contextual)
{
    fputs("{\"label\":", out);
    completion_json_escape(out, label);
    fputs(",\"kind\":", out);
    completion_json_escape(out, kind);
    fputs(",\"detail\":", out);
    completion_json_escape(out, detail);
    fputs(",\"filter_text\":", out);
    completion_json_escape(out, filter_text);
    fputs(",\"insert_text\":", out);
    completion_json_escape(out, insert_text);
    fputs(",\"insert_text_format\":", out);
    completion_json_escape(out, snippet ? "snippet" : "plain");
    if (contextual) fputs(",\"contextual\":true", out);
    fputc('}', out);
}

bool driver_completion_data_json_write(FILE *out, const char *compiler_version)
{
    if (!out) return false;

    fputs("{\"schema_version\":\"completion-data-json-v1\",\"compiler_version\":", out);
    completion_json_escape(out, compiler_version ? compiler_version : "");
    fputs(",\"language\":\"baa\",\"items\":[", out);

    bool first = true;
    size_t count = 0;
    const BaaLanguageKeyword *keywords = baa_language_keywords(&count);
    for (size_t i = 0; i < count; ++i)
    {
        if (!first) fputc(',', out);
        completion_print_item(out,
                              keywords[i].label,
                              keywords[i].completion_kind,
                              keywords[i].detail,
                              keywords[i].label,
                              keywords[i].label,
                              false,
                              !keywords[i].lexical_keyword);
        first = false;
    }

    const BaaLanguageCompletionEntry *groups[] = {
        baa_language_directives(&count),
        NULL,
    };
    const size_t directive_count = count;
    groups[1] = baa_language_snippets(&count);
    const size_t group_counts[] = {directive_count, count};
    for (size_t group = 0; group < 2; ++group)
    {
        for (size_t i = 0; i < group_counts[group]; ++i)
        {
            const BaaLanguageCompletionEntry *entry = &groups[group][i];
            if (!first) fputc(',', out);
            completion_print_item(out,
                                  entry->label,
                                  entry->completion_kind,
                                  entry->detail,
                                  entry->filter_text,
                                  entry->insert_text,
                                  entry->snippet,
                                  false);
            first = false;
        }
    }

    fputs("]}\n", out);
    return ferror(out) == 0;
}
