/**
 * @file main.c
 * @brief نقطة الدخول ومحرك سطر الأوامر (CLI Driver).
 * يقوم بإدارة عملية الترجمة من التحليل إلى التجميع والربط.
 * @version 0.2.2 (Diagnostic Engine Integration)
 */

#include "baa.h"

// ============================================================================
// إعدادات المترجم (Compiler Configuration)
// ============================================================================

typedef struct {
    char* input_file;       // ملف المصدر (.b)
    char* output_file;      // ملف الخرج (.exe, .o, .s)
    bool assembly_only;     // -S: إنتاج كود تجميع فقط
    bool compile_only;      // -c: تجميع إلى كائن فقط (بدون ربط)
    bool verbose;           // -v: وضع التفاصيل
} CompilerConfig;

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

/**
 * @brief طباعة رسالة المساعدة.
 */
void print_help() {
    printf("Baa Compiler (baa) - The Arabic Programming Language\n");
    printf("Usage: baa [options] file...\n");
    printf("Options:\n");
    printf("  -o <file>    Place the output into <file>\n");
    printf("  -S           Compile only; do not assemble or link (generates .s)\n");
    printf("  -c           Compile and assemble, but do not link (generates .o)\n");
    printf("  -v           Verbose mode\n");
    printf("  --version    Display version information\n");
    printf("  --help       Display this information\n");
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

    // 1. تحليل المعاملات (Argument Parsing)
    if (argc < 2) {
        print_help();
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg[0] == '-') {
            if (strcmp(arg, "-S") == 0) config.assembly_only = true;
            else if (strcmp(arg, "-c") == 0) config.compile_only = true;
            else if (strcmp(arg, "-v") == 0) config.verbose = true;
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
            else {
                printf("Error: Unknown flag '%s'\n", arg);
                return 1;
            }
        } else {
            if (config.input_file == NULL) config.input_file = arg;
            else { printf("Error: Multiple input files not supported yet\n"); return 1; }
        }
    }

    if (!config.input_file) {
        printf("Error: No input file specified\n");
        return 1;
    }

    // تحديد اسم الملف المخرج الافتراضي إذا لم يتم تحديده
    if (!config.output_file) {
        if (config.assembly_only) config.output_file = change_extension(config.input_file, ".s");
        else if (config.compile_only) config.output_file = change_extension(config.input_file, ".o");
        else config.output_file = strdup("out.exe");
    }

    // --- المرحلة 1: الواجهة الأمامية (Frontend) ---
    if (config.verbose) printf("[INFO] Compiling %s...\n", config.input_file);
    
    char* source = read_file(config.input_file);
    
    // تمرير اسم الملف للـ Lexer لتتبع الأخطاء
    Lexer lexer;
    lexer_init(&lexer, source, config.input_file);
    
    Node* ast = parse(&lexer);

    // التحقق من وجود أخطاء قبل المتابعة
    if (error_has_occurred()) {
        fprintf(stderr, "Aborting due to compilation errors.\n");
        free(source);
        return 1;
    }
    
    if (config.verbose) printf("[INFO] AST generated successfully.\n");

    // --- المرحلة 2: الواجهة الخلفية (Backend - Codegen) ---
    // نولد دائماً ملف تجميع مؤقت أو نهائي
    char* asm_file = config.assembly_only ? config.output_file : "temp.s";
    
    FILE* f_asm = fopen(asm_file, "w");
    if (!f_asm) { printf("Error: Could not write assembly file\n"); return 1; }
    
    codegen(ast, f_asm);
    fclose(f_asm);
    
    if (config.verbose) printf("[INFO] Assembly generated at %s.\n", asm_file);

    // إذا طلب المستخدم كود التجميع فقط، نتوقف هنا
    if (config.assembly_only) {
        free(source);
        return 0;
    }

    // --- المرحلة 3: التجميع (Assemble) ---
    char* obj_file = config.compile_only ? config.output_file : "temp.o";
    char cmd_assemble[1024];
    sprintf(cmd_assemble, "gcc -c %s -o %s", asm_file, obj_file);
    
    if (config.verbose) printf("[CMD] %s\n", cmd_assemble);
    if (system(cmd_assemble) != 0) {
        printf("Error: Assembler (gcc) failed.\n");
        return 1;
    }

    // إذا طلب المستخدم التجميع فقط (-c)، نتوقف هنا
    if (config.compile_only) {
        remove("temp.s");
        free(source);
        return 0;
    }

    // --- المرحلة 4: الربط (Link) ---
    char cmd_link[1024];
    sprintf(cmd_link, "gcc %s -o %s", obj_file, config.output_file);
    
    if (config.verbose) printf("[CMD] %s\n", cmd_link);
    if (system(cmd_link) != 0) {
        printf("Error: Linker (gcc) failed.\n");
        return 1;
    }

    // التنظيف
    if (!config.verbose) {
        remove("temp.s");
        remove("temp.o");
    }
    free(source);

    if (config.verbose) printf("[INFO] Build successful: %s\n", config.output_file);
    return 0;
}