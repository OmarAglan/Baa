/**
 * @file driver_cli.c
 * @brief تحليل معاملات سطر الأوامر وطباعة المساعدة/الإصدار.
 * @version 0.3.4
 */

#include "driver_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    WARN_PARSE_SET_ALL,
    WARN_PARSE_SET_AS_ERRORS,
    WARN_PARSE_SET_COLOR,
    WARN_PARSE_SET_SPECIFIC,
} WarnParseAction;

typedef struct
{
    const char* flag;
    WarnParseAction action;
    WarningType warning_type;
    bool value;
} WarnFlagSpec;

static const WarnFlagSpec k_warn_flag_specs[] = {
    {"-Wall", WARN_PARSE_SET_ALL, WARN_UNUSED_VARIABLE, true},
    {"-Werror", WARN_PARSE_SET_AS_ERRORS, WARN_UNUSED_VARIABLE, true},
    {"-Wno-color", WARN_PARSE_SET_COLOR, WARN_UNUSED_VARIABLE, false},
    {"-Wcolor", WARN_PARSE_SET_COLOR, WARN_UNUSED_VARIABLE, true},
    {"-Wunused-variable", WARN_PARSE_SET_SPECIFIC, WARN_UNUSED_VARIABLE, true},
    {"-Wno-unused-variable", WARN_PARSE_SET_SPECIFIC, WARN_UNUSED_VARIABLE, false},
    {"-Wdead-code", WARN_PARSE_SET_SPECIFIC, WARN_DEAD_CODE, true},
    {"-Wno-dead-code", WARN_PARSE_SET_SPECIFIC, WARN_DEAD_CODE, false},
    {"-Wimplicit-narrowing", WARN_PARSE_SET_SPECIFIC, WARN_IMPLICIT_NARROWING, true},
    {"-Wno-implicit-narrowing", WARN_PARSE_SET_SPECIFIC, WARN_IMPLICIT_NARROWING, false},
    {"-Wsigned-unsigned-compare", WARN_PARSE_SET_SPECIFIC, WARN_SIGNED_UNSIGNED_COMPARE, true},
    {"-Wno-signed-unsigned-compare", WARN_PARSE_SET_SPECIFIC, WARN_SIGNED_UNSIGNED_COMPARE, false},
};

static bool runtime_check_token_equals(const char* start, size_t len, const char* expected)
{
    return expected && strlen(expected) == len && strncmp(start, expected, len) == 0;
}

static bool parse_runtime_check_mask(const char* spec, unsigned* out_mask)
{
    if (!spec || !out_mask) return false;
    if (!spec[0]) return false;

    unsigned mask = 0;
    const char* p = spec;
    while (*p) {
        const char* start = p;
        while (*p && *p != ',' && *p != '+') p++;
        size_t len = (size_t)(p - start);
        if (len == 0) return false;

        if (runtime_check_token_equals(start, len, "all")) {
            mask |= BAA_RUNTIME_CHECK_ALL;
        } else if (runtime_check_token_equals(start, len, "none")) {
            mask = 0;
        } else if (runtime_check_token_equals(start, len, "bounds")) {
            mask |= BAA_RUNTIME_CHECK_BOUNDS;
        } else if (runtime_check_token_equals(start, len, "null")) {
            mask |= BAA_RUNTIME_CHECK_NULL;
        } else if (runtime_check_token_equals(start, len, "div-zero") ||
                   runtime_check_token_equals(start, len, "div0") ||
                   runtime_check_token_equals(start, len, "div")) {
            mask |= BAA_RUNTIME_CHECK_DIV_ZERO;
        } else if (runtime_check_token_equals(start, len, "shift")) {
            mask |= BAA_RUNTIME_CHECK_SHIFT;
        } else {
            return false;
        }

        if (*p == ',' || *p == '+') {
            p++;
            if (!*p) return false;
        }
    }

    *out_mask = mask;
    return true;
}

