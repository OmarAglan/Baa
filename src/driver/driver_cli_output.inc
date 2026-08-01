void driver_print_help(void)
{
    printf("Baa Compiler (baa) - The Arabic Programming Language\n");
    printf("Usage: baa [options] <files.baa|files.نظم>...\n");
    printf("\nOptions:\n");
    printf("  -o <file>    Specify output filename\n");
    printf("  -I <dir>     Add include search directory (can be repeated)\n");
    printf("  -I<dir>      Add include search directory (compact form)\n");
    printf("  -S, -s       Compile to assembly only (.s)\n");
    printf("  --emit-nazm  Emit canonical Arabic Nazm source only (.نظم)\n");
    printf("  --assembler=gas|nazm  Select the normal assembler (default: nazm)\n");
    printf("  --nazm-path=<path>  Nazm executable; otherwise BAA_NAZM or نظم from PATH\n");
    printf("  --نظم-داخل-العملية  Use linked nazm-api-v1 (opt-in build only)\n");
    printf("  --nazm-shadow=<path>  Compare explicit GAS rollback with a Nazm shadow beside it\n");
    printf("  -c           Compile to object file only (.o)\n");
    printf("  --check      Parse/analyze source files without emitting code\n");
    printf("  --check-header  Parse/analyze header declarations without emitting code\n");
    printf("  --source-stdin=<file>  Read one tooling source from stdin using this logical path\n");
    printf("  --diagnostics=json  Emit machine-readable diagnostics JSON to stdout\n");
    printf("  --dump-tokens=json  Emit tokens-json-v1 source tokens without preprocessing\n");
    printf("  --dump-structure=json  Emit tolerant structure-json-v1 editor ranges\n");
    printf("  --dump-symbols=json  Emit symbols-json-v1 document declarations and stop after analysis\n");
    printf("  --semantic-query=json --position-byte=N  Emit compiler-owned semantic navigation data\n");
    printf("  --semantic-index=json  Emit compiler-owned symbol identities and occurrences\n");
    printf("  --completion-data=json  Emit completion-data-json-v1 language metadata\n");
    printf("  --format=json  Emit compiler-owned format-json-v1 source formatting\n");
    printf("  -v           Enable verbose output with timing\n");
    printf("  --startup=custom  Emit the default Arabic startup stub in -S output\n");
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
    printf("  baa main.baa helper.نظم -o app.exe\n");
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
    printf("        \"nazm_source\": true,\n");
    printf("        \"structured_arch_ops\": true,\n");
    printf("        \"inline_asm\": false\n");
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
