/**
 * @file driver_nazm.c
 * @brief تنفيذ وضع --emit-nazm مع فشل ظاهر ومن دون رجوع صامت إلى GAS.
 */

#include "driver_internal.h"
#include "driver_artifacts.h"

#ifdef BAA_EMBEDDED_NAZM
#include "nazm.h"
#endif

#include <ctype.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#endif

bool driver_nazm_is_source_path(const char *path)
{
    static const char suffix[] = ".نظم";
    if (!path) return false;
    size_t path_len = strlen(path);
    size_t suffix_len = sizeof(suffix) - 1u;
    return path_len >= suffix_len &&
           memcmp(path + path_len - suffix_len, suffix, suffix_len) == 0;
}

BaaCompilerExitCode driver_validate_nazm_inputs(
    const CompilerConfig *config,
    char **input_files,
    int input_count)
{
    if (!config || !input_files || input_count <= 0)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    bool has_direct_input = false;
    for (int i = 0; i < input_count; ++i)
    {
        if (driver_nazm_is_source_path(input_files[i]))
        {
            has_direct_input = true;
            break;
        }
    }
    if (!has_direct_input) return BAA_COMPILER_EXIT_SUCCESS;
    if (config->nazm_shadow_executable)
    {
        fprintf(stderr,
                "خطأ: --nazm-shadow يقارن خرج باء المولد فقط، "
                "ولا يقبل ملف `.نظم` مباشرا.\n");
        return BAA_COMPILER_EXIT_INVALID_INVOCATION;
    }
    if (config->assembly_only || config->emit_nazm ||
        config->check_only || config->header_check)
    {
        fprintf(stderr,
                "غير مدعوم: ملفات `.نظم` المباشرة تعمل حاليا مع -c "
                "أو البناء والربط الكامل فقط؛ لا يوجد تحقق JSON أو تحويل مصدر لها.\n");
        return BAA_COMPILER_EXIT_UNSUPPORTED;
    }
    return BAA_COMPILER_EXIT_SUCCESS;
}

static const char k_driver_nazm_linux_startup[] =
    "; نقطة البدء المستضافة العربية لهدف لينكس\n"
    ".نص\n"
    ".خارجي الرئيسية\n"
    ".خارجي ابدأ_المكتبة_المستضافة\n"
    ".عام الرئيسية_بدء\n"
    "الرئيسية_بدء:\n"
    "    انقل سجل_عام_١٠، مؤشر_المكدس\n"
    "    انقل فهرس_المصدر، [مؤشر_المكدس]\n"
    "    احسب_عنوان سجل_البيانات، [مؤشر_المكدس+٨]\n"
    "    احسب_عنوان فهرس_الوجهة، [مؤشر_التعليمة+الرئيسية]\n"
    "    خالف_بتيا سجل_العداد_٣٢، سجل_العداد_٣٢\n"
    "    خالف_بتيا سجل_عام_٨_٣٢، سجل_عام_٨_٣٢\n"
    "    خالف_بتيا سجل_عام_٩_٣٢، سجل_عام_٩_٣٢\n"
    "    اطرح مؤشر_المكدس، ٨\n"
    "    ادفع سجل_عام_١٠\n"
    "    ناد ابدأ_المكتبة_المستضافة\n"
    "    أوقف\n";

static char *driver_nazm_artifact_path(const char *base, const char *suffix)
{
    if (!base || !suffix) return NULL;
    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);
    char *path = (char *)malloc(base_len + suffix_len + 1u);
    if (!path) return NULL;
    memcpy(path, base, base_len);
    memcpy(path + base_len, suffix, suffix_len + 1u);
    return path;
}

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

static bool driver_nazm_write_text(const char *path, const char *text)
{
    if (!path || !text) return false;
    FILE *out = baa_fopen_utf8(path, "wb");
    if (!out) return false;
    size_t size = strlen(text);
    bool ok = fwrite(text, 1u, size, out) == size;
    if (fclose(out) != 0) ok = false;
    return ok;
}