/**
 * @brief تحليل علم تحذير (-W...).
 * @return true إذا تم التعرف على العلم.
 */
static bool parse_warning_flag(const char *flag)
{
    if (!flag) return false;

    for (size_t i = 0; i < sizeof(k_warn_flag_specs) / sizeof(k_warn_flag_specs[0]); i++)
    {
        const WarnFlagSpec* spec = &k_warn_flag_specs[i];
        if (strcmp(flag, spec->flag) != 0) continue;

        switch (spec->action)
        {
            case WARN_PARSE_SET_ALL:
                g_warning_config.all_warnings = spec->value;
                break;
            case WARN_PARSE_SET_AS_ERRORS:
                g_warning_config.warnings_as_errors = spec->value;
                break;
            case WARN_PARSE_SET_COLOR:
                g_warning_config.colored_output = spec->value;
                break;
            case WARN_PARSE_SET_SPECIFIC:
                g_warning_config.enabled[spec->warning_type] = spec->value;
                break;
            default:
                return false;
        }
        return true;
    }

    return false;
}

static void parse_release_temp_arrays(char** inputs, const char** include_dirs)
{
    free(inputs);
    free((void*)include_dirs);
}

static void parse_set_result(DriverParseResult* out,
                             CompilerConfig* config,
                             DriverCommand cmd,
                             char** inputs,
                             int input_count,
                             const char** include_dirs,
                             size_t include_dir_count)
{
    out->cmd = cmd;
    out->input_files = inputs;
    out->input_count = input_count;
    out->include_dirs = include_dirs;
    out->include_dir_count = include_dir_count;
    out->explain_code = NULL;
    config->include_dirs = include_dirs;
    config->include_dir_count = include_dir_count;
}

#include "driver_cli_output.inc"

void driver_parse_result_free(DriverParseResult *r)
{
    if (!r) return;
    free(r->input_files);
    free((void *)r->include_dirs);
    r->input_files = NULL;
    r->input_count = 0;
    r->include_dirs = NULL;
    r->include_dir_count = 0;
    r->explain_code = NULL;
    r->cmd = DRIVER_CMD_COMPILE;
}

