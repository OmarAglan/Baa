/**
 * @file main.c
 * @brief نقطة الدخول ومحرك سطر الأوامر (CLI Driver).
 * @version 0.2.9 (Input & UX Polish)
 */

#include "baa.h"
#include "ir_lower.h"
#include "ir_optimizer.h"
#include "isel.h"
#include "regalloc.h"
#include "emit.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// ============================================================================
// إعدادات المترجم (Compiler Configuration)
// ============================================================================

typedef struct {
    char* input_file;       // ملف المصدر (.baa)
    char* output_file;      // ملف الخرج (.exe, .o, .s)
    bool assembly_only;     // -S: إنتاج كود تجميع فقط
    bool compile_only;      // -c: تجميع إلى كائن فقط (بدون ربط)
    bool verbose;           // -v: وضع التفاصيل
    bool show_timings;      // -v: عرض وقت الترجمة
    bool dump_ir;           // --dump-ir: طباعة IR بعد التحليل الدلالي
    bool emit_ir;           // --emit-ir: كتابة IR إلى ملف .ir بجانب المصدر
    bool dump_ir_opt;       // --dump-ir-opt: طباعة IR بعد التحسين
    OptLevel opt_level;     // -O0, -O1, -O2: مستوى التحسين
    double start_time;      // وقت بدء الترجمة
} CompilerConfig;

// ============================================================================
// دوال قياس الوقت (Timing Functions)
// ============================================================================

/**
 * @brief الحصول على الوقت الحالي بدقة عالية (بالثواني).
 */
static double get_time_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

// ============================================================================
// دالات مساعدة (Helper Functions)
// ============================================================================

/**
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 */
char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Error: Could not open input file '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (!buffer) {
        printf("Error: Memory allocation failed\n");
        exit(1);
    }
    fread(buffer, 1, length, f);
    buffer[length] = '\0';
    fclose(f);
    return buffer;
}

/**
 * @brief تغيير امتداد الملف (مثلاً من .b إلى .s).
 */
char* change_extension(const char* filename, const char* new_ext) {
    char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return strdup(new_ext + (dot == filename)); // Fallback

    size_t base_len = dot - filename;
    size_t ext_len = strlen(new_ext);
    char* new_name = malloc(base_len + ext_len + 1);
    strncpy(new_name, filename, base_len);
    strcpy(new_name + base_len, new_ext);
    return new_name;
}

// ============================================================================
// IR Integration (v0.3.0.7)
// ============================================================================

/**
 * @brief تحليل علم تحذير (-W...).
 * @return true إذا تم التعرف على العلم.
 */
static bool parse_warning_flag(const char* flag) {
    // -Wall: تفعيل جميع التحذيرات
    if (strcmp(flag, "-Wall") == 0) {
        g_warning_config.all_warnings = true;
        return true;
    }
    
    // -Werror: معاملة التحذيرات كأخطاء
    if (strcmp(flag, "-Werror") == 0) {
        g_warning_config.warnings_as_errors = true;
        return true;
    }
    
    // -Wno-color: تعطيل الألوان
    if (strcmp(flag, "-Wno-color") == 0) {
        g_warning_config.colored_output = false;
        return true;
    }
    
    // -Wcolor: تفعيل الألوان
    if (strcmp(flag, "-Wcolor") == 0) {
        g_warning_config.colored_output = true;
        return true;
    }
    
    // -Wunused-variable: تفعيل تحذير المتغيرات غير المستخدمة
    if (strcmp(flag, "-Wunused-variable") == 0) {
        g_warning_config.enabled[WARN_UNUSED_VARIABLE] = true;
        return true;
    }
    
    // -Wno-unused-variable: تعطيل تحذير المتغيرات غير المستخدمة
    if (strcmp(flag, "-Wno-unused-variable") == 0) {
        g_warning_config.enabled[WARN_UNUSED_VARIABLE] = false;
        return true;
    }
    
    // -Wdead-code: تفعيل تحذير الكود الميت
    if (strcmp(flag, "-Wdead-code") == 0) {
        g_warning_config.enabled[WARN_DEAD_CODE] = true;
        return true;
    }
    
    // -Wno-dead-code: تعطيل تحذير الكود الميت
    if (strcmp(flag, "-Wno-dead-code") == 0) {
        g_warning_config.enabled[WARN_DEAD_CODE] = false;
        return true;
    }
    
    return false;
}

