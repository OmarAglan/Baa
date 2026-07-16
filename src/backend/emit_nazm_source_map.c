/**
 * @file emit_nazm_source_map.c
 * @brief خريطة مصدر باء وتوجيهات مواضع التنقيح العربية لمصدر نظم المولد.
 *
 * This file is included by emit_nazm.c and is not a separate translation unit.
 */

typedef struct
{
    FILE *out;
    bool first_entry;
    unsigned generated_line;
    bool debug_info;
    const char **debug_files;
    int debug_file_count;
    int debug_file_capacity;
    int last_debug_file;
    int last_debug_line;
    int last_debug_column;
    bool debug_error;
} NazmSourceMapWriter;

static int nazm_debug_file_id(FILE *out,
                              NazmSourceMapWriter *map,
                              const char *path,
                              unsigned *written_lines)
{
    if (!out || !map || !path) return 0;
    for (int i = 0; i < map->debug_file_count; ++i)
    {
        if (map->debug_files[i] &&
            strcmp(map->debug_files[i], path) == 0)
            return i + 1;
    }

    if (map->debug_file_count >= map->debug_file_capacity)
    {
        int new_capacity = map->debug_file_capacity == 0
                         ? 8
                         : map->debug_file_capacity * 2;
        const char **new_files = (const char **)realloc(
            map->debug_files,
            (size_t)new_capacity * sizeof(const char *));
        if (!new_files)
        {
            map->debug_error = true;
            return 0;
        }
        map->debug_files = new_files;
        map->debug_file_capacity = new_capacity;
    }

    int id = map->debug_file_count + 1;
    map->debug_files[map->debug_file_count++] = path;
    fputs("    .ملف_بايتات ", out);
    nazm_write_unsigned(out, (uint64_t)id);
    fputs("، \"", out);
    bool first_byte = true;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p)
    {
        if (!first_byte) fputs("،", out);
        first_byte = false;
        nazm_write_unsigned(out, (uint64_t)*p);
    }
    fputs("\"\n", out);
    if (written_lines) *written_lines += 1;
    return id;
}

static unsigned nazm_write_debug_location_reset(
    FILE *out,
    NazmSourceMapWriter *map)
{
    if (!out || !map || !map->debug_info) return 0;
    fputs("    .موضع ٠، ٠، ٠\n", out);
    map->last_debug_file = 0;
    map->last_debug_line = 0;
    map->last_debug_column = 0;
    return 1;
}

static unsigned nazm_write_source_span(FILE *out,
                                       NazmSourceMapWriter *map,
                                       const MachineInst *inst)
{
    if (!inst || inst->src_line <= 0) return 0;
    unsigned lines = 0;
    if (map && map->debug_info && inst->src_file)
    {
        int file_id =
            nazm_debug_file_id(out, map, inst->src_file, &lines);
        int column = inst->src_col > 0 ? inst->src_col : 1;
        if (file_id > 0 &&
            (file_id != map->last_debug_file ||
             inst->src_line != map->last_debug_line ||
             column != map->last_debug_column))
        {
            fputs("    .موضع ", out);
            nazm_write_unsigned(out, (uint64_t)file_id);
            fputs("، ", out);
            nazm_write_unsigned(out, (uint64_t)inst->src_line);
            fputs("، ", out);
            nazm_write_unsigned(out, (uint64_t)column);
            fputc('\n', out);
            lines += 1;
            map->last_debug_file = file_id;
            map->last_debug_line = inst->src_line;
            map->last_debug_column = column;
        }
    }

    fputs("    ; موضع باء: السطر ", out);
    nazm_write_unsigned(out, (uint64_t)inst->src_line);
    if (inst->src_col > 0)
    {
        fputs("، العمود ", out);
        nazm_write_unsigned(out, (uint64_t)inst->src_col);
    }
    fputc('\n', out);
    return lines + 1;
}

static void nazm_write_utf8_hex(FILE *out, const char *value)
{
    static const char digits[] = "0123456789abcdef";
    if (!out || !value) return;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p)
    {
        fputc(digits[*p >> 4], out);
        fputc(digits[*p & 0x0fu], out);
    }
}

static bool nazm_source_map_begin(NazmSourceMapWriter *map,
                                  FILE *out,
                                  const char *generated_path)
{
    if (!map) return false;
    map->out = out;
    map->first_entry = true;
    map->generated_line = 0;
    map->debug_files = NULL;
    map->debug_file_count = 0;
    map->debug_file_capacity = 0;
    map->last_debug_file = 0;
    map->last_debug_line = 0;
    map->last_debug_column = 0;
    if (!out) return true;
    fputs("{\n  \"schema\": \"baa-nazm-source-map-v1\",\n", out);
    fputs("  \"generated_path_utf8_hex\": \"", out);
    nazm_write_utf8_hex(out, generated_path ? generated_path : "");
    fputs("\",\n  \"entries\": [\n", out);
    return !ferror(out);
}

static void nazm_source_map_entry(NazmSourceMapWriter *map,
                                  unsigned generated_start,
                                  unsigned generated_end,
                                  const MachineInst *inst)
{
    if (!map || !map->out || !inst || !inst->src_file ||
        inst->src_line <= 0 || generated_start == 0 ||
        generated_end < generated_start)
        return;
    fputs(map->first_entry ? "" : ",\n", map->out);
    map->first_entry = false;
    fprintf(map->out,
            "    {\"generated_line_start\": %u, \"generated_line_end\": %u, "
            "\"source_file_utf8_hex\": \"",
            generated_start,
            generated_end);
    nazm_write_utf8_hex(map->out, inst->src_file);
    fprintf(map->out,
            "\", \"source_line\": %d, \"source_column\": %d}",
            inst->src_line,
            inst->src_col > 0 ? inst->src_col : 1);
}

static bool nazm_source_map_end(NazmSourceMapWriter *map)
{
    if (!map || !map->out) return true;
    fputs("\n  ]\n}\n", map->out);
    return !ferror(map->out);
}
