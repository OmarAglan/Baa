/**
 * @file driver_completion.c
 * @brief محوّل مفردات باء المركزية إلى completion-data-json-v1.
 */

#include "driver_completion.h"

#include "../frontend/analysis.h"
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
                                  const char *documentation,
                                  const char *filter_text,
                                  const char *insert_text,
                                  bool snippet,
                                  bool contextual,
                                  int relevance)
{
    fputs("{\"label\":", out);
    completion_json_escape(out, label);
    fputs(",\"kind\":", out);
    completion_json_escape(out, kind);
    fputs(",\"detail\":", out);
    completion_json_escape(out, detail);
    fputs(",\"documentation\":", out);
    completion_json_escape(out, documentation);
    fputs(",\"filter_text\":", out);
    completion_json_escape(out, filter_text);
    fputs(",\"insert_text\":", out);
    completion_json_escape(out, insert_text);
    fputs(",\"insert_text_format\":", out);
    completion_json_escape(out, snippet ? "snippet" : "plain");
    if (contextual) fputs(",\"contextual\":true", out);
    fprintf(out, ",\"relevance\":%d", relevance);
    fputc('}', out);
}

static const char *completion_type_name(DataType type)
{
    switch (type)
    {
        case TYPE_INT: return "صحيح";
        case TYPE_I8: return "ص٨";
        case TYPE_I16: return "ص١٦";
        case TYPE_I32: return "ص٣٢";
        case TYPE_U8: return "ط٨";
        case TYPE_U16: return "ط١٦";
        case TYPE_U32: return "ط٣٢";
        case TYPE_U64: return "ط٦٤";
        case TYPE_STRING: return "نص";
        case TYPE_POINTER: return "مؤشر";
        case TYPE_FUNC_PTR: return "دالة";
        case TYPE_BOOL: return "منطقي";
        case TYPE_CHAR: return "حرف";
        case TYPE_FLOAT: return "عشري";
        case TYPE_VOID: return "عدم";
        case TYPE_ENUM: return "تعداد";
        case TYPE_STRUCT: return "هيكل";
        case TYPE_UNION: return "اتحاد";
        default: return "غير_معروف";
    }
}

static void completion_builtin_detail(char *buffer,
                                      size_t capacity,
                                      const BaaBuiltinDescriptor *builtin)
{
    if (!buffer || capacity == 0 || !builtin) return;
    size_t used = 0;
    const int initial = snprintf(buffer, capacity, "%s(",
                                 builtin->name ? builtin->name : "");
    if (initial < 0) return;
    used = (size_t)initial < capacity ? (size_t)initial : capacity - 1;
    for (int i = 0; i < builtin->param_count && used + 1 < capacity; ++i)
    {
        const int written = snprintf(
            buffer + used,
            capacity - used,
            "%s%s",
            i > 0 ? "، " : "",
            completion_type_name(
                builtin->param_types ? builtin->param_types[i] : TYPE_INT));
        if (written < 0 || (size_t)written >= capacity - used)
        {
            used = capacity - 1;
            break;
        }
        used += (size_t)written;
    }
    if (builtin->variadic && used + 1 < capacity)
    {
        const int written = snprintf(buffer + used, capacity - used, "%s...",
                                     builtin->param_count > 0 ? "، " : "");
        if (written > 0 && (size_t)written < capacity - used)
            used += (size_t)written;
    }
    if (used + 1 < capacity)
        snprintf(buffer + used, capacity - used, ") ← %s",
                 completion_type_name(builtin->return_type));
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
                              keywords[i].detail,
                              keywords[i].label,
                              keywords[i].label,
                              false,
                              !keywords[i].lexical_keyword,
                              40);
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
                                  entry->detail,
                                  entry->filter_text,
                                  entry->insert_text,
                                  entry->snippet,
                                  false,
                                  group == 0 ? 10 : 20);
            first = false;
        }
    }

    size_t builtin_count = 0;
    const BaaBuiltinDescriptor *builtins =
        baa_builtin_descriptors(&builtin_count);
    for (size_t i = 0; i < builtin_count; ++i)
    {
        char detail[1024] = {0};
        completion_builtin_detail(detail, sizeof(detail), &builtins[i]);
        if (!first) fputc(',', out);
        completion_print_item(out,
                              builtins[i].name,
                              "function",
                              detail,
                              "دالة مدمجة يملك مترجم باء تعريفها وتحققها الدلالي.",
                              builtins[i].name,
                              builtins[i].name,
                              false,
                              false,
                              30);
        first = false;
    }

    fputs("]}\n", out);
    return ferror(out) == 0;
}
