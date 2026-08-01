static void driver_nazm_report_unsupported(const char *reason,
                                           const char *blocker_kind,
                                           const char *blocker_detail,
                                           const char *source_file,
                                           int source_line,
                                           int source_col)
{
    const char *message = reason ? reason : "صيغة نظم غير مدعومة.";
    const char *kind = blocker_kind ? blocker_kind : "غير_مصنف";
    if (source_file && source_file[0] && source_line > 0)
        fprintf(stderr, "%s:%d:%d: ", source_file, source_line,
                source_col > 0 ? source_col : 1);
    if (blocker_detail && blocker_detail[0])
        fprintf(stderr,
                "خطأ: %s [عائق_نظم=%s؛تفصيل=%s]\n",
                message,
                kind,
                blocker_detail);
    else
        fprintf(stderr, "خطأ: %s [عائق_نظم=%s]\n", message, kind);
}

static int driver_nazm_hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return 10 + value - 'a';
    return -1;
}

static char *driver_nazm_decode_utf8_hex(const char *begin, const char *end)
{
    if (!begin || !end || end < begin) return NULL;
    size_t hex_len = (size_t)(end - begin);
    if ((hex_len & 1u) != 0u) return NULL;
    char *decoded = (char *)malloc(hex_len / 2u + 1u);
    if (!decoded) return NULL;
    for (size_t i = 0; i < hex_len; i += 2u)
    {
        int high = driver_nazm_hex_value(begin[i]);
        int low = driver_nazm_hex_value(begin[i + 1u]);
        if (high < 0 || low < 0)
        {
            free(decoded);
            return NULL;
        }
        decoded[i / 2u] = (char)((high << 4) | low);
    }
    decoded[hex_len / 2u] = '\0';
    return decoded;
}

static bool driver_nazm_parse_unsigned_after(const char *entry,
                                             const char *field,
                                             unsigned *out_value)
{
    if (!entry || !field || !out_value) return false;
    const char *position = strstr(entry, field);
    if (!position) return false;
    position += strlen(field);
    while (*position == ' ' || *position == '\t') ++position;
    if (!isdigit((unsigned char)*position)) return false;
    char *end = NULL;
    unsigned long value = strtoul(position, &end, 10);
    if (end == position || value > UINT_MAX) return false;
    *out_value = (unsigned)value;
    return true;
}

static bool driver_nazm_lookup_source_map(const char *map_path,
                                          unsigned generated_line,
                                          char **out_file,
                                          unsigned *out_line,
                                          unsigned *out_col)
{
    if (out_file) *out_file = NULL;
    if (!map_path || !out_file || !out_line || !out_col) return false;
    char *document = read_file(map_path);
    if (!document || !strstr(document, "\"schema\": \"baa-nazm-source-map-v1\""))
    {
        free(document);
        return false;
    }

    const char *cursor = document;
    bool found = false;
    while ((cursor = strstr(cursor, "\"generated_line_start\":")) != NULL)
    {
        unsigned start = 0;
        unsigned end = 0;
        unsigned source_line = 0;
        unsigned source_col = 0;
        if (!driver_nazm_parse_unsigned_after(cursor, "\"generated_line_start\":", &start) ||
            !driver_nazm_parse_unsigned_after(cursor, "\"generated_line_end\":", &end) ||
            !driver_nazm_parse_unsigned_after(cursor, "\"source_line\":", &source_line) ||
            !driver_nazm_parse_unsigned_after(cursor, "\"source_column\":", &source_col))
        {
            ++cursor;
            continue;
        }
        const char *hex_field = strstr(cursor, "\"source_file_utf8_hex\": \"");
        if (!hex_field)
        {
            ++cursor;
            continue;
        }
        hex_field += strlen("\"source_file_utf8_hex\": \"");
        const char *hex_end = strchr(hex_field, '"');
        if (!hex_end)
        {
            ++cursor;
            continue;
        }
        if (generated_line >= start && generated_line <= end)
        {
            char *source_file = driver_nazm_decode_utf8_hex(hex_field, hex_end);
            if (source_file)
            {
                *out_file = source_file;
                *out_line = source_line;
                *out_col = source_col;
                found = true;
            }
            break;
        }
        cursor = hex_end + 1;
    }
    free(document);
    return found;
}

static bool driver_nazm_generated_diagnostic_line(const char *diagnostic,
                                                  unsigned *out_line)
{
    if (!diagnostic || !out_line) return false;
    for (const char *cursor = diagnostic; *cursor; ++cursor)
    {
        if (*cursor != ':' || !isdigit((unsigned char)cursor[1])) continue;
        char *line_end = NULL;
        unsigned long line = strtoul(cursor + 1, &line_end, 10);
        if (!line_end || *line_end != ':' ||
            !isdigit((unsigned char)line_end[1]))
            continue;
        char *column_end = NULL;
        (void)strtoul(line_end + 1, &column_end, 10);
        if (!column_end || *column_end != ':' || line == 0 || line > UINT_MAX)
            continue;
        *out_line = (unsigned)line;
        return true;
    }
    return false;
}

static void driver_nazm_replay_diagnostic(const char *diagnostic_path,
                                          const char *map_path)
{
    char *diagnostic = diagnostic_path ? read_file(diagnostic_path) : NULL;
    if (!diagnostic || !diagnostic[0])
    {
        free(diagnostic);
        return;
    }
    fputs(diagnostic, stderr);
    size_t length = strlen(diagnostic);
    if (length > 0 && diagnostic[length - 1u] != '\n') fputc('\n', stderr);

    unsigned generated_line = 0;
    char *source_file = NULL;
    unsigned source_line = 0;
    unsigned source_col = 0;
    if (driver_nazm_generated_diagnostic_line(diagnostic, &generated_line) &&
        driver_nazm_lookup_source_map(map_path,
                                      generated_line,
                                      &source_file,
                                      &source_line,
                                      &source_col))
    {
        fprintf(stderr,
                "موضع باء الأصلي: %s:%u:%u (من سطر نظم %u).\n",
                source_file,
                source_line,
                source_col,
                generated_line);
    }
    free(source_file);
    free(diagnostic);
}
