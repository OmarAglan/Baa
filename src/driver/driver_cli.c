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
    printf("  --check      Parse/analyze source files without emitting code\n");
    printf("  --check-header  Parse/analyze header declarations without emitting code\n");
    printf("  --diagnostics=json  Emit machine-readable diagnostics JSON to stdout\n");
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
    printf("  -fruntime-checks     Enable all optional runtime safety checks\n");
    printf("  -fruntime-checks=<list>  Enable selected checks: all, bounds, null, div-zero, shift, none\n");
    printf("  -fno-runtime-checks  Disable optional runtime safety checks (default)\n");
    printf("  -O0            Disable optimization\n");
    printf("  -O1            Basic optimization (default)\n");
    printf("  -O2            Full optimization (+ CSE)\n");
    printf("  -funroll-loops  Unroll small constant-count loops (after Out-of-SSA)\n");
    printf("  --target=<t>    Target: x86_64-windows | x86_64-linux\n");
    printf("  --target-info=json  Print stable host/target capabilities as JSON\n");
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
    printf("  --explain <CODE>  Explain a stable diagnostic code in Arabic\n");
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

static const char *target_object_format_name(const BaaTarget *target)
{
    if (!target) return "unknown";
    return target->obj_format == BAA_OBJFORMAT_COFF ? "coff" : "elf";
}

static void print_target_record_json(const BaaTarget *target,
                                     const BaaTarget *host,
                                     bool trailing_comma)
{
    bool is_host = target && host && target->kind == host->kind;
    bool is_linux = baa_target_is_linux(target);

    printf("    {\n");
    printf("      \"name\": \"%s\",\n", target->name);
    printf("      \"triple\": \"%s\",\n", target->triple);
    printf("      \"object_format\": \"%s\",\n", target_object_format_name(target));
    printf("      \"executable_suffix\": \"%s\",\n", target->default_exe_ext);
    printf("      \"is_host\": %s,\n", is_host ? "true" : "false");
    printf("      \"capabilities\": {\n");
    printf("        \"assembly\": true,\n");
    printf("        \"object\": %s,\n", is_host ? "true" : "false");
    printf("        \"link\": %s,\n", is_host ? "true" : "false");
    printf("        \"libc\": true,\n");
    printf("        \"stdlib\": true,\n");
    printf("        \"pic\": %s,\n", is_linux ? "true" : "false");
    printf("        \"pie\": %s,\n", is_linux ? "true" : "false");
    printf("        \"stack_protector\": %s,\n", is_linux ? "true" : "false");
    printf("        \"inline_asm\": true\n");
    printf("      }\n");
    printf("    }%s\n", trailing_comma ? "," : "");
}

void driver_print_target_info_json(const BaaTarget *selected_target)
{
    const BaaTarget *host = baa_target_host_default();
    const BaaTarget *selected = selected_target ? selected_target : host;
    const BaaTarget *windows_target = baa_target_builtin_windows_x86_64();
    const BaaTarget *linux_target = baa_target_builtin_linux_x86_64();

    printf("{\n");
    printf("  \"schema_version\": \"target-info-v1\",\n");
    printf("  \"compiler_version\": \"%s\",\n", BAA_VERSION);
    printf("  \"host_target\": \"%s\",\n", host->name);
    printf("  \"selected_target\": \"%s\",\n", selected->name);
    printf("  \"targets\": [\n");
    print_target_record_json(windows_target, host, true);
    print_target_record_json(linux_target, host, false);
    printf("  ]\n");
    printf("}\n");
}

typedef struct
{
    const char *code;
    const char *category;
    const char *title;
    const char *explanation;
    const char *advice;
} DiagnosticExplainEntry;