/**
 * @brief طباعة رسالة المساعدة.
 */
void print_help() {
    printf("Baa Compiler (baa) - The Arabic Programming Language\n");
    printf("Usage: baa [options] <files>...\n");
    printf("\nOptions:\n");
    printf("  -o <file>    Specify output filename\n");
    printf("  -S, -s       Compile to assembly only (.s)\n");
    printf("  -c           Compile to object file only (.o)\n");
    printf("  -v           Enable verbose output with timing\n");
    printf("  --dump-ir    Dump Baa IR (Arabic) to stdout after analysis\n");
    printf("  --emit-ir    Write Baa IR (Arabic) to <input>.ir after analysis\n");
    printf("  --dump-ir-opt  Dump Baa IR (Arabic) after optimization\n");
    printf("  -O0            Disable optimization\n");
    printf("  -O1            Basic optimization (default)\n");
    printf("  -O2            Full optimization (+ CSE)\n");
    printf("  --help, -h   Show this help message\n");
    printf("  --version    Show version info\n");
    printf("\nWarning Options:\n");
    printf("  -Wall              Enable all warnings\n");
    printf("  -Werror            Treat warnings as errors\n");
    printf("  -Wunused-variable  Warn about unused variables\n");
    printf("  -Wdead-code        Warn about unreachable code\n");
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

/**
 * @brief طباعة رقم الإصدار.
 */
void print_version() {
    printf("baa version %s\n", BAA_VERSION);
    printf("Built on %s\n", BAA_BUILD_DATE);
}

// ============================================================================
// نقطة الدخول (Main Entry Point)
// ============================================================================

int main(int argc, char** argv) {
    CompilerConfig config = {0};
    config.output_file = NULL;
    config.opt_level = OPT_LEVEL_1;  // Default optimization level

    char* input_files[32];  // دعم حتى 32 ملف مصدر
    int input_count = 0;
    
    // تهيئة نظام التحذيرات
    warning_init();
    
    // تسجيل وقت البدء
    config.start_time = get_time_seconds();
    
    // 0. التحقق من وجود معاملات
    if (argc < 2) {
        print_help();
        return 1;
    }

    // 1. التحقق من أمر التحديث (Update Command)
    // يجب أن يكون "update" هو المعامل الوحيد
    if (argc == 2 && strcmp(argv[1], "update") == 0) {
        run_updater();
        return 0;
    }
    
    // 2. تحليل المعاملات (Argument Parsing)
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg[0] == '-') {
            if (strcmp(arg, "-S") == 0 || strcmp(arg, "-s") == 0) config.assembly_only = true;
            else if (strcmp(arg, "-c") == 0) config.compile_only = true;
            else if (strcmp(arg, "-v") == 0) {
                config.verbose = true;
                config.show_timings = true;
            }
            else if (strcmp(arg, "--dump-ir") == 0) {
                config.dump_ir = true;
            }
            else if (strcmp(arg, "--emit-ir") == 0) {
                config.emit_ir = true;
            }
            else if (strcmp(arg, "--dump-ir-opt") == 0) {
                config.dump_ir_opt = true;
            }
            else if (strcmp(arg, "-O0") == 0) {
                config.opt_level = OPT_LEVEL_0;
            }
            else if (strcmp(arg, "-O1") == 0) {
                config.opt_level = OPT_LEVEL_1;
            }
            else if (strcmp(arg, "-O2") == 0) {
                config.opt_level = OPT_LEVEL_2;
            }
            else if (strcmp(arg, "-o") == 0) {
                if (i + 1 < argc) config.output_file = argv[++i];
                else { printf("Error: -o requires a filename\n"); return 1; }
            }
            else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
                print_help();
                return 0;
            }
            else if (strcmp(arg, "--version") == 0) {
                print_version();
                return 0;
            }
            // أعلام التحذيرات (-W...)
            else if (strncmp(arg, "-W", 2) == 0) {
                if (!parse_warning_flag(arg)) {
                    printf("Error: Unknown warning flag '%s'\n", arg);
                    return 1;
                }
            }
            else {
                printf("Error: Unknown flag '%s'\n", arg);
                return 1;
            }
        } else {
            if (input_count < 32) {
                input_files[input_count++] = arg;
            } else {
                printf("Error: Too many input files (Max 32)\n");
                return 1;
            }
        }
    }

    if (input_count == 0) {
        printf("Error: No input file specified\n");
        return 1;
    }

    // تحديد اسم الملف المخرج الافتراضي
    if (!config.output_file) {
        if (config.assembly_only) config.output_file = NULL; // سيتم تحديده لكل ملف
        else if (config.compile_only) config.output_file = NULL; // سيتم تحديده لكل ملف
        else config.output_file = strdup("out.exe");
    }

    // قائمة ملفات الكائنات للربط
    char* obj_files_to_link[32];
    int obj_count = 0;

    // --- حلقة المعالجة لكل ملف ---
    for (int i = 0; i < input_count; i++) {
        char* current_input = input_files[i];
        
        if (config.verbose) printf("\n[INFO] Processing %s (%d/%d)...\n", current_input, i+1, input_count);

        // 1. قراءة الملف
        char* source = read_file(current_input);
        
        // 2. التحليل اللفظي والقواعدي
        Lexer lexer;
        lexer_init(&lexer, source, current_input);
        Node* ast = parse(&lexer);

        if (error_has_occurred()) {
            fprintf(stderr, "Aborting %s due to syntax errors.\n", current_input);
            free(source);
            return 1;
        }

        // 3. التحليل الدلالي
        if (config.verbose) printf("[INFO] Running semantic analysis...\n");
        if (!analyze(ast)) {
            fprintf(stderr, "Aborting %s due to semantic errors.\n", current_input);
            free(source);
            return 1;
        }
        
        // التحقق من التحذيرات كأخطاء
        if (g_warning_config.warnings_as_errors && warning_has_occurred()) {
            fprintf(stderr, "Aborting %s: warnings treated as errors (-Werror).\n", current_input);
            free(source);
            return 1;
        }

        // 3.5. مرحلة IR (v0.3.0.7): AST → IR
        IRModule* ir_module = ir_lower_program(ast, current_input);
        if (!ir_module) {
            fprintf(stderr, "Aborting %s: internal IR lowering failure.\n", current_input);
            free(source);
            return 1;
        }

        // طباعة IR (اختياري) --dump-ir
        if (config.dump_ir) {
            if (config.verbose) printf("[INFO] Dumping IR (--dump-ir)...\n");
            ir_module_print(ir_module, stdout, 1);
        }

        // كتابة IR إلى ملف (اختياري) --emit-ir
        if (config.emit_ir) {
            char* ir_file = change_extension(current_input, ".ir");
            if (config.verbose) printf("[INFO] Writing IR (--emit-ir): %s\n", ir_file);
            ir_module_dump(ir_module, ir_file, 1);
            free(ir_file);
        }

        // 3.6. مرحلة التحسين (v0.3.1.6): Optimization Pipeline
        if (config.opt_level > OPT_LEVEL_0) {
            if (config.verbose) printf("[INFO] Running optimizer (-%s)...\n", ir_optimizer_level_name(config.opt_level));
            ir_optimizer_run(ir_module, config.opt_level);
        }

        // طباعة IR بعد التحسين (اختياري) --dump-ir-opt
        if (config.dump_ir_opt) {
            if (config.verbose) printf("[INFO] Dumping optimized IR (--dump-ir-opt)...\n");
            ir_module_print(ir_module, stdout, 1);
        }

        // 4. اختيار التعليمات (Instruction Selection) (v0.3.2.1)
        if (config.verbose) printf("[INFO] Running instruction selection...\n");
        MachineModule* mach_module = isel_run(ir_module);
        if (!mach_module) {
            fprintf(stderr, "Aborting %s: instruction selection failed.\n", current_input);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        // 5. تخصيص السجلات (Register Allocation) (v0.3.2.2)
        if (config.verbose) printf("[INFO] Running register allocation...\n");
        if (!regalloc_run(mach_module)) {
            fprintf(stderr, "Aborting %s: register allocation failed.\n", current_input);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        // 6. إصدار كود التجميع (Code Emission) (v0.3.2.3)
        char* asm_file;
        if (config.assembly_only && input_count == 1 && config.output_file) asm_file = config.output_file;
        else asm_file = change_extension(current_input, ".s");

        FILE* f_asm = fopen(asm_file, "w");
        if (!f_asm) {
            printf("Error: Could not write assembly file '%s'\n", asm_file);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        if (config.verbose) printf("[INFO] Emitting assembly: %s\n", asm_file);
        if (!emit_module(mach_module, f_asm)) {
            fprintf(stderr, "Aborting %s: code emission failed.\n", current_input);
            fclose(f_asm);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }
        fclose(f_asm);

        // تحرير الموارد
        mach_module_free(mach_module);
        ir_module_free(ir_module);
        free(source);

        if (config.assembly_only) {
            if (config.verbose) printf("[INFO] Generated assembly: %s\n", asm_file);
            continue; // انتقل للملف التالي
        }

        // 5. التجميع (Assembly)
        char* obj_file;
        if (config.compile_only && input_count == 1 && config.output_file) obj_file = config.output_file;
        else obj_file = change_extension(current_input, ".o");

        char cmd_assemble[1024];
        sprintf(cmd_assemble, "gcc -c %s -o %s", asm_file, obj_file);
        
        if (config.verbose) printf("[CMD] %s\n", cmd_assemble);
        if (system(cmd_assemble) != 0) {
            printf("Error: Assembler failed for %s\n", current_input);
            return 1;
        }

        // تنظيف ملف التجميع المؤقت إذا لم يكن مطلوباً
        if (!config.assembly_only && !config.verbose) {
            remove(asm_file);
            if (asm_file != config.output_file) free(asm_file); // تحرير الاسم المولد
        }

        obj_files_to_link[obj_count++] = obj_file;
    }

    // إذا طلب المستخدم -S أو -c، نتوقف هنا
    if (config.assembly_only || config.compile_only) {
        return 0;
    }

    // --- المرحلة النهائية: الربط (Linking) ---
    if (config.verbose) printf("\n[INFO] Linking %d object files...\n", obj_count);

    char cmd_link[4096];
    strcpy(cmd_link, "gcc");
    for (int i = 0; i < obj_count; i++) {
        strcat(cmd_link, " ");
        strcat(cmd_link, obj_files_to_link[i]);
    }
    strcat(cmd_link, " -o ");
    strcat(cmd_link, config.output_file);

    if (config.verbose) printf("[CMD] %s\n", cmd_link);
    if (system(cmd_link) != 0) {
        printf("Error: Linker failed.\n");
        return 1;
    }

    // تنظيف ملفات الكائنات المؤقتة
    if (!config.verbose) {
        for (int i = 0; i < obj_count; i++) {
            remove(obj_files_to_link[i]);
            // free(obj_files_to_link[i]); // تم تخصيصه بواسطة change_extension
        }
    }

    // ملخص التحذيرات
    int warn_count = warning_get_count();
    if (warn_count > 0 && config.verbose) {
        printf("[INFO] Compilation completed with %d warning(s).\n", warn_count);
    }
    
    // عرض وقت الترجمة
    if (config.verbose) {
        double elapsed = get_time_seconds() - config.start_time;
        printf("[INFO] Build successful: %s\n", config.output_file);
        printf("[INFO] Compilation time: %.3f seconds\n", elapsed);
    }
    
    return 0;
}