static const char *driver_nazm_environment_executable(void)
{
#ifdef _WIN32
    /*
     * The narrow CRT environment follows the active code page and corrupts an
     * Arabic executable path before it reaches the UTF-8 process layer.  Read
     * the native UTF-16 environment and convert it explicitly instead.
     */
    static char utf8_value[131072];
    wchar_t wide_value[32768];
    DWORD length = GetEnvironmentVariableW(L"BAA_NAZM", wide_value,
                                            (DWORD)(sizeof(wide_value) /
                                                    sizeof(wide_value[0])));
    if (length == 0 || length >= sizeof(wide_value) / sizeof(wide_value[0]))
        return NULL;
    int converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                        wide_value, -1, utf8_value,
                                        (int)sizeof(utf8_value), NULL, NULL);
    return converted > 0 ? utf8_value : NULL;
#else
    const char *environment = getenv("BAA_NAZM");
    return environment && environment[0] ? environment : NULL;
#endif
}

const char *driver_nazm_get_executable(const CompilerConfig *config)
{
    if (config && config->nazm_executable && config->nazm_executable[0])
        return config->nazm_executable;
    const char *environment = driver_nazm_environment_executable();
    if (environment) return environment;
    return "نظم";
}

#ifdef BAA_EMBEDDED_NAZM
static char *driver_nazm_copy_text(const char *text)
{
    if (!text) return NULL;
    size_t size = strlen(text) + 1u;
    char *copy = (char *)malloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}
#endif

static char *driver_nazm_extract_fingerprint(const char *document)
{
    static const char key[] = "\"fingerprint\":\"";
    if (!document || !strstr(document, "\"schema\":\"nazm-api-v1\""))
        return NULL;
    const char *value = strstr(document, key);
    if (!value) return NULL;
    value += sizeof(key) - 1u;
    const char *end = strchr(value, '"');
    if (!end || end == value) return NULL;
    size_t size = (size_t)(end - value);
    char *fingerprint = (char *)malloc(size + 1u);
    if (!fingerprint) return NULL;
    memcpy(fingerprint, value, size);
    fingerprint[size] = '\0';
    return fingerprint;
}