bool driver_parse_cli(int argc, char **argv, CompilerConfig *config, DriverParseResult *out)
{
    if (!argv || !config || !out) return false;

    memset(out, 0, sizeof(*out));
    out->cmd = DRIVER_CMD_COMPILE;

    // يجب أن يكون "update" هو المعامل الوحيد
    if (argc == 2 && strcmp(argv[1], "update") == 0)
    {
        out->cmd = DRIVER_CMD_UPDATE;
        return true;
    }

    char **inputs = (char **)calloc((size_t)argc, sizeof(char *));
    if (!inputs)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        return false;
    }
    const char **include_dirs = (const char **)calloc((size_t)argc, sizeof(const char *));
    if (!include_dirs)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
        free(inputs);
        return false;
    }

    int input_count = 0;
    size_t include_dir_count = 0;
    bool target_info_requested = false;
    bool completion_data_requested = false;
    bool structure_dump_requested = false;
    bool format_requested = false;
    bool token_dump_requested = false;

    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if (!arg) continue;

        if (arg[0] == '-')
        {
            if (strcmp(arg, "-S") == 0 || strcmp(arg, "-s") == 0)
                config->assembly_only = true;
            else if (strcmp(arg, "--emit-nazm") == 0)
                config->emit_nazm = true;
            else if (strncmp(arg, "--assembler=", 12) == 0)
            {
                const char *assembler = arg + 12;
                if (strcmp(assembler, "gas") == 0)
                    config->assembler = BAA_ASSEMBLER_GAS;
                else if (strcmp(assembler, "nazm") == 0)
                    config->assembler = BAA_ASSEMBLER_NAZM;
                else
                {
                    fprintf(stderr,
                            "خطأ: قيمة --assembler غير معروفة '%s' "
                            "(المتوقع gas أو nazm).\n",
                            assembler);
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->assembler_explicit = true;
            }
            else if (strncmp(arg, "--nazm-path=", 12) == 0)
            {
                const char *path = arg + 12;
                if (!path[0])
                {
                    fprintf(stderr, "خطأ: --nazm-path يتطلب مسار ملف نظم التنفيذي.\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->nazm_executable = (char *)path;
            }
            else if (strcmp(arg, "--نظم-داخل-العملية") == 0)
                config->nazm_in_process = true;
            else if (strncmp(arg, "--nazm-shadow=", 14) == 0)
            {
                const char *path = arg + 14;
                if (!path[0])
                {
                    fprintf(stderr, "خطأ: --nazm-shadow يتطلب مسار ملف نظم التنفيذي.\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->nazm_shadow_executable = (char *)path;
            }
            else if (strcmp(arg, "-c") == 0)
                config->compile_only = true;
            else if (strcmp(arg, "--check") == 0)
                config->check_only = true;
            else if (strcmp(arg, "--check-header") == 0)
                config->header_check = true;
            else if (strncmp(arg, "--source-stdin=", 15) == 0)
            {
                const char *logical_path = arg + 15;
                if (!logical_path[0])
                {
                    fprintf(stderr, "Error: --source-stdin requires a logical source path\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                if (config->source_stdin_file)
                {
                    fprintf(stderr, "Error: --source-stdin may be specified only once\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->source_stdin_file = logical_path;
            }
            else if (strcmp(arg, "--diagnostics=json") == 0)
                config->diagnostics_json = true;
            else if (strncmp(arg, "--diagnostics=", 14) == 0)
            {
                fprintf(stderr, "Error: Unsupported diagnostics format '%s' (expected json)\n", arg + 14);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "--dump-symbols=json") == 0)
            {
                config->dump_symbols_json = true;
                config->check_only = true;
            }
            else if (strncmp(arg, "--dump-symbols=", 15) == 0)
            {
                fprintf(stderr, "Error: Unsupported symbol format '%s' (expected json)\n", arg + 15);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "--dump-tokens=json") == 0)
                token_dump_requested = true;
            else if (strncmp(arg, "--dump-tokens=", 14) == 0)
            {
                fprintf(stderr,
                        "Error: Unsupported token format '%s' (expected json)\n",
                        arg + 14);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "--dump-structure=json") == 0)
                structure_dump_requested = true;
            else if (strncmp(arg, "--dump-structure=", 17) == 0)
            {
                fprintf(stderr,
                        "Error: Unsupported structure format '%s' (expected json)\n",
                        arg + 17);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "--semantic-query=json") == 0)
            {
                config->semantic_query_json = true;
                config->check_only = true;
            }
            else if (strncmp(arg, "--semantic-query=", 17) == 0)
            {
                fprintf(stderr,
                        "Error: Unsupported semantic query format '%s' (expected json)\n",
                        arg + 17);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "--semantic-index=json") == 0)
            {
                config->semantic_index_json = true;
                config->check_only = true;
            }
            else if (strncmp(arg, "--semantic-index=", 17) == 0)
            {
                fprintf(stderr,
                        "Error: Unsupported semantic index format '%s' (expected json)\n",
                        arg + 17);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strncmp(arg, "--position-byte=", 16) == 0)
            {
                const char* value_text = arg + 16;
                char* end = NULL;
                errno = 0;
                const unsigned long long value = strtoull(value_text, &end, 10);
                if (!value_text[0] || errno == ERANGE || !end || *end != '\0' ||
                    value > (unsigned long long)SIZE_MAX)
                {
                    fprintf(stderr,
                            "Error: --position-byte requires a non-negative byte offset\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->semantic_query_byte = (size_t)value;
                config->semantic_query_byte_set = true;
            }
            else if (strcmp(arg, "--completion-data=json") == 0)
                completion_data_requested = true;
            else if (strncmp(arg, "--completion-data=", 18) == 0)
            {
                fprintf(stderr,
                        "Error: Unsupported completion data format '%s' (expected json)\n",
                        arg + 18);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "--format=json") == 0)
                format_requested = true;
            else if (strncmp(arg, "--format=", 9) == 0)
            {
                fprintf(stderr,
                        "Error: Unsupported source format '%s' (expected json)\n",
                        arg + 9);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "-v") == 0)
                config->verbose = true;
            else if (strcmp(arg, "--startup=custom") == 0)
                config->custom_startup = true;
            else if (strcmp(arg, "--dump-ir") == 0)
                config->dump_ir = true;
            else if (strcmp(arg, "--emit-ir") == 0)
                config->emit_ir = true;
            else if (strcmp(arg, "--dump-ir-opt") == 0)
                config->dump_ir_opt = true;
            else if (strcmp(arg, "--verify") == 0)
            {
                config->verify_ir = true;
                config->verify_ssa = true;
            }
            else if (strcmp(arg, "--verify-ir") == 0)
                config->verify_ir = true;
            else if (strcmp(arg, "--verify-ssa") == 0)
                config->verify_ssa = true;
            else if (strcmp(arg, "--verify-gate") == 0)
                config->verify_gate = true;
            else if (strcmp(arg, "--time-phases") == 0)
                config->time_phases = true;
            else if (strcmp(arg, "--emit-build-manifest") == 0)
            {
                if (i + 1 < argc && argv[i + 1] && argv[i + 1][0])
                    config->build_manifest_file = argv[++i];
                else
                {
                    fprintf(stderr, "Error: --emit-build-manifest requires a filename\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
            }
            else if (strcmp(arg, "--incremental") == 0)
                config->incremental = true;
            else if (strcmp(arg, "--cache-dir") == 0)
            {
                if (i + 1 < argc && argv[i + 1] && argv[i + 1][0])
                    config->cache_dir = argv[++i];
                else
                {
                    fprintf(stderr, "Error: --cache-dir requires a directory path\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
            }
            else if (strcmp(arg, "--debug-info") == 0)
                config->debug_info = true;
            else if (strcmp(arg, "--asm-comments") == 0)
                config->codegen_opts.asm_comments = true;
            else if (strcmp(arg, "-fruntime-checks") == 0)
            {
                config->runtime_checks = true;
                config->runtime_check_mask = BAA_RUNTIME_CHECK_ALL;
            }
            else if (strncmp(arg, "-fruntime-checks=", 17) == 0)
            {
                unsigned mask = 0;
                const char* spec = arg + 17;
                if (!parse_runtime_check_mask(spec, &mask))
                {
                    fprintf(stderr,
                            "خطأ: اختيار فحص وقت التشغيل '%s' غير معروف (المتوقع: all,bounds,null,div-zero,shift,none)\n",
                            spec);
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->runtime_check_mask = mask;
                config->runtime_checks = (mask != 0);
            }
            else if (strcmp(arg, "-fno-runtime-checks") == 0)
            {
                config->runtime_checks = false;
                config->runtime_check_mask = 0;
            }
            else if (strncmp(arg, "--target=", 9) == 0)
            {
                const char *t = arg + 9;
                const BaaTarget *parsed = baa_target_parse(t);
                if (!parsed)
                {
                    fprintf(stderr, "Error: Unknown target '%s'\n", t);
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                config->target = parsed;
            }
            else if (strcmp(arg, "--target-info=json") == 0 ||
                     strcmp(arg, "--print-target-info=json") == 0)
                target_info_requested = true;
            else if (strncmp(arg, "--target-info=", 14) == 0)
            {
                fprintf(stderr, "Error: Unsupported target info format '%s' (expected json)\n", arg + 14);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strcmp(arg, "-fPIC") == 0)
                config->codegen_opts.pic = true;
            else if (strcmp(arg, "-fPIE") == 0)
            {
                config->codegen_opts.pie = true;
                config->codegen_opts.pic = true;
            }
            else if (strcmp(arg, "-fno-pic") == 0)
                config->codegen_opts.pic = false;
            else if (strcmp(arg, "-fno-pie") == 0)
                config->codegen_opts.pie = false;
            else if (strcmp(arg, "-fstack-protector") == 0)
                config->codegen_opts.stack_protector = BAA_STACKPROT_ON;
            else if (strcmp(arg, "-fstack-protector-all") == 0)
                config->codegen_opts.stack_protector = BAA_STACKPROT_ALL;
            else if (strcmp(arg, "-fno-stack-protector") == 0)
                config->codegen_opts.stack_protector = BAA_STACKPROT_OFF;
            else if (strncmp(arg, "-mcmodel=", 9) == 0)
            {
                const char *m = arg + 9;
                if (strcmp(m, "small") == 0)
                    config->codegen_opts.code_model = BAA_CODEMODEL_SMALL;
                else
                {
                    fprintf(stderr,
                            "Error: Unsupported code model '%s' (only small is supported)\n",
                            m);
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
            }
            else if (strcmp(arg, "-O0") == 0)
                config->opt_level = OPT_LEVEL_0;
            else if (strcmp(arg, "-O1") == 0)
                config->opt_level = OPT_LEVEL_1;
            else if (strcmp(arg, "-O2") == 0)
                config->opt_level = OPT_LEVEL_2;
            else if (strcmp(arg, "-funroll-loops") == 0)
                config->funroll_loops = true;
            else if (strcmp(arg, "-o") == 0)
            {
                if (i + 1 < argc)
                    config->output_file = argv[++i];
                else
                {
                    fprintf(stderr, "Error: -o requires a filename\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
            }
            else if (strcmp(arg, "-I") == 0)
            {
                if (i + 1 >= argc || !argv[i + 1] || argv[i + 1][0] == '\0')
                {
                    fprintf(stderr, "Error: -I requires a directory path\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                include_dirs[include_dir_count++] = argv[++i];
            }
            else if (strncmp(arg, "-I", 2) == 0)
            {
                const char *dir = arg + 2;
                if (!dir[0])
                {
                    fprintf(stderr, "Error: -I requires a directory path\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                include_dirs[include_dir_count++] = dir;
            }
            else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
            {
                parse_set_result(out,
                                 config,
                                 DRIVER_CMD_HELP,
                                 inputs,
                                 input_count,
                                 include_dirs,
                                 include_dir_count);
                return true;
            }
            else if (strcmp(arg, "--version") == 0)
            {
                parse_set_result(out,
                                 config,
                                 DRIVER_CMD_VERSION,
                                 inputs,
                                 input_count,
                                 include_dirs,
                                 include_dir_count);
                return true;
            }
            else if (strcmp(arg, "--explain") == 0)
            {
                if (i + 1 < argc && argv[i + 1] && argv[i + 1][0])
                {
                    const char *code = argv[++i];
                    parse_set_result(out,
                                     config,
                                     DRIVER_CMD_EXPLAIN,
                                     inputs,
                                     input_count,
                                     include_dirs,
                                     include_dir_count);
                    out->explain_code = code;
                    return true;
                }
                fprintf(stderr, "Error: --explain requires a diagnostic code\n");
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
            else if (strncmp(arg, "--explain=", 10) == 0)
            {
                const char *code = arg + 10;
                if (!code[0])
                {
                    fprintf(stderr, "Error: --explain requires a diagnostic code\n");
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
                parse_set_result(out,
                                 config,
                                 DRIVER_CMD_EXPLAIN,
                                 inputs,
                                 input_count,
                                 include_dirs,
                                 include_dir_count);
                out->explain_code = code;
                return true;
            }
            else if (strncmp(arg, "-W", 2) == 0)
            {
                if (!parse_warning_flag(arg))
                {
                    fprintf(stderr, "Error: Unknown warning flag '%s'\n", arg);
                    parse_release_temp_arrays(inputs, include_dirs);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "Error: Unknown flag '%s'\n", arg);
                parse_release_temp_arrays(inputs, include_dirs);
                return false;
            }
        }
        else
        {
            inputs[input_count++] = arg;
        }
    }

    if (completion_data_requested &&
        (target_info_requested || format_requested ||
         input_count != 0 || config->source_stdin_file))
    {
        fprintf(stderr,
                "Error: --completion-data=json must be used without sources or target info\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }

    if (config->source_stdin_file)
    {
        if ((!config->check_only && !format_requested &&
             !token_dump_requested && !structure_dump_requested) ||
            config->header_check || input_count != 0 ||
            config->build_manifest_file || config->incremental)
        {
            fprintf(stderr,
                    "Error: --source-stdin requires --check, --format=json, "
                    "--dump-tokens=json, or --dump-structure=json, "
                    "exactly one logical source, "
                    "and no positional input, incremental cache, or build manifest\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        inputs[input_count++] = (char *)config->source_stdin_file;
    }

    const int machine_readable_source_modes =
        (config->diagnostics_json ? 1 : 0) +
        (token_dump_requested ? 1 : 0) +
        (structure_dump_requested ? 1 : 0) +
        (config->dump_symbols_json ? 1 : 0) +
        (config->semantic_query_json ? 1 : 0) +
        (config->semantic_index_json ? 1 : 0) +
        (format_requested ? 1 : 0);
    if (machine_readable_source_modes > 1)
    {
        fprintf(stderr,
                "Error: machine-readable compiler modes produce separate JSON documents\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (token_dump_requested)
    {
        if (target_info_requested || input_count != 1 ||
            config->check_only || config->header_check ||
            config->assembly_only || config->emit_nazm ||
            config->compile_only || config->output_file ||
            config->build_manifest_file || config->incremental)
        {
            fprintf(stderr,
                    "Error: --dump-tokens=json requires exactly one Baa "
                    "source and cannot be combined with compilation modes\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        if (driver_nazm_is_source_path(inputs[0]))
        {
            fprintf(stderr,
                    "Error: --dump-tokens=json accepts Baa sources only\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        config->verbose = false;
    }
    if (structure_dump_requested)
    {
        if (target_info_requested || input_count != 1 ||
            config->check_only || config->header_check ||
            config->assembly_only || config->emit_nazm ||
            config->compile_only || config->output_file ||
            config->build_manifest_file || config->incremental)
        {
            fprintf(stderr,
                    "Error: --dump-structure=json requires exactly one Baa "
                    "source and cannot be combined with compilation modes\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        if (driver_nazm_is_source_path(inputs[0]))
        {
            fprintf(stderr,
                    "Error: --dump-structure=json accepts Baa sources only\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        config->verbose = false;
    }
    if (config->dump_symbols_json && input_count != 1)
    {
        fprintf(stderr, "Error: --dump-symbols=json requires exactly one Baa source\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->dump_symbols_json && driver_nazm_is_source_path(inputs[0]))
    {
        fprintf(stderr, "Error: --dump-symbols=json accepts Baa sources only\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->dump_symbols_json) config->verbose = false;

    if (config->semantic_query_json && !config->semantic_query_byte_set)
    {
        fprintf(stderr,
                "Error: --semantic-query=json requires --position-byte=<offset>\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (!config->semantic_query_json && config->semantic_query_byte_set)
    {
        fprintf(stderr,
                "Error: --position-byte is valid only with --semantic-query=json\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->semantic_query_json && input_count != 1)
    {
        fprintf(stderr,
                "Error: --semantic-query=json requires exactly one Baa source\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->semantic_query_json && driver_nazm_is_source_path(inputs[0]))
    {
        fprintf(stderr,
                "Error: --semantic-query=json accepts Baa sources only\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->semantic_query_json) config->verbose = false;

    if (config->semantic_index_json && input_count != 1)
    {
        fprintf(stderr,
                "Error: --semantic-index=json requires exactly one Baa source\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->semantic_index_json && driver_nazm_is_source_path(inputs[0]))
    {
        fprintf(stderr,
                "Error: --semantic-index=json accepts Baa sources only\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->semantic_index_json) config->verbose = false;

    if (format_requested)
    {
        if (target_info_requested || input_count != 1 ||
            config->check_only || config->header_check ||
            config->assembly_only || config->emit_nazm ||
            config->compile_only || config->output_file ||
            config->build_manifest_file || config->incremental)
        {
            fprintf(stderr,
                    "Error: --format=json requires exactly one Baa source "
                    "and cannot be combined with compilation modes\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        if (driver_nazm_is_source_path(inputs[0]))
        {
            fprintf(stderr, "Error: --format=json accepts Baa sources only\n");
            parse_release_temp_arrays(inputs, include_dirs);
            return false;
        }
        config->verbose = false;
    }

    if (config->emit_nazm &&
        (config->assembly_only || config->compile_only ||
         config->check_only || config->header_check))
    {
        fprintf(stderr, "خطأ: --emit-nazm لا يقبل -S أو -c أو أوضاع الفحص.\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->nazm_shadow_executable &&
        (config->emit_nazm || config->assembly_only || config->compile_only ||
         config->check_only || config->header_check))
    {
        fprintf(stderr, "خطأ: --nazm-shadow يعمل فقط مع بناء وربط تنفيذي كامل.\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->nazm_shadow_executable &&
        config->assembler_explicit &&
        config->assembler == BAA_ASSEMBLER_NAZM)
    {
        fprintf(stderr,
                "خطأ: --nazm-shadow لا يجتمع مع --assembler=nazm؛ "
                "المسار الطبيعي نفسه يستخدم نظم.\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->nazm_shadow_executable && !config->assembler_explicit)
    {
        /*
         * يبقى وضع الظل مقارنة صريحة بين مسار الرجوع GAS ومسار نظم حتى بعد
         * اعتماد نظم افتراضيا. لا يوجد رجوع تلقائي عند فشل الظل.
         */
        config->assembler = BAA_ASSEMBLER_GAS;
    }
    bool has_direct_nazm_input = false;
    for (int i = 0; i < input_count; ++i)
    {
        if (driver_nazm_is_source_path(inputs[i]))
        {
            has_direct_nazm_input = true;
            break;
        }
    }
    if (config->nazm_executable &&
        config->assembler != BAA_ASSEMBLER_NAZM &&
        !has_direct_nazm_input)
    {
        fprintf(stderr,
                "خطأ: --nazm-path يتطلب --assembler=nazm "
                "أو ملف مصدر مباشر بامتداد `.نظم`.\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }
    if (config->nazm_in_process &&
        (config->assembler != BAA_ASSEMBLER_NAZM ||
         config->nazm_shadow_executable || config->nazm_executable))
    {
        fprintf(stderr,
                "خطأ: --نظم-داخل-العملية يتطلب مجمع نظم الطبيعي ولا يجتمع "
                "مع مسار ملف تنفيذي أو وضع الظل.\n");
        parse_release_temp_arrays(inputs, include_dirs);
        return false;
    }

    parse_set_result(out,
                     config,
                     format_requested ? DRIVER_CMD_FORMAT :
                     structure_dump_requested ? DRIVER_CMD_STRUCTURE_DUMP :
                     token_dump_requested ? DRIVER_CMD_TOKEN_DUMP :
                     completion_data_requested ? DRIVER_CMD_COMPLETION_DATA :
                     target_info_requested ? DRIVER_CMD_TARGET_INFO : DRIVER_CMD_COMPILE,
                     inputs,
                     input_count,
                     include_dirs,
                     include_dir_count);
    return true;
}