static const DiagnosticExplainEntry g_diagnostic_explain_entries[] = {
    {
        "B0001",
        "syntax",
        "خطأ نحوي أو لفظي عام",
        "حدث الخطأ أثناء قراءة الوحدات أو بناء شجرة البرنامج. غالباً يعني ذلك رمزاً مفقوداً أو ترتيباً غير صالح.",
        "راجع الموضع المشار إليه وسطر المساعدة إن وُجد؛ أكثر الحالات شيوعاً هي نسيان '.' أو '؛' أو قوس إغلاق."
    },
    {
        "B1000",
        "semantic",
        "خطأ دلالي عام",
        "البرنامج مفهوم نحوياً، لكن التحليل الدلالي رفضه بسبب نوع أو نطاق أو قاعدة لغة غير مستوفاة.",
        "طابق الأنواع، عرّف الرموز قبل استخدامها، واتبع سطر 'مساعدة:' عندما يظهر."
    },
    {
        "B1100",
        "warning",
        "تحذير متغير غير مستخدم",
        "تم تعريف متغير أو رمز عام ولم يُستخدم في المسار المحلل.",
        "استخدم المتغير أو احذف التعريف إن لم يكن مطلوباً."
    },
    {
        "B1101",
        "warning",
        "تحذير كود غير قابل للوصول",
        "توجد تعليمات بعد جملة تنهي المسار مثل 'إرجع' أو 'توقف' أو 'استمر'.",
        "انقل التعليمات قبل نهاية المسار أو احذفها إذا كانت ميتة فعلاً."
    },
    {
        "B1102",
        "warning",
        "تحذير إرجاع ضمني",
        "قد تنتهي دالة دون جملة إرجاع صريحة.",
        "أضف 'إرجع' مناسباً لكل مسار منطقي في الدالة."
    },
    {
        "B1103",
        "warning",
        "تحذير حجب اسم",
        "متغير محلي يستخدم اسماً يحجب رمزاً من نطاق أوسع.",
        "غيّر اسم المتغير المحلي أو استخدم نطاقاً أوضح لتجنب الالتباس."
    },
    {
        "B1104",
        "warning",
        "تحذير تضييق ضمني",
        "قد يؤدي التحويل الضمني بين الأعداد إلى فقدان بيانات.",
        "استخدم نوعاً أوسع أو تحويلاً صريحاً إذا كان التضييق مقصوداً."
    },
    {
        "B1105",
        "warning",
        "تحذير مقارنة موقّع/غير موقّع",
        "المقارنة تمزج بين نوع عددي موقّع وآخر غير موقّع.",
        "وحّد إشارة النوعين أو استخدم تحويلاً صريحاً ومدروساً."
    },
};

bool driver_print_diagnostic_explain(const char *code)
{
    if (!code || !code[0])
    {
        fprintf(stderr, "خطأ: --explain يتطلب رمز تشخيص مثل B1000.\n");
        return false;
    }

    size_t count = sizeof(g_diagnostic_explain_entries) / sizeof(g_diagnostic_explain_entries[0]);
    for (size_t i = 0; i < count; i++)
    {
        const DiagnosticExplainEntry *entry = &g_diagnostic_explain_entries[i];
        if (strcmp(entry->code, code) == 0)
        {
            printf("الرمز: %s\n", entry->code);
            printf("الفئة: %s\n", entry->category);
            printf("العنوان: %s\n", entry->title);
            printf("الشرح: %s\n", entry->explanation);
            printf("اقتراح: %s\n", entry->advice);
            return true;
        }
    }

    fprintf(stderr, "خطأ: لا يوجد شرح معروف لرمز التشخيص '%s'.\n", code);
    return false;
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
            else if (strcmp(arg, "--check") == 0)
                config->check_only = true;
            else if (strcmp(arg, "--check-header") == 0)
                config->header_check = true;
            else if (strcmp(arg, "--diagnostics=json") == 0)
                config->diagnostics_json = true;
            else if (strncmp(arg, "--diagnostics=", 14) == 0)
            {
                fprintf(stderr, "Error: Unsupported diagnostics format '%s' (expected json)\n", arg + 14);
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

    parse_set_result(out,
                     config,
                     target_info_requested ? DRIVER_CMD_TARGET_INFO : DRIVER_CMD_COMPILE,
                     inputs,
                     input_count,
                     include_dirs,
                     include_dir_count);
    return true;
}