BaaCompilerExitCode driver_nazm_resolve_fingerprint(CompilerConfig *config)
{
    if (!config) return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    if (config->nazm_fingerprint && config->nazm_fingerprint[0])
        return BAA_COMPILER_EXIT_SUCCESS;

#ifdef BAA_EMBEDDED_NAZM
    if (config->nazm_in_process)
    {
        NazmApiInfo info = nazm_api_info();
        if (info.api_version != NAZM_API_VERSION ||
            !info.api_schema || strcmp(info.api_schema, "nazm-api-v1") != 0 ||
            !info.fingerprint || !info.fingerprint[0])
        {
            fprintf(stderr,
                    "خطأ داخلي: مكتبة نظم المضمنة لا تحقق عقد nazm-api-v1.\n");
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        }
        config->nazm_fingerprint = driver_nazm_copy_text(info.fingerprint);
        return config->nazm_fingerprint
            ? BAA_COMPILER_EXIT_SUCCESS
            : BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }
#endif

    char *info_path = driver_make_temp_artifact_path(
        ".باء", "بصمة-نظم", ".json");
    char *diagnostic_path = driver_make_temp_artifact_path(
        ".باء", "تشخيص-بصمة-نظم", ".txt");
    if (!info_path || !diagnostic_path)
    {
        free(info_path);
        free(diagnostic_path);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    const char *argv[] = {
        driver_nazm_get_executable(config),
        "--معلومات-الواجهة=json",
        NULL,
    };
    BaaProcessResult process = {0};
    bool ran = baa_process_run_redirect(
        argv, NULL, info_path, diagnostic_path, &process);
    if (!ran || !process.started || process.exit_code != 0)
    {
        driver_nazm_replay_diagnostic(diagnostic_path, NULL);
        fprintf(stderr,
                "خطأ: تعذر قراءة بصمة إصدار وقدرات مجمّع نظم '%s'.\n",
                driver_nazm_get_executable(config));
        (void)driver_toolchain_delete_file_utf8(info_path);
        (void)driver_toolchain_delete_file_utf8(diagnostic_path);
        free(info_path);
        free(diagnostic_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    char *document = read_file(info_path);
    config->nazm_fingerprint = driver_nazm_extract_fingerprint(document);
    free(document);
    (void)driver_toolchain_delete_file_utf8(info_path);
    (void)driver_toolchain_delete_file_utf8(diagnostic_path);
    free(info_path);
    free(diagnostic_path);
    if (!config->nazm_fingerprint)
    {
        fprintf(stderr,
                "خطأ: أعاد مجمّع نظم معلومات واجهة بلا بصمة صالحة.\n");
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
    return BAA_COMPILER_EXIT_SUCCESS;
}

#ifdef BAA_EMBEDDED_NAZM
static bool driver_nazm_read_bytes(const char *path,
                                   uint8_t **out_data,
                                   size_t *out_size)
{
    if (out_data) *out_data = NULL;
    if (out_size) *out_size = 0u;
    if (!path || !out_data || !out_size) return false;
    FILE *file = baa_fopen_utf8(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    size_t size = (size_t)length;
    uint8_t *data = (uint8_t *)malloc(size ? size : 1u);
    if (!data)
    {
        fclose(file);
        return false;
    }
    bool ok = !size || fread(data, 1u, size, file) == size;
    if (fclose(file) != 0) ok = false;
    if (!ok)
    {
        free(data);
        return false;
    }
    *out_data = data;
    *out_size = size;
    return true;
}

static void driver_nazm_report_api_diagnostics(const NazmResult *result,
                                               const char *source_map_path)
{
    if (!result) return;
    for (size_t i = 0; i < result->diagnostic_count; ++i)
    {
        const NazmDiagnostic *diagnostic = &result->diagnostics[i];
        fprintf(stderr,
                "خطأ في %s:%d:%d: %s\n",
                diagnostic->file ? diagnostic->file : "",
                diagnostic->line,
                diagnostic->col,
                diagnostic->message ? diagnostic->message : "خطأ نظم");

        char *source_file = NULL;
        unsigned source_line = 0;
        unsigned source_col = 0;
        if (source_map_path && diagnostic->line > 0 &&
            driver_nazm_lookup_source_map(source_map_path,
                                           (unsigned)diagnostic->line,
                                           &source_file,
                                           &source_line,
                                           &source_col))
        {
            fprintf(stderr,
                    "موضع باء الأصلي: %s:%u:%u (من سطر نظم %d).\n",
                    source_file,
                    source_line,
                    source_col,
                    diagnostic->line);
        }
        free(source_file);
    }
}

static BaaCompilerExitCode driver_nazm_run_in_process(
    const CompilerConfig *config,
    const char *source_path,
    const char *source_map_path,
    const char *object_path,
    const char *logical_source_name,
    bool user_source)
{
    if (!config || !source_path || !object_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    uint8_t *source = NULL;
    size_t source_size = 0u;
    if (!driver_nazm_read_bytes(source_path, &source, &source_size))
    {
        fprintf(stderr, "خطأ: تعذرت قراءة مصدر نظم '%s'.\n", source_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    NazmOptions options = nazm_default_options();
    options.format = config->target &&
                     config->target->obj_format == BAA_OBJFORMAT_COFF
        ? NAZM_FORMAT_COFF
        : NAZM_FORMAT_ELF64;
    NazmResult result = nazm_assemble_buffer(
        source,
        source_size,
        logical_source_name && logical_source_name[0]
            ? logical_source_name
            : source_path,
        options);
    free(source);

    if (result.status != NAZM_STATUS_OK)
    {
        driver_nazm_report_api_diagnostics(&result, source_map_path);
        NazmStatus status = result.status;
        nazm_result_free(&result);
        if (status == NAZM_STATUS_OUT_OF_MEMORY ||
            status == NAZM_STATUS_INTERNAL_ERROR ||
            status == NAZM_STATUS_INVALID_ARGUMENT)
            return BAA_COMPILER_EXIT_INTERNAL_ERROR;
        if (status == NAZM_STATUS_SOURCE_ERROR && user_source)
            return BAA_COMPILER_EXIT_SOURCE_ERROR;
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    FILE *object = baa_fopen_utf8(object_path, "wb");
    bool written = object &&
        fwrite(result.object_data, 1u, result.object_size, object) ==
            result.object_size;
    if (object && fclose(object) != 0) written = false;
    nazm_result_free(&result);
    if (!written)
    {
        fprintf(stderr,
                "خطأ: تعذرت كتابة كائن نظم المضمن '%s'.\n",
                object_path);
        (void)driver_toolchain_delete_file_utf8(object_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
    return BAA_COMPILER_EXIT_SUCCESS;
}
#endif

static BaaCompilerExitCode driver_nazm_run_assembler(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    const char *executable,
    const char *source_path,
    const char *source_map_path,
    const char *object_path,
    const char *logical_source_name,
    bool keep_source,
    bool user_source)
{
    if (!config || !executable || !source_path || !object_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

#ifdef BAA_EMBEDDED_NAZM
    if (config->nazm_in_process)
    {
        double started = 0.0;
        if (times && config->time_phases) started = driver_time_seconds();
        BaaCompilerExitCode rc = driver_nazm_run_in_process(
            config,
            source_path,
            source_map_path,
            object_path,
            logical_source_name,
            user_source);
        if (times && config->time_phases)
            times->assemble_s += driver_time_seconds() - started;
        if (rc != BAA_COMPILER_EXIT_SUCCESS)
            (void)driver_toolchain_delete_file_utf8(object_path);
        if (!keep_source)
        {
            (void)driver_toolchain_delete_file_utf8(source_path);
            if (source_map_path)
                (void)driver_toolchain_delete_file_utf8(source_map_path);
        }
        return rc;
    }
#endif

    const char *format = config->target &&
                         config->target->obj_format == BAA_OBJFORMAT_COFF
        ? "كوف"
        : "إلف64";
    char *diagnostic_path =
        driver_nazm_artifact_path(object_path, ".تشخيص-نظم");
    if (!diagnostic_path) return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    const char *argv[10] = {
        executable,
        "-ص",
        format,
        "-خ",
        object_path,
        NULL,
    };
    size_t argv_count = 5;
    if (logical_source_name && logical_source_name[0])
    {
        argv[argv_count++] = "--اسم-المصدر";
        argv[argv_count++] = logical_source_name;
    }
    argv[argv_count++] = source_path;
    argv[argv_count] = NULL;
    double started = 0.0;
    if (times && config->time_phases) started = driver_time_seconds();
    BaaProcessResult process = {0};
    bool process_ok = baa_process_run_redirect(argv,
                                               NULL,
                                               NULL,
                                               diagnostic_path,
                                               &process);
    if (times && config->time_phases)
        times->assemble_s += driver_time_seconds() - started;

    driver_nazm_replay_diagnostic(diagnostic_path, source_map_path);
    (void)driver_toolchain_delete_file_utf8(diagnostic_path);
    free(diagnostic_path);

    if (!process_ok || !process.started || process.exit_code != 0)
    {
        fprintf(stderr,
                "خطأ: فشل مجمّع نظم '%s'%s%d.\n",
                executable,
                process.started ? " برمز خروج " : " قبل بدء العملية؛ الرمز ",
                process.exit_code);
        (void)driver_toolchain_delete_file_utf8(object_path);
        if (!keep_source)
        {
            (void)driver_toolchain_delete_file_utf8(source_path);
            if (source_map_path)
                (void)driver_toolchain_delete_file_utf8(source_map_path);
        }
        if (user_source && process_ok && process.started &&
            process.exit_code == 1)
            return BAA_COMPILER_EXIT_SOURCE_ERROR;
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    if (!keep_source)
    {
        (void)driver_toolchain_delete_file_utf8(source_path);
        if (source_map_path)
            (void)driver_toolchain_delete_file_utf8(source_map_path);
    }
    return BAA_COMPILER_EXIT_SUCCESS;
}

BaaCompilerExitCode driver_emit_nazm_source(const CompilerConfig *config,
                                            MachineModule *module,
                                            const char *output_path)
{
    if (!config || !module || !output_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    if (config->codegen_opts.stack_protector != BAA_STACKPROT_OFF)
    {
        driver_nazm_report_unsupported(
            "مسار نظم لا ينتج حماية المكدس المكافئة بعد.",
            "حماية_المكدس",
            NULL,
            NULL,
            0,
            0);
        return BAA_COMPILER_EXIT_UNSUPPORTED;
    }

    char *source_map_path = driver_nazm_artifact_path(output_path,
                                                       ".خريطة-باء.json");
    FILE *out = baa_fopen_utf8(output_path, "wb");
    FILE *source_map = source_map_path
        ? baa_fopen_utf8(source_map_path, "wb")
        : NULL;
    if (!out || !source_map)
    {
        fprintf(stderr, "خطأ: تعذرت كتابة مصدر نظم '%s'.\n", output_path);
        if (out) fclose(out);
        if (source_map) fclose(source_map);
        (void)driver_toolchain_delete_file_utf8(output_path);
        if (source_map_path)
            (void)driver_toolchain_delete_file_utf8(source_map_path);
        free(source_map_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    BaaNazmEmitResult result = emit_nazm_module_with_source_map(module,
                                                                out,
                                                                source_map,
                                                                output_path,
                                                                config->target,
                                                                config->debug_info);
    int close_result = fclose(out);
    int close_map_result = fclose(source_map);
    if (result.status == BAA_NAZM_EMIT_OK &&
        close_result == 0 && close_map_result == 0)
    {
        free(source_map_path);
        return BAA_COMPILER_EXIT_SUCCESS;
    }

    (void)driver_toolchain_delete_file_utf8(output_path);
    (void)driver_toolchain_delete_file_utf8(source_map_path);
    free(source_map_path);
    if (result.status == BAA_NAZM_EMIT_UNSUPPORTED)
    {
        driver_nazm_report_unsupported(result.reason,
                                       result.blocker_kind,
                                       result.blocker_detail,
                                       result.source_file,
                                       result.source_line,
                                       result.source_col);
        return BAA_COMPILER_EXIT_UNSUPPORTED;
    }

    fprintf(stderr, "خطأ: فشلت كتابة مصدر نظم '%s'.\n", output_path);
    return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
}

BaaCompilerExitCode driver_assemble_nazm_module(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    MachineModule *module,
    const char *source_path,
    const char *object_path,
    bool keep_source)
{
    if (!config || !module || !source_path || !object_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    (void)driver_toolchain_delete_file_utf8(object_path);
    double started = 0.0;
    if (times && config->time_phases) started = driver_time_seconds();
    BaaCompilerExitCode emit_rc =
        driver_emit_nazm_source(config, module, source_path);
    if (times && config->time_phases)
        times->emit_s += driver_time_seconds() - started;
    if (emit_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        (void)driver_toolchain_delete_file_utf8(object_path);
        return emit_rc;
    }

    char *source_map_path =
        driver_nazm_artifact_path(source_path, ".خريطة-باء.json");
    if (!source_map_path)
    {
        (void)driver_toolchain_delete_file_utf8(source_path);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    BaaCompilerExitCode rc = driver_nazm_run_assembler(
        config,
        times,
        driver_nazm_get_executable(config),
        source_path,
        source_map_path,
        object_path,
        "باء-مولد.نظم",
        keep_source,
        false);
    free(source_map_path);
    return rc;
}

BaaCompilerExitCode driver_assemble_nazm_startup(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    const char *source_path,
    const char *object_path,
    bool keep_source)
{
    if (!config || !source_path || !object_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    if (!config->target || config->target->obj_format != BAA_OBJFORMAT_ELF)
        return BAA_COMPILER_EXIT_UNSUPPORTED;
    (void)driver_toolchain_delete_file_utf8(object_path);
    if (!driver_nazm_write_text(source_path, k_driver_nazm_linux_startup))
    {
        fprintf(stderr,
                "خطأ: تعذرت كتابة مصدر بدء التشغيل لنظم '%s'.\n",
                source_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }
    return driver_nazm_run_assembler(
        config,
        times,
        driver_nazm_get_executable(config),
        source_path,
        NULL,
        object_path,
        "بدء-باء.نظم",
        keep_source,
        false);
}

BaaCompilerExitCode driver_compile_nazm_input(
    const CompilerConfig *config,
    int input_count,
    const char *source_path,
    CompilerPhaseTimes *times,
    DriverBuildManifest *build_manifest,
    char **out_object_path)
{
    static unsigned long input_counter = 0;
    if (out_object_path) *out_object_path = NULL;
    if (!config || input_count <= 0 || !source_path || !times ||
        !out_object_path || !driver_nazm_is_source_path(source_path))
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    char *object_path = NULL;
    bool object_path_owned = true;
    if (config->compile_only)
    {
        if (input_count == 1 && config->output_file)
        {
            object_path = config->output_file;
            object_path_owned = false;
        }
        else
        {
            size_t path_len = strlen(source_path);
            size_t suffix_len = strlen(".نظم");
            size_t object_len = path_len - suffix_len + strlen(".o");
            object_path = (char *)malloc(object_len + 1u);
            if (object_path)
            {
                memcpy(object_path, source_path, path_len - suffix_len);
                memcpy(object_path + path_len - suffix_len,
                       ".o",
                       strlen(".o") + 1u);
            }
        }
    }
    else
    {
        char suffix[96];
        int length = snprintf(suffix,
                              sizeof(suffix),
                              ".وحدة-نظم-%lu.o",
                              ++input_counter);
        if (length > 0 && (size_t)length < sizeof(suffix))
        {
            object_path = config->output_file
                ? driver_nazm_artifact_path(config->output_file, suffix)
                : driver_nazm_artifact_path(".باء", suffix);
        }
    }

    if (!object_path)
    {
        fprintf(stderr,
                "خطأ: تعذر تحديد مسار كائن مصدر نظم '%s'.\n",
                source_path);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    if (driver_build_try_reuse_object(config,
                                      source_path,
                                      object_path,
                                      build_manifest))
    {
        if (config->verbose)
            printf("[INFO] Reused direct Nazm object: %s -> %s\n",
                   source_path,
                   object_path);
        *out_object_path = object_path;
        return BAA_COMPILER_EXIT_SUCCESS;
    }

    (void)driver_toolchain_delete_file_utf8(object_path);
    BaaCompilerExitCode rc = driver_nazm_run_assembler(
        config,
        times,
        driver_nazm_get_executable(config),
        source_path,
        NULL,
        object_path,
        NULL,
        true,
        true);
    if (rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        if (object_path_owned) free(object_path);
        return rc;
    }

    if (!driver_build_update_cache(config,
                                   source_path,
                                   object_path,
                                   NULL,
                                   0u,
                                   build_manifest))
    {
        fprintf(stderr,
                "خطأ: فشل تسجيل مصدر نظم المباشر في بيان البناء.\n");
        (void)driver_toolchain_delete_file_utf8(object_path);
        if (object_path_owned) free(object_path);
        return BAA_COMPILER_EXIT_TOOLCHAIN_ERROR;
    }

    if (config->verbose)
        printf("[INFO] Assembled direct Nazm source: %s -> %s\n",
               source_path,
               object_path);
    *out_object_path = object_path;
    return BAA_COMPILER_EXIT_SUCCESS;
}

BaaCompilerExitCode driver_emit_nazm_shadow_object(const CompilerConfig *config,
                                                   MachineModule *module,
                                                   char **out_object_path)
{
    if (out_object_path) *out_object_path = NULL;
    if (!config || !module || !config->nazm_shadow_executable ||
        !config->output_file || !out_object_path)
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;

    const char *object_suffix = config->target &&
                                config->target->obj_format == BAA_OBJFORMAT_COFF
        ? ".ظل-نظم.obj"
        : ".ظل-نظم.o";
    char *source_path = driver_nazm_artifact_path(config->output_file,
                                                  ".ظل-نظم.نظم");
    char *object_path = driver_nazm_artifact_path(config->output_file,
                                                  object_suffix);
    char *source_map_path = source_path
        ? driver_nazm_artifact_path(source_path, ".خريطة-باء.json")
        : NULL;
    if (!source_path || !object_path || !source_map_path)
    {
        free(source_path);
        free(object_path);
        free(source_map_path);
        return BAA_COMPILER_EXIT_INTERNAL_ERROR;
    }

    BaaCompilerExitCode emit_rc =
        driver_emit_nazm_source(config, module, source_path);
    if (emit_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        free(source_path);
        free(object_path);
        free(source_map_path);
        return emit_rc;
    }

    BaaCompilerExitCode assemble_rc = driver_nazm_run_assembler(
        config,
        NULL,
        config->nazm_shadow_executable,
        source_path,
        source_map_path,
        object_path,
        "باء-مولد.نظم",
        true,
        false);
    if (assemble_rc != BAA_COMPILER_EXIT_SUCCESS)
    {
        (void)driver_toolchain_delete_file_utf8(source_path);
        (void)driver_toolchain_delete_file_utf8(source_map_path);
        (void)driver_toolchain_delete_file_utf8(object_path);
        free(source_path);
        free(object_path);
        free(source_map_path);
        return assemble_rc;
    }

    if (config->verbose)
    {
        printf("[INFO] Generated Nazm shadow source: %s\n", source_path);
        printf("[INFO] Generated Nazm shadow object: %s\n", object_path);
    }

    free(source_path);
    free(source_map_path);
    *out_object_path = object_path;
    return BAA_COMPILER_EXIT_SUCCESS;
}
