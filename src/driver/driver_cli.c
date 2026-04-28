/**
 * @file driver_cli.c
 * @brief تحليل معاملات سطر الأوامر وطباعة المساعدة/الإصدار.
 * @version 0.3.4
 */

#include "driver_internal.h"

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
    config->include_dirs = include_dirs;
    config->include_dir_count = include_dir_count;
}

void driver_print_help(void)
{
    printf("Baa Compiler (baa) - The Arabic Programming Language\n");
    printf("Usage: baa [options] <files>...\n");
    printf("\nOptions:\n");
    printf("  -o <file>    Specify output filename\n");
    printf("  -I <dir>     Add include search directory (can be repeated)\n");
    printf("  -I<dir>      Add include search directory (compact form)\n");
    printf("  -S, -s       Compile to assembly only (.s)\n");
    printf("  -c           Compile to object file only (.o)\n");
    printf("  -v           Enable verbose output with timing\n");
    printf("  --startup=custom  Use custom entrypoint (__baa_start) while keeping CRT/libc init\n");
    printf("  --dump-ir    Dump Baa IR (Arabic) to stdout after analysis\n");
    printf("  --emit-ir    Write Baa IR (Arabic) to <input>.ir after analysis\n");
    printf("  --dump-ir-opt  Dump Baa IR (Arabic) after optimization\n");
    printf("  --verify       Run all verifiers (--verify-ir + --verify-ssa; requires -O1/-O2)\n");
    printf("  --verify-ir    Verify IR well-formedness (operands/types/terminators/phi/calls)\n");
    printf("  --verify-ssa   Verify SSA invariants after Mem2Reg (requires -O1/-O2)\n");
    printf("  --verify-gate  Debug: run verify-ir/verify-ssa after each optimizer iteration\n");
    printf("  --time-phases  Print per-phase timing/memory stats\n");
    printf("  --emit-build-manifest <file>  Write deterministic build dependency manifest\n");
    printf("  --incremental   Reuse cached object files when dependency hashes match\n");
    printf("  --cache-dir <dir>  Override incremental cache directory (default: .baa_build/cache)\n");
    printf("  --debug-info   Emit debug line info (.file/.loc) and pass -g to toolchain\n");
    printf("  --asm-comments  Emit explanatory comments in generated assembly\n");
    printf("  -O0            Disable optimization\n");
    printf("  -O1            Basic optimization (default)\n");
    printf("  -O2            Full optimization (+ CSE)\n");
    printf("  -funroll-loops  Unroll small constant-count loops (after Out-of-SSA)\n");
    printf("  --target=<t>    Target: x86_64-windows | x86_64-linux\n");
    printf("  -fPIC           Emit PIC-friendly code (ELF/Linux)\n");
    printf("  -fPIE           Build as PIE (ELF/Linux; adds -pie at link)\n");
    printf("  -fno-pic        Disable PIC\n");
    printf("  -fno-pie        Disable PIE\n");
    printf("  -mcmodel=small  Code model (only small supported)\n");
    printf("  -fstack-protector       Enable stack canary (ELF/Linux)\n");
    printf("  -fstack-protector-all   Enable canary for all functions (ELF/Linux)\n");
    printf("  -fno-stack-protector    Disable stack canary\n");
    printf("  --help, -h   Show this help message\n");
    printf("  --version    Show version info\n");
    printf("\nWarning Options:\n");
    printf("  -Wall              Enable all warnings\n");
    printf("  -Werror            Treat warnings as errors\n");
    printf("  -Wunused-variable  Warn about unused variables\n");
    printf("  -Wdead-code        Warn about unreachable code\n");
    printf("  -Wimplicit-narrowing  Warn on potentially lossy implicit numeric conversions\n");
    printf("  -Wsigned-unsigned-compare  Warn on signed/unsigned mixed comparisons\n");
    printf("  -Wno-<warning>     Disable specific warning\n");
    printf("  -Wcolor            Force colored output\n");
    printf("  -Wno-color         Disable colored output\n");
    printf("\nCommands:\n");
    printf("  update       Update compiler to the latest version\n");
    printf("\nExamples:\n");
    printf("  baa main.baa\n");
    printf("  baa main.baa lib.baa -o app.exe\n");
    printf("  baa -Wall -Werror main.baa\n");
    printf("  baa -S main.baa\n");
}

void driver_print_version(void)
{
    printf("baa version %s\n", BAA_VERSION);
    printf("Built on %s\n", BAA_BUILD_DATE);
}

void driver_parse_result_free(DriverParseResult *r)
{
    if (!r) return;
    free(r->input_files);
    free((void *)r->include_dirs);
    r->input_files = NULL;
    r->input_count = 0;
    r->include_dirs = NULL;
    r->include_dir_count = 0;
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

    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if (!arg) continue;

        if (arg[0] == '-')
        {
            if (strcmp(arg, "-S") == 0 || strcmp(arg, "-s") == 0)
                config->assembly_only = true;
            else if (strcmp(arg, "-c") == 0)
                config->compile_only = true;
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

    parse_set_result(out,
                     config,
                     DRIVER_CMD_COMPILE,
                     inputs,
                     input_count,
                     include_dirs,
                     include_dir_count);
    return true;
}
