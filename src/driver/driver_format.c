/**
 * @file driver_format.c
 * @brief إخراج format-json-v1 من منسق باء المملوك للمصرف.
 */

#include "driver_format.h"

#include <string.h>

static void format_json_string(FILE *out, const char *text)
{
    fputc('"', out);
    if (text)
    {
        for (const unsigned char *cursor = (const unsigned char *)text;
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

BaaFormatStatus driver_format_json_write(FILE *out,
                                         const char *compiler_version,
                                         const char *file,
                                         const char *source)
{
    if (!out || !source) return BAA_FORMAT_OUT_OF_MEMORY;
    BaaFormatOutput formatted = {0};
    const BaaFormatStatus status = baa_format_source(source, &formatted);
    if (status != BAA_FORMAT_OK) return status;

    fputs("{\"schema_version\":\"format-json-v1\",\"compiler_version\":", out);
    format_json_string(out, compiler_version ? compiler_version : "");
    fputs(",\"language\":\"baa\",\"file\":", out);
    format_json_string(out, file ? file : "");
    fputs(",\"position_encoding\":\"utf-8-bytes\","
          "\"line_ending\":\"lf\",\"indent_width\":4,\"insert_spaces\":true,"
          "\"source_bytes\":", out);
    fprintf(out, "%zu", strlen(source));
    fputs(",\"formatted_bytes\":", out);
    fprintf(out, "%zu", strlen(formatted.text));
    fputs(",\"changed\":", out);
    fputs(formatted.changed ? "true" : "false", out);
    fputs(",\"formatted_text\":", out);
    format_json_string(out, formatted.text);
    fputs("}\n", out);

    baa_format_output_free(&formatted);
    return ferror(out) ? BAA_FORMAT_OUT_OF_MEMORY : BAA_FORMAT_OK;
}
