/**
 * @file main.c
 * @brief نقطة الدخول ومحرك سطر الأوامر (CLI Driver).
 * @version 0.3.2.9.3
 */

#include "baa.h"
#include "ir_lower.h"
#include "ir_optimizer.h"
#include "ir_outssa.h"
#include "ir_unroll.h"
#include "ir_verify_ssa.h"
#include "ir_verify_ir.h"
#include "isel.h"
#include "regalloc.h"
#include "emit.h"
#include "target.h"
#include "code_model.h"
#include <time.h>
#include <stdarg.h>

#if defined(_WIN32) && defined(BAA_ENABLE_LEGACY_CODEGEN)
// إعلان الخلفية القديمة (AST->ASM) عند تفعيلها في CMake.
void codegen(Node* node, FILE* file);
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#ifdef _WIN32
#define BAA_PATH_SUFFIX_GCC_BIN "\\gcc\\bin\\gcc.exe"
#define BAA_PATH_SUFFIX_GCC_BIN_DEV "\\..\\gcc\\bin\\gcc.exe"

/**
 * @brief بناء مسار (base + suffix) داخل مخزن مع ضمان عدم تجاوز السعة.
 * @return true عند النجاح، false عند تجاوز السعة.
 */
static bool path_build_suffix(char* out, size_t out_cap, const char* base, const char* suffix)
{
    if (!out || out_cap == 0 || !base || !suffix) return false;

    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);

    if (base_len >= out_cap) return false;
    if (suffix_len > out_cap - 1 - base_len) return false;

    memcpy(out, base, base_len);
    memcpy(out + base_len, suffix, suffix_len + 1);
    return true;
}

/**
 * @brief وضع اقتباس مزدوج حول مسار داخل مخزن ( "path" ) مع ضمان السعة.
 * @return true عند النجاح، false عند تجاوز السعة.
 */
static bool path_quote(char* out, size_t out_cap, const char* path)
{
    if (!out || out_cap == 0 || !path) return false;

    size_t path_len = strlen(path);
    if (path_len + 2 + 1 > out_cap) return false;

    out[0] = '"';
    memcpy(out + 1, path, path_len);
    out[1 + path_len] = '"';
    out[1 + path_len + 1] = '\0';
    return true;
}
#endif

// ============================================================================
// إعدادات المترجم (Compiler Configuration)
// ============================================================================

typedef enum
{
    BAA_BACKEND_IR = 0,
    BAA_BACKEND_LEGACY = 1,
} BaaBackendKind;

typedef struct
{
    char *input_file;   // ملف المصدر (.baa)
    char *output_file;  // ملف الخرج (.exe, .o, .s)
    bool assembly_only; // -S: إنتاج كود تجميع فقط
    bool compile_only;  // -c: تجميع إلى كائن فقط (بدون ربط)
    bool verbose;       // -v: وضع التفاصيل
    bool show_timings;  // -v: عرض وقت الترجمة
    bool dump_ir;       // --dump-ir: طباعة IR بعد التحليل الدلالي
    bool emit_ir;       // --emit-ir: كتابة IR إلى ملف .ir بجانب المصدر
    bool dump_ir_opt;   // --dump-ir-opt: طباعة IR بعد التحسين
    bool verify_ir;     // --verify-ir: التحقق من سلامة/تشكيل الـ IR (well-formedness)
    bool verify_ssa;    // --verify-ssa: التحقق من صحة SSA (بعد Mem2Reg وقبل OutSSA)
    bool verify_gate;   // --verify-gate: تشغيل verify-ir/verify-ssa بعد كل دورة تمريرات داخل المُحسِّن (debug)
    bool time_phases;   // --time-phases: قياس وطمأنة أزمنة مراحل المُصرّف
    bool compare_backends; // --compare-backends: مقارنة الخلفية القديمة مقابل خلفية IR (Windows-only)
    bool debug_info;    // --debug-info: إصدار معلومات ديبغ (سطر/ملف) داخل ملف .s
    bool funroll_loops; // -funroll-loops: فك الحلقات بعد Out-of-SSA (تحسين اختياري)
    OptLevel opt_level; // -O0, -O1, -O2: مستوى التحسين
    const BaaTarget* target; // --target=...: الهدف الحالي
    BaaCodegenOptions codegen_opts; // v0.3.2.8.3
    BaaBackendKind backend; // --backend=ir|legacy: اختيار مسار الخلفية
    double start_time;  // وقت بدء الترجمة
} CompilerConfig;

typedef struct
{
    double read_file_s;
    double parse_s;
    double analyze_s;
    double lower_ir_s;
    double optimize_s;
    double verify_ir_s;
    double verify_ssa_s;
    double outssa_s;
    double unroll_s;
    double isel_s;
    double regalloc_s;
    double emit_s;
    double assemble_s;
    double link_s;
    double total_s;

    size_t ir_arena_used_max;
    size_t ir_arena_cap_max;
    size_t ir_arena_chunks_max;
} CompilerPhaseTimes;

// ============================================================================
// بناء الأوامر بأمان (Safe Command Builder)
// ============================================================================

/**
 * @brief إلحاق نص إلى مخزن أمر مع التحقق من السعة.
 * @return true عند النجاح، false عند تجاوز السعة.
 */
static bool cmd_append(char* buf, size_t cap, size_t* len_io, const char* s)
{
    if (!buf || cap == 0 || !len_io || !s) return false;

    size_t len = *len_io;
    size_t slen = strlen(s);
    if (len >= cap) return false;
    if (slen > cap - 1 - len) return false;

    memcpy(buf + len, s, slen);
    len += slen;
    buf[len] = '\0';
    *len_io = len;
    return true;
}

/**
 * @brief إلحاق نص مُنسّق إلى مخزن أمر مع التحقق من السعة.
 * @return true عند النجاح، false عند تجاوز السعة أو خطأ تنسيق.
 */
static bool cmd_appendf(char* buf, size_t cap, size_t* len_io, const char* fmt, ...)
{
    if (!buf || cap == 0 || !len_io || !fmt) return false;

    size_t len = *len_io;
    if (len >= cap) return false;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + len, cap - len, fmt, args);
    va_end(args);

    if (written < 0) return false;
    if ((size_t)written >= cap - len) return false;

    *len_io = len + (size_t)written;
    return true;
}

// ============================================================================
// دوال قياس الوقت (Timing Functions)
// ============================================================================

/**
 * @brief الحصول على الوقت الحالي بدقة عالية (بالثواني).
 */
static double get_time_seconds(void)
{
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

static BaaObjectFormat host_object_format(void)
{
#ifdef _WIN32
    return BAA_OBJFORMAT_COFF;
#else
    return BAA_OBJFORMAT_ELF;
#endif
}

// ============================================================================
// دالات مساعدة (Helper Functions)
// ============================================================================

/**
 * @brief قراءة ملف بالكامل إلى الذاكرة.
 */
char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Error: Could not open input file '%s'\n", path);
        exit(1);
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        printf("Error: Could not seek input file '%s'\n", path);
        exit(1);
    }
    long length = ftell(f);
    if (length < 0) {
        printf("Error: Could not read size of input file '%s'\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        printf("Error: Could not seek input file '%s'\n", path);
        exit(1);
    }

    char *buffer = malloc((size_t)length + 1);
    if (!buffer)
    {
        printf("Error: Memory allocation failed\n");
        exit(1);
    }

    size_t got = fread(buffer, 1, (size_t)length, f);
    if (got != (size_t)length)
    {
        if (ferror(f)) {
            printf("Error: Could not read input file '%s'\n", path);
            exit(1);
        }
        length = (long)got;
    }

    buffer[length] = '\0';
    fclose(f);
    return buffer;
}

/**
 * @brief تغيير امتداد الملف (مثلاً من .b إلى .s).
 */
char *change_extension(const char *filename, const char *new_ext)
{
    if (!filename || !new_ext) {
        fprintf(stderr, "خطأ: change_extension استلم مُدخلات فارغة (NULL)\n");
        exit(1);
    }

    char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return strdup(new_ext + (dot == filename)); // Fallback

    size_t base_len = dot - filename;
    size_t ext_len = strlen(new_ext);
    char *new_name = malloc(base_len + ext_len + 1);
    if (!new_name) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    memcpy(new_name, filename, base_len);
    memcpy(new_name + base_len, new_ext, ext_len + 1);
    return new_name;
}

// ============================================================================
// البحث عن GCC المرفق (Bundled GCC Resolution)
// ============================================================================

/**
 * @brief مسار GCC المكتشف (فارغ = استخدام "gcc" من PATH).
 *
 * عند بدء التشغيل، نبحث عن gcc.exe بجوار baa.exe في المجلد الفرعي
 * gcc\bin\gcc.exe.  إذا وُجد، نستعمل المسار الكامل في جميع أوامر
 * التجميع والربط.  وإلا نرجع إلى "gcc" العادي على PATH.
 */
static char g_gcc_path[MAX_PATH] = "";

/**
 * @brief تحديد مسار مجلد baa.exe والبحث عن gcc المضمّن.
 *
 * يبحث في:
 *   1. <baa_dir>\gcc\bin\gcc.exe   (التثبيت عبر المثبّت)
 *   2. <baa_dir>\..\gcc\bin\gcc.exe (تطوير محلي)
 * إذا لم يُوجد، يبقى g_gcc_path فارغاً → يُستخدم "gcc" من PATH.
 */
static void resolve_gcc_path(void)
{
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof(exe_path));
    if (len == 0 || len >= (DWORD)sizeof(exe_path))
        return;

    // إزالة اسم الملف التنفيذي (الحصول على المجلد)
    char *last_sep = strrchr(exe_path, '\\');
    if (!last_sep)
        return;
    *last_sep = '\0';

    // المسار 1: <baa_dir>\gcc\bin\gcc.exe
    char candidate[MAX_PATH];
    if (path_build_suffix(candidate, sizeof(candidate), exe_path, BAA_PATH_SUFFIX_GCC_BIN) &&
        GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
    {
        (void)path_quote(g_gcc_path, sizeof(g_gcc_path), candidate);
        return;
    }

    // المسار 2: <baa_dir>\..\gcc\bin\gcc.exe (هيكل التطوير)
    if (path_build_suffix(candidate, sizeof(candidate), exe_path, BAA_PATH_SUFFIX_GCC_BIN_DEV) &&
        GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
    {
        (void)path_quote(g_gcc_path, sizeof(g_gcc_path), candidate);
        return;
    }
#endif
    // لم يُوجد gcc مضمّن ← سيُستخدم "gcc" من PATH
}

/**
 * @brief إرجاع أمر GCC المناسب ("gcc" أو المسار الكامل).
 */
static const char *get_gcc_command(void)
{
    return g_gcc_path[0] ? g_gcc_path : "gcc";
}

// ============================================================================
// IR Integration (v0.3.0.7)
// ============================================================================

#if defined(_WIN32)
// ============================================================================
// مقارنة الخلفيات (Regression: legacy vs IR) — Windows-only
// ============================================================================

static bool win_get_self_exe_path(char* out, size_t out_cap)
{
    if (!out || out_cap == 0) return false;
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_cap);
    if (n == 0 || n >= (DWORD)out_cap) return false;
    out[n] = '\0';
    return true;
}

static bool win_make_temp_path(char* out, size_t out_cap, const char* prefix, const char* ext)
{
    if (!out || out_cap == 0 || !prefix || !ext) return false;

    char tmp_dir[MAX_PATH];
    DWORD n = GetTempPathA((DWORD)sizeof(tmp_dir), tmp_dir);
    if (n == 0 || n >= (DWORD)sizeof(tmp_dir)) return false;

    char tmp_file[MAX_PATH];
    UINT u = GetTempFileNameA(tmp_dir, prefix, 0, tmp_file);
    if (u == 0) return false;

    // GetTempFileNameA ينشئ الملف فعلياً؛ نحذفه ونستخدم الاسم كأساس.
    (void)DeleteFileA(tmp_file);

    // استبدال الامتداد.
    char* dot = strrchr(tmp_file, '.');
    if (!dot)
    {
        size_t need = strlen(tmp_file) + strlen(ext) + 1;
        if (need > out_cap) return false;
        (void)snprintf(out, out_cap, "%s%s", tmp_file, ext);
        return true;
    }

    size_t base_len = (size_t)(dot - tmp_file);
    size_t ext_len = strlen(ext);
    if (base_len + ext_len + 1 > out_cap) return false;
    memcpy(out, tmp_file, base_len);
    memcpy(out + base_len, ext, ext_len + 1);
    return true;
}

static const char* opt_level_to_flag(OptLevel lvl)
{
    switch (lvl)
    {
        case OPT_LEVEL_0: return "O0";
        case OPT_LEVEL_1: return "O1";
        case OPT_LEVEL_2: return "O2";
        default: return "O1";
    }
}

static int file_read_all(const char* path, char** out_buf, size_t* out_len)
{
    if (!out_buf || !out_len) return 0;
    *out_buf = NULL;
    *out_len = 0;

    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return 0; }

    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    *out_buf = b;
    *out_len = got;
    return 1;
}

static void report_first_diff(const char* a, size_t a_len, const char* b, size_t b_len)
{
    size_t n = (a_len < b_len) ? a_len : b_len;
    size_t i = 0;
    for (; i < n; i++)
    {
        if (a[i] != b[i]) break;
    }

    fprintf(stderr, "اختلاف في المخرجات عند الموضع %zu.\n", i);
    fprintf(stderr, "ملاحظة: قد يكون اختلاف CRLF/LF إذا كانت البيئة غير متسقة.\n");
}

static int run_compare_backends(const CompilerConfig* config, const char* input_file)
{
    if (!config || !input_file) return 1;

#if !defined(BAA_ENABLE_LEGACY_CODEGEN)
    fprintf(stderr,
            "خطأ: وضع --compare-backends يتطلب بناء المترجم مع الخلفية القديمة.\n"
            "أعد تهيئة CMake مع -DBAA_ENABLE_LEGACY_CODEGEN=ON.\n");
    return 1;
#else
    if (!config->target || config->target->kind != BAA_TARGET_X86_64_WINDOWS)
    {
        fprintf(stderr, "خطأ: --compare-backends مدعوم فقط لهدف x86_64-windows.\n");
        return 1;
    }

    // قيود المقارنة: ملف واحد، بناء نهائي، بدون خصائص IR الخاصة.
    if (config->assembly_only || config->compile_only)
    {
        fprintf(stderr, "خطأ: --compare-backends يتطلب بناء وربط نهائي (بدون -S/-c).\n");
        return 1;
    }
    if (config->dump_ir || config->emit_ir || config->dump_ir_opt || config->verify_ir || config->verify_ssa || config->verify_gate)
    {
        fprintf(stderr, "خطأ: --compare-backends لا يعمل مع أوضاع IR/التحقق (dump/verify).\n");
        return 1;
    }
    if (config->funroll_loops)
    {
        fprintf(stderr, "خطأ: --compare-backends لا يعمل مع -funroll-loops (الخلفية القديمة لا تدعم ذلك).\n");
        return 1;
    }

    char self_path[MAX_PATH];
    if (!win_get_self_exe_path(self_path, sizeof(self_path)))
    {
        fprintf(stderr, "خطأ: فشل تحديد مسار المترجم الحالي.\n");
        return 1;
    }

    char ir_exe[MAX_PATH];
    char legacy_exe[MAX_PATH];
    char ir_out[MAX_PATH];
    char legacy_out[MAX_PATH];
    if (!win_make_temp_path(ir_exe, sizeof(ir_exe), "baa", ".exe") ||
        !win_make_temp_path(legacy_exe, sizeof(legacy_exe), "baa", ".exe") ||
        !win_make_temp_path(ir_out, sizeof(ir_out), "baa", ".txt") ||
        !win_make_temp_path(legacy_out, sizeof(legacy_out), "baa", ".txt"))
    {
        fprintf(stderr, "خطأ: فشل إنشاء مسارات مؤقتة للمقارنة.\n");
        return 1;
    }

    char q_self[MAX_PATH * 2];
    char q_input[MAX_PATH * 2];
    char q_ir_exe[MAX_PATH * 2];
    char q_legacy_exe[MAX_PATH * 2];
    char q_ir_out[MAX_PATH * 2];
    char q_legacy_out[MAX_PATH * 2];

    if (!path_quote(q_self, sizeof(q_self), self_path) ||
        !path_quote(q_input, sizeof(q_input), input_file) ||
        !path_quote(q_ir_exe, sizeof(q_ir_exe), ir_exe) ||
        !path_quote(q_legacy_exe, sizeof(q_legacy_exe), legacy_exe) ||
        !path_quote(q_ir_out, sizeof(q_ir_out), ir_out) ||
        !path_quote(q_legacy_out, sizeof(q_legacy_out), legacy_out))
    {
        fprintf(stderr, "خطأ: فشل اقتباس المسارات.\n");
        return 1;
    }

    const char* opt = opt_level_to_flag(config->opt_level);
    const char* tgt = config->target->name ? config->target->name : "x86_64-windows";

    char cmd_ir[4096];
    char cmd_legacy[4096];
    {
        size_t len = 0;
        cmd_ir[0] = '\0';
        if (!cmd_appendf(cmd_ir, sizeof(cmd_ir), &len,
                         "%s -%s --target=%s --backend=ir %s -o %s",
                         q_self, opt, tgt, q_input, q_ir_exe))
        {
            fprintf(stderr, "خطأ: أمر المقارنة (IR) طويل جداً.\n");
            return 1;
        }
    }
    {
        size_t len = 0;
        cmd_legacy[0] = '\0';
        if (!cmd_appendf(cmd_legacy, sizeof(cmd_legacy), &len,
                         "%s -%s --target=%s --backend=legacy %s -o %s",
                         q_self, opt, tgt, q_input, q_legacy_exe))
        {
            fprintf(stderr, "خطأ: أمر المقارنة (legacy) طويل جداً.\n");
            return 1;
        }
    }

    if (config->verbose)
    {
        printf("[CMD] %s\n", cmd_ir);
        printf("[CMD] %s\n", cmd_legacy);
    }

    if (system(cmd_ir) != 0)
    {
        fprintf(stderr, "فشل بناء نسخة IR أثناء المقارنة.\n");
        return 1;
    }
    if (system(cmd_legacy) != 0)
    {
        fprintf(stderr, "فشل بناء نسخة legacy أثناء المقارنة.\n");
        return 1;
    }

    char run_ir[4096];
    char run_legacy[4096];
    {
        size_t len = 0;
        run_ir[0] = '\0';
        if (!cmd_appendf(run_ir, sizeof(run_ir), &len, "%s > %s 2>&1", q_ir_exe, q_ir_out))
        {
            fprintf(stderr, "خطأ: أمر التشغيل (IR) طويل جداً.\n");
            return 1;
        }
    }
    {
        size_t len = 0;
        run_legacy[0] = '\0';
        if (!cmd_appendf(run_legacy, sizeof(run_legacy), &len, "%s > %s 2>&1", q_legacy_exe, q_legacy_out))
        {
            fprintf(stderr, "خطأ: أمر التشغيل (legacy) طويل جداً.\n");
            return 1;
        }
    }

    int rc_ir = system(run_ir);
    int rc_legacy = system(run_legacy);

    // حسب توصية المشروع: حتى لو تطابقت المخرجات، الفشل غير الصفري يجب أن يُفشل المقارنة.
    if (rc_ir != 0 || rc_legacy != 0)
    {
        fprintf(stderr, "فشل تشغيل البرامج أثناء المقارنة (ir=%d, legacy=%d).\n", rc_ir, rc_legacy);
        return 1;
    }

    char* a = NULL;
    char* b = NULL;
    size_t a_len = 0, b_len = 0;
    if (!file_read_all(ir_out, &a, &a_len) || !file_read_all(legacy_out, &b, &b_len))
    {
        fprintf(stderr, "خطأ: فشل قراءة مخرجات المقارنة.\n");
        free(a);
        free(b);
        return 1;
    }

    int ok = (a_len == b_len) && (memcmp(a, b, a_len) == 0);
    if (!ok)
    {
        fprintf(stderr, "فشل: مخرجات الخلفيتين غير متطابقة.\n");
        report_first_diff(a, a_len, b, b_len);
    }

    free(a);
    free(b);

    // إن طلب المستخدم -o، انسخ نسخة IR كـ "ناتج" افتراضي بعد نجاح المقارنة.
    if (ok && config->output_file)
    {
        if (!CopyFileA(ir_exe, config->output_file, FALSE))
        {
            fprintf(stderr, "تحذير: فشل نسخ ناتج IR إلى -o (%s).\n", config->output_file);
        }
    }

    (void)DeleteFileA(ir_exe);
    (void)DeleteFileA(legacy_exe);
    (void)DeleteFileA(ir_out);
    (void)DeleteFileA(legacy_out);

    if (ok)
    {
        if (config->verbose) printf("[INFO] compare-backends: PASS\n");
        return 0;
    }
    return 1;
#endif
}
#endif // _WIN32

/**
 * @brief تحليل علم تحذير (-W...).
 * @return true إذا تم التعرف على العلم.
 */
static bool parse_warning_flag(const char *flag)
{
    // -Wall: تفعيل جميع التحذيرات
    if (strcmp(flag, "-Wall") == 0)
    {
        g_warning_config.all_warnings = true;
        return true;
    }

    // -Werror: معاملة التحذيرات كأخطاء
    if (strcmp(flag, "-Werror") == 0)
    {
        g_warning_config.warnings_as_errors = true;
        return true;
    }

    // -Wno-color: تعطيل الألوان
    if (strcmp(flag, "-Wno-color") == 0)
    {
        g_warning_config.colored_output = false;
        return true;
    }

    // -Wcolor: تفعيل الألوان
    if (strcmp(flag, "-Wcolor") == 0)
    {
        g_warning_config.colored_output = true;
        return true;
    }

    // -Wunused-variable: تفعيل تحذير المتغيرات غير المستخدمة
    if (strcmp(flag, "-Wunused-variable") == 0)
    {
        g_warning_config.enabled[WARN_UNUSED_VARIABLE] = true;
        return true;
    }

    // -Wno-unused-variable: تعطيل تحذير المتغيرات غير المستخدمة
    if (strcmp(flag, "-Wno-unused-variable") == 0)
    {
        g_warning_config.enabled[WARN_UNUSED_VARIABLE] = false;
        return true;
    }

    // -Wdead-code: تفعيل تحذير الكود الميت
    if (strcmp(flag, "-Wdead-code") == 0)
    {
        g_warning_config.enabled[WARN_DEAD_CODE] = true;
        return true;
    }

    // -Wno-dead-code: تعطيل تحذير الكود الميت
    if (strcmp(flag, "-Wno-dead-code") == 0)
    {
        g_warning_config.enabled[WARN_DEAD_CODE] = false;
        return true;
    }

    return false;
}

/**
 * @brief طباعة رسالة المساعدة.
 */
void print_help()
{
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
    printf("  --verify       Run all verifiers (--verify-ir + --verify-ssa; requires -O1/-O2)\n");
    printf("  --verify-ir    Verify IR well-formedness (operands/types/terminators/phi/calls)\n");
    printf("  --verify-ssa   Verify SSA invariants after Mem2Reg (requires -O1/-O2)\n");
    printf("  --verify-gate  Debug: run verify-ir/verify-ssa after each optimizer iteration\n");
    printf("  --time-phases  Print per-phase timing/memory stats\n");
    printf("  --debug-info   Emit debug line info (.file/.loc) and pass -g to toolchain\n");
    printf("  -O0            Disable optimization\n");
    printf("  -O1            Basic optimization (default)\n");
    printf("  -O2            Full optimization (+ CSE)\n");
    printf("  -funroll-loops  Unroll small constant-count loops (after Out-of-SSA)\n");
    printf("  --target=<t>    Target: x86_64-windows | x86_64-linux\n");
    printf("  --backend=<b>   Backend: ir | legacy (legacy is Windows-only)\n");
    printf("  --compare-backends  Windows-only: build+run both backends and compare output\n");
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
void print_version()
{
    printf("baa version %s\n", BAA_VERSION);
    printf("Built on %s\n", BAA_BUILD_DATE);
}

// ============================================================================
// نقطة الدخول (Main Entry Point)
// ============================================================================

int main(int argc, char **argv)
{
    CompilerConfig config = {0};
    config.output_file = NULL;
    config.opt_level = OPT_LEVEL_1; // Default optimization level
    config.funroll_loops = false;
    config.target = baa_target_host_default();
    config.codegen_opts = baa_codegen_options_default();
    config.backend = BAA_BACKEND_IR;

    char *input_files[32]; // دعم حتى 32 ملف مصدر
    int input_count = 0;

    // تهيئة نظام التحذيرات
    warning_init();

    // البحث عن GCC المضمّن (إن وُجد)
    resolve_gcc_path();

    // تسجيل وقت البدء
    config.start_time = get_time_seconds();

    // 0. التحقق من وجود معاملات
    if (argc < 2)
    {
        print_help();
        return 1;
    }

    // 1. التحقق من أمر التحديث (Update Command)
    // يجب أن يكون "update" هو المعامل الوحيد
    if (argc == 2 && strcmp(argv[1], "update") == 0)
    {
        run_updater();
        return 0;
    }

    // 2. تحليل المعاملات (Argument Parsing)
    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if (arg[0] == '-')
        {
            if (strcmp(arg, "-S") == 0 || strcmp(arg, "-s") == 0)
                config.assembly_only = true;
            else if (strcmp(arg, "-c") == 0)
                config.compile_only = true;
            else if (strcmp(arg, "-v") == 0)
            {
                config.verbose = true;
                config.show_timings = true;
            }
            else if (strcmp(arg, "--dump-ir") == 0)
            {
                config.dump_ir = true;
            }
            else if (strcmp(arg, "--emit-ir") == 0)
            {
                config.emit_ir = true;
            }
            else if (strcmp(arg, "--dump-ir-opt") == 0)
            {
                config.dump_ir_opt = true;
            }
            else if (strcmp(arg, "--verify") == 0)
            {
                config.verify_ir = true;
                config.verify_ssa = true;
            }
            else if (strcmp(arg, "--verify-ir") == 0)
            {
                config.verify_ir = true;
            }
            else if (strcmp(arg, "--verify-ssa") == 0)
            {
                config.verify_ssa = true;
            }
            else if (strcmp(arg, "--verify-gate") == 0)
            {
                config.verify_gate = true;
            }
            else if (strcmp(arg, "--time-phases") == 0)
            {
                config.time_phases = true;
            }
            else if (strncmp(arg, "--backend=", 10) == 0)
            {
                const char* b = arg + 10;
                if (strcmp(b, "ir") == 0)
                {
                    config.backend = BAA_BACKEND_IR;
                }
                else if (strcmp(b, "legacy") == 0)
                {
#if !defined(_WIN32) || !defined(BAA_ENABLE_LEGACY_CODEGEN)
                    fprintf(stderr,
                            "خطأ: الخلفية القديمة (legacy) غير مفعلة في هذا البناء.\n"
                            "ملاحظة (Windows): أعد تهيئة CMake مع -DBAA_ENABLE_LEGACY_CODEGEN=ON.\n");
                    return 1;
#else
                    config.backend = BAA_BACKEND_LEGACY;
#endif
                }
                else
                {
                    fprintf(stderr, "خطأ: قيمة --backend غير معروفة: %s\n", b);
                    return 1;
                }
            }
            else if (strcmp(arg, "--compare-backends") == 0)
            {
                config.compare_backends = true;
            }
            else if (strcmp(arg, "--debug-info") == 0)
            {
                config.debug_info = true;
            }
            else if (strncmp(arg, "--target=", 9) == 0)
            {
                const char *t = arg + 9;
                const BaaTarget *parsed = baa_target_parse(t);
                if (!parsed)
                {
                    printf("Error: Unknown target '%s'\n", t);
                    return 1;
                }
                config.target = parsed;
            }
            else if (strcmp(arg, "-fPIC") == 0)
            {
                config.codegen_opts.pic = true;
            }
            else if (strcmp(arg, "-fPIE") == 0)
            {
                config.codegen_opts.pie = true;
                config.codegen_opts.pic = true;
            }
            else if (strcmp(arg, "-fno-pic") == 0)
            {
                config.codegen_opts.pic = false;
            }
            else if (strcmp(arg, "-fno-pie") == 0)
            {
                config.codegen_opts.pie = false;
            }
            else if (strcmp(arg, "-fstack-protector") == 0)
            {
                config.codegen_opts.stack_protector = BAA_STACKPROT_ON;
            }
            else if (strcmp(arg, "-fstack-protector-all") == 0)
            {
                config.codegen_opts.stack_protector = BAA_STACKPROT_ALL;
            }
            else if (strcmp(arg, "-fno-stack-protector") == 0)
            {
                config.codegen_opts.stack_protector = BAA_STACKPROT_OFF;
            }
            else if (strncmp(arg, "-mcmodel=", 9) == 0)
            {
                const char *m = arg + 9;
                if (strcmp(m, "small") == 0)
                {
                    config.codegen_opts.code_model = BAA_CODEMODEL_SMALL;
                }
                else
                {
                    printf("Error: Unsupported code model '%s' (only small is supported)\n", m);
                    return 1;
                }
            }
            else if (strcmp(arg, "-O0") == 0)
            {
                config.opt_level = OPT_LEVEL_0;
            }
            else if (strcmp(arg, "-O1") == 0)
            {
                config.opt_level = OPT_LEVEL_1;
            }
        else if (strcmp(arg, "-O2") == 0)
        {
            config.opt_level = OPT_LEVEL_2;
        }
        else if (strcmp(arg, "-funroll-loops") == 0)
        {
            config.funroll_loops = true;
        }
            else if (strcmp(arg, "-o") == 0)
            {
                if (i + 1 < argc)
                    config.output_file = argv[++i];
                else
                {
                    printf("Error: -o requires a filename\n");
                    return 1;
                }
            }
            else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
            {
                print_help();
                return 0;
            }
            else if (strcmp(arg, "--version") == 0)
            {
                print_version();
                return 0;
            }
            // أعلام التحذيرات (-W...)
            else if (strncmp(arg, "-W", 2) == 0)
            {
                if (!parse_warning_flag(arg))
                {
                    printf("Error: Unknown warning flag '%s'\n", arg);
                    return 1;
                }
            }
            else
            {
                printf("Error: Unknown flag '%s'\n", arg);
                return 1;
            }
        }
        else
        {
            if (input_count < 32)
            {
                input_files[input_count++] = arg;
            }
            else
            {
                printf("Error: Too many input files (Max 32)\n");
                return 1;
            }
        }
    }

    if (input_count == 0)
    {
        printf("Error: No input file specified\n");
        return 1;
    }

    // تحديد اسم الملف المخرج الافتراضي
    if (!config.output_file && !config.compare_backends)
    {
        if (config.assembly_only)
            config.output_file = NULL; // سيتم تحديده لكل ملف
        else if (config.compile_only)
            config.output_file = NULL; // سيتم تحديده لكل ملف
        else
        {
            const char *ext = (config.target && config.target->default_exe_ext) ? config.target->default_exe_ext : ".exe";
            size_t n = strlen("out") + strlen(ext) + 1;
            config.output_file = (char *)malloc(n);
            if (!config.output_file)
            {
                fprintf(stderr, "خطأ: نفدت الذاكرة.\n");
                return 1;
            }
            (void)snprintf(config.output_file, n, "out%s", ext);
        }
    }

    // قائمة ملفات الكائنات للربط
    char *obj_files_to_link[32];
    int obj_count = 0;

    CompilerPhaseTimes phase_times = {0};

    // v0.3.2.8.4: لا ندعم حالياً الربط/التجميع العابر للأهداف (cross-link/cross-assemble).
    // - نسمح بـ -S لتوليد assembly فقط لأي هدف.
    // - أما -c أو الربط النهائي فيتطلبان أن يطابق الهدف نظام المضيف.
    if (!config.assembly_only)
    {
        if (config.target && config.target->obj_format != host_object_format())
        {
            fprintf(stderr,
                    "خطأ: الهدف '%s' لا يطابق نظام المضيف لمرحلة التجميع/الربط.\n"
                    "ملاحظة: استخدم -S لتوليد ملف .s فقط. الدعم الكامل لـ cross-target مؤجل.\n",
                    config.target->name ? config.target->name : "<unknown>");
            return 1;
        }
    }

    // مقارنة الخلفيات (Windows-only)
    if (config.compare_backends)
    {
#ifdef _WIN32
        if (input_count != 1)
        {
            fprintf(stderr, "خطأ: --compare-backends يتطلب ملف مصدر واحد فقط.\n");
            return 1;
        }
        return run_compare_backends(&config, input_files[0]);
#else
        fprintf(stderr, "خطأ: --compare-backends مدعوم فقط على Windows.\n");
        return 1;
#endif
    }

    // قيود الخلفية القديمة
    if (config.backend == BAA_BACKEND_LEGACY)
    {
        if (input_count != 1)
        {
            fprintf(stderr, "خطأ: الخلفية القديمة (legacy) تدعم ملفاً واحداً فقط (لتفادي تعارضات الأقسام/الرموز).\n");
            return 1;
        }
        if (!config.target || config.target->kind != BAA_TARGET_X86_64_WINDOWS)
        {
            fprintf(stderr, "خطأ: الخلفية القديمة (legacy) مدعومة فقط لهدف x86_64-windows.\n");
            return 1;
        }
        if (config.dump_ir || config.emit_ir || config.dump_ir_opt || config.verify_ir || config.verify_ssa || config.verify_gate)
        {
            fprintf(stderr, "خطأ: الخلفية القديمة (legacy) لا تدعم أوضاع IR/التحقق (dump/verify).\n");
            return 1;
        }
        if (config.funroll_loops)
        {
            fprintf(stderr, "خطأ: -funroll-loops غير مدعوم على الخلفية القديمة (legacy).\n");
            return 1;
        }
    }

    // --- حلقة المعالجة لكل ملف ---
    for (int i = 0; i < input_count; i++)
    {
        char *current_input = input_files[i];

        double file_t0 = 0.0;
        if (config.time_phases) file_t0 = get_time_seconds();

        if (config.verbose)
            printf("\n[INFO] Processing %s (%d/%d)...\n", current_input, i + 1, input_count);

        // 1. قراءة الملف
        double t0 = 0.0;
        if (config.time_phases) t0 = get_time_seconds();
        char *source = read_file(current_input);
        if (config.time_phases) phase_times.read_file_s += (get_time_seconds() - t0);

        // 2. التحليل اللفظي والقواعدي
        if (config.time_phases) t0 = get_time_seconds();
        Lexer lexer;
        lexer_init(&lexer, source, current_input);
        Node *ast = parse(&lexer);
        if (config.time_phases) phase_times.parse_s += (get_time_seconds() - t0);

        if (error_has_occurred())
        {
            fprintf(stderr, "Aborting %s due to syntax errors.\n", current_input);
            free(source);
            return 1;
        }

        // 3. التحليل الدلالي
        if (config.verbose)
            printf("[INFO] Running semantic analysis...\n");
        if (config.time_phases) t0 = get_time_seconds();
        if (!analyze(ast))
        {
            fprintf(stderr, "Aborting %s due to semantic errors.\n", current_input);
            free(source);
            return 1;
        }
        if (config.time_phases) phase_times.analyze_s += (get_time_seconds() - t0);

        // التحقق من التحذيرات كأخطاء
        if (g_warning_config.warnings_as_errors && warning_has_occurred())
        {
            fprintf(stderr, "Aborting %s: warnings treated as errors (-Werror).\n", current_input);
            free(source);
            return 1;
        }

        // ====================================================================
        // الخلفية القديمة (AST -> ASM) — Windows-only
        // ====================================================================
        if (config.backend == BAA_BACKEND_LEGACY)
        {
#if defined(_WIN32) && defined(BAA_ENABLE_LEGACY_CODEGEN)
            char *asm_file;
            if (config.assembly_only && input_count == 1 && config.output_file)
                asm_file = config.output_file;
            else
                asm_file = change_extension(current_input, ".s");

            FILE *f_asm = fopen(asm_file, "w");
            if (!f_asm)
            {
                printf("Error: Could not write assembly file '%s'\n", asm_file);
                free(source);
                return 1;
            }

            if (config.verbose)
                printf("[INFO] Running legacy codegen (AST->ASM)...\n");

            if (config.time_phases) t0 = get_time_seconds();
            codegen(ast, f_asm);
            if (config.time_phases) phase_times.emit_s += (get_time_seconds() - t0);
            fclose(f_asm);

            free(source);

            if (config.assembly_only)
            {
                if (config.verbose)
                    printf("[INFO] Generated assembly: %s\n", asm_file);
                if (config.time_phases) phase_times.total_s += (get_time_seconds() - file_t0);
                continue;
            }

            // التجميع (Assembly)
            char *obj_file;
            if (config.compile_only && input_count == 1 && config.output_file)
                obj_file = config.output_file;
            else
                obj_file = change_extension(current_input, ".o");

            char cmd_assemble[1024];
            {
                size_t cmd_len = 0;
                cmd_assemble[0] = '\0';

                if (!cmd_appendf(cmd_assemble, sizeof(cmd_assemble), &cmd_len,
                                 "%s%s -c %s -o %s",
                                 get_gcc_command(),
                                 config.debug_info ? " -g" : "",
                                 asm_file, obj_file))
                {
                    fprintf(stderr, "خطأ: أمر التجميع طويل جداً (تجاوز السعة).\n");
                    return 1;
                }
            }

            if (config.verbose)
                printf("[CMD] %s\n", cmd_assemble);
            double asm_t0 = 0.0;
            if (config.time_phases) asm_t0 = get_time_seconds();
            if (system(cmd_assemble) != 0)
            {
                printf("Error: Assembler failed for %s\n", current_input);
                return 1;
            }
            if (config.time_phases) phase_times.assemble_s += (get_time_seconds() - asm_t0);

            // تنظيف ملف التجميع المؤقت إذا لم يكن مطلوباً
            if (!config.assembly_only && !config.verbose)
            {
                remove(asm_file);
                if (asm_file != config.output_file)
                    free(asm_file);
            }

            obj_files_to_link[obj_count++] = obj_file;

            if (config.time_phases) phase_times.total_s += (get_time_seconds() - file_t0);
            continue;
#else
            fprintf(stderr, "خطأ: الخلفية القديمة (legacy) غير متاحة في هذا البناء.\n");
            free(source);
            return 1;
#endif
        }

        // 3.5. مرحلة IR (v0.3.0.7): AST → IR
        if (config.time_phases) t0 = get_time_seconds();
        IRModule *ir_module = ir_lower_program(ast, current_input);
        if (config.time_phases) phase_times.lower_ir_s += (get_time_seconds() - t0);
        if (!ir_module)
        {
            fprintf(stderr, "Aborting %s: internal IR lowering failure.\n", current_input);
            free(source);
            return 1;
        }

        // طباعة IR (اختياري) --dump-ir
        if (config.dump_ir)
        {
            if (config.verbose)
                printf("[INFO] Dumping IR (--dump-ir)...\n");
            ir_module_print(ir_module, stdout, 1);
        }

        // كتابة IR إلى ملف (اختياري) --emit-ir
        if (config.emit_ir)
        {
            char *ir_file = change_extension(current_input, ".ir");
            if (config.verbose)
                printf("[INFO] Writing IR (--emit-ir): %s\n", ir_file);
            ir_module_dump(ir_module, ir_file, 1);
            free(ir_file);
        }

        // 3.6. مرحلة التحسين (v0.3.1.6): Optimization Pipeline
        if (config.verify_gate && config.opt_level == OPT_LEVEL_0)
        {
            fprintf(stderr, "خطأ: --verify-gate يتطلب -O1 أو -O2 لأن بوابة التحقق تعمل داخل المُحسِّن.\n");
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        if (config.opt_level > OPT_LEVEL_0)
        {
            if (config.verbose)
                printf("[INFO] Running optimizer (-%s)...\n", ir_optimizer_level_name(config.opt_level));

            if (config.time_phases) t0 = get_time_seconds();

            if (config.verify_gate) {
                ir_optimizer_set_verify_gate(1);
            }

            if (!ir_optimizer_run(ir_module, config.opt_level))
            {
                fprintf(stderr, "Aborting %s: optimizer failed.\n", current_input);
                if (config.verify_gate) {
                    fprintf(stderr, "ملاحظة: قد يكون سبب الفشل هو بوابة التحقق (--verify-gate).\n");
                }
                ir_module_free(ir_module);
                free(source);
                return 1;
            }

            if (config.time_phases) phase_times.optimize_s += (get_time_seconds() - t0);

            if (config.verify_gate) {
                ir_optimizer_set_verify_gate(0);
            }
        }

        // طباعة IR بعد التحسين (اختياري) --dump-ir-opt
        if (config.dump_ir_opt)
        {
            if (config.verbose)
                printf("[INFO] Dumping optimized IR (--dump-ir-opt)...\n");
            ir_module_print(ir_module, stdout, 1);
        }

        // 3.6.24. التحقق من سلامة الـ IR (v0.3.2.6.5): قبل الخروج من SSA/الخلفية
        if (config.verify_ir)
        {
            if (config.verbose)
                printf("[INFO] Verifying IR well-formedness (--verify-ir)...\n");

            if (config.time_phases) t0 = get_time_seconds();

            if (!ir_module_verify_ir(ir_module, stderr))
            {
                fprintf(stderr, "فشل التحقق من سلامة الـ IR.\n");
                ir_module_free(ir_module);
                free(source);
                return 1;
            }

            if (config.time_phases) phase_times.verify_ir_s += (get_time_seconds() - t0);
        }

        // 3.6.25. التحقق من SSA (v0.3.2.5.3): بعد Mem2Reg وقبل الخروج من SSA
        if (config.verify_ssa)
        {
            if (config.opt_level == OPT_LEVEL_0)
            {
                fprintf(stderr, "خطأ: --verify-ssa يتطلب -O1 أو -O2 لأن SSA يُبنى عبر Mem2Reg داخل المُحسِّن.\n");
                ir_module_free(ir_module);
                free(source);
                return 1;
            }

            if (config.verbose)
                printf("[INFO] Verifying SSA (--verify-ssa)...\n");

            if (config.time_phases) t0 = get_time_seconds();
            if (!ir_module_verify_ssa(ir_module, stderr))
            {
                fprintf(stderr, "فشل التحقق من SSA.\n");
                ir_module_free(ir_module);
                free(source);
                return 1;
            }

            if (config.time_phases) phase_times.verify_ssa_s += (get_time_seconds() - t0);
        }

        // 3.6.5. الخروج من SSA (v0.3.2.5.2): إزالة فاي قبل الخلفية
        // هذه الخطوة ضرورية لأن الخلفية الحالية لا تُنفّذ IR_OP_PHI فعلياً.
        if (config.time_phases) t0 = get_time_seconds();
        (void)ir_outssa_run(ir_module);
        if (config.time_phases) phase_times.outssa_s += (get_time_seconds() - t0);

        // 3.6.5.1. فك الحلقات (v0.3.2.7.1) — اختياري
        // نطبقه بعد Out-of-SSA لأن القيم الحلقية تصبح صريحة عبر نسخ على الحواف.
        if (config.funroll_loops)
        {
            if (config.verbose)
                printf("[INFO] Unrolling loops (-funroll-loops)...\n");
            if (config.time_phases) t0 = get_time_seconds();
            (void)ir_unroll_run(ir_module, 8);
            if (config.time_phases) phase_times.unroll_s += (get_time_seconds() - t0);

            // إن كان --verify-ir مفعلاً، أعد التحقق لأننا عدّلنا الـ IR بعد مرحلة التحقق الأصلية.
            if (config.verify_ir)
            {
                if (config.verbose)
                    printf("[INFO] Re-verifying IR after unrolling (--verify-ir)...\n");
                if (!ir_module_verify_ir(ir_module, stderr))
                {
                    fprintf(stderr, "فشل التحقق من سلامة الـ IR بعد فك الحلقات.\n");
                    ir_module_free(ir_module);
                    free(source);
                    return 1;
                }
            }
        }

        // 4. اختيار التعليمات (Instruction Selection) (v0.3.2.1)
        if (config.verbose)
            printf("[INFO] Running instruction selection...\n");
        bool enable_tco = (config.opt_level >= OPT_LEVEL_2);
        if (config.time_phases) t0 = get_time_seconds();
        MachineModule *mach_module = isel_run_ex(ir_module, enable_tco, config.target);
        if (config.time_phases) phase_times.isel_s += (get_time_seconds() - t0);
        if (!mach_module)
        {
            fprintf(stderr, "Aborting %s: instruction selection failed.\n", current_input);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        // 5. تخصيص السجلات (Register Allocation) (v0.3.2.2)
        if (config.verbose)
            printf("[INFO] Running register allocation...\n");
        if (config.time_phases) t0 = get_time_seconds();
        if (!regalloc_run_ex(mach_module, config.target))
        {
            fprintf(stderr, "Aborting %s: register allocation failed.\n", current_input);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }
        if (config.time_phases) phase_times.regalloc_s += (get_time_seconds() - t0);

        // 6. إصدار كود التجميع (Code Emission) (v0.3.2.3)
        char *asm_file;
        if (config.assembly_only && input_count == 1 && config.output_file)
            asm_file = config.output_file;
        else
            asm_file = change_extension(current_input, ".s");

        FILE *f_asm = fopen(asm_file, "w");
        if (!f_asm)
        {
            printf("Error: Could not write assembly file '%s'\n", asm_file);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }

        if (config.verbose)
            printf("[INFO] Emitting assembly: %s\n", asm_file);

        // v0.3.2.8.3: حماية المكدس حالياً مدعومة فقط على ELF/Linux
        if (config.codegen_opts.stack_protector != BAA_STACKPROT_OFF)
        {
            if (!config.target || config.target->obj_format != BAA_OBJFORMAT_ELF)
            {
                fprintf(stderr, "خطأ: -fstack-protector مدعوم حالياً فقط لهدف ELF/Linux.\n");
                fclose(f_asm);
                mach_module_free(mach_module);
                ir_module_free(ir_module);
                free(source);
                return 1;
            }
        }

        if (config.time_phases) t0 = get_time_seconds();
        if (!emit_module_ex2(mach_module, f_asm, config.debug_info, config.target, config.codegen_opts))
        {
            fprintf(stderr, "Aborting %s: code emission failed.\n", current_input);
            fclose(f_asm);
            mach_module_free(mach_module);
            ir_module_free(ir_module);
            free(source);
            return 1;
        }
        if (config.time_phases) phase_times.emit_s += (get_time_seconds() - t0);
        fclose(f_asm);

        // تحرير الموارد
        mach_module_free(mach_module);

        if (config.time_phases)
        {
            IRArenaStats s = {0};
            ir_arena_get_stats(&ir_module->arena, &s);
            if (s.used_bytes > phase_times.ir_arena_used_max) phase_times.ir_arena_used_max = s.used_bytes;
            if (s.cap_bytes > phase_times.ir_arena_cap_max) phase_times.ir_arena_cap_max = s.cap_bytes;
            if (s.chunks > phase_times.ir_arena_chunks_max) phase_times.ir_arena_chunks_max = s.chunks;
        }

        ir_module_free(ir_module);
        free(source);

        if (config.time_phases) phase_times.total_s += (get_time_seconds() - file_t0);

        if (config.assembly_only)
        {
            if (config.verbose)
                printf("[INFO] Generated assembly: %s\n", asm_file);
            continue; // انتقل للملف التالي
        }

        // 5. التجميع (Assembly)
        char *obj_file;
        if (config.compile_only && input_count == 1 && config.output_file)
            obj_file = config.output_file;
        else
            obj_file = change_extension(current_input, ".o");

        char cmd_assemble[1024];
        {
            size_t cmd_len = 0;
            cmd_assemble[0] = '\0';

            if (!cmd_appendf(cmd_assemble, sizeof(cmd_assemble), &cmd_len,
                             "%s%s -c %s -o %s",
                             get_gcc_command(),
                             config.debug_info ? " -g" : "",
                             asm_file, obj_file))
            {
                fprintf(stderr, "خطأ: أمر التجميع طويل جداً (تجاوز السعة).\n");
                return 1;
            }
        }

        if (config.verbose)
            printf("[CMD] %s\n", cmd_assemble);
        double asm_t0 = 0.0;
        if (config.time_phases) asm_t0 = get_time_seconds();
        if (system(cmd_assemble) != 0)
        {
            printf("Error: Assembler failed for %s\n", current_input);
            return 1;
        }
        if (config.time_phases) phase_times.assemble_s += (get_time_seconds() - asm_t0);

        // تنظيف ملف التجميع المؤقت إذا لم يكن مطلوباً
        if (!config.assembly_only && !config.verbose)
        {
            remove(asm_file);
            if (asm_file != config.output_file)
                free(asm_file); // تحرير الاسم المولد
        }

        obj_files_to_link[obj_count++] = obj_file;
    }

    // إذا طلب المستخدم -S أو -c، نتوقف هنا
    if (config.assembly_only || config.compile_only)
    {
        if (config.time_phases)
        {
            double total = get_time_seconds() - config.start_time;
            fprintf(stderr,
                    "[TIME] read=%.6f parse=%.6f analyze=%.6f lower=%.6f opt=%.6f verify_ir=%.6f verify_ssa=%.6f outssa=%.6f unroll=%.6f isel=%.6f regalloc=%.6f emit=%.6f assemble=%.6f link=%.6f total=%.6f\n",
                    phase_times.read_file_s,
                    phase_times.parse_s,
                    phase_times.analyze_s,
                    phase_times.lower_ir_s,
                    phase_times.optimize_s,
                    phase_times.verify_ir_s,
                    phase_times.verify_ssa_s,
                    phase_times.outssa_s,
                    phase_times.unroll_s,
                    phase_times.isel_s,
                    phase_times.regalloc_s,
                    phase_times.emit_s,
                    phase_times.assemble_s,
                    phase_times.link_s,
                    total);
            fprintf(stderr,
                    "[MEM] ir_arena_used_max=%zu ir_arena_cap_max=%zu ir_arena_chunks_max=%zu\n",
                    phase_times.ir_arena_used_max,
                    phase_times.ir_arena_cap_max,
                    phase_times.ir_arena_chunks_max);
        }
        return 0;
    }

    // --- المرحلة النهائية: الربط (Linking) ---
    if (config.verbose)
        printf("\n[INFO] Linking %d object files...\n", obj_count);

    char cmd_link[4096];
    {
        size_t cmd_len = 0;
        cmd_link[0] = '\0';

        if (!cmd_append(cmd_link, sizeof(cmd_link), &cmd_len, get_gcc_command()))
        {
            fprintf(stderr, "خطأ: أمر الربط طويل جداً (تجاوز السعة).\n");
            return 1;
        }

        if (config.debug_info)
        {
            if (!cmd_append(cmd_link, sizeof(cmd_link), &cmd_len, " -g"))
            {
                fprintf(stderr, "خطأ: أمر الربط طويل جداً (تجاوز السعة).\n");
                return 1;
            }
        }

        // v0.3.2.8.3: PIE على ELF/Linux
        if (config.codegen_opts.pie)
        {
            if (config.target && config.target->obj_format == BAA_OBJFORMAT_ELF)
            {
                if (!cmd_append(cmd_link, sizeof(cmd_link), &cmd_len, " -pie"))
                {
                    fprintf(stderr, "خطأ: أمر الربط طويل جداً (تجاوز السعة).\n");
                    return 1;
                }
            }
        }

        for (int i = 0; i < obj_count; i++)
        {
            if (!cmd_appendf(cmd_link, sizeof(cmd_link), &cmd_len, " %s", obj_files_to_link[i]))
            {
                fprintf(stderr, "خطأ: أمر الربط طويل جداً (تجاوز السعة).\n");
                return 1;
            }
        }

        if (!cmd_appendf(cmd_link, sizeof(cmd_link), &cmd_len, " -o %s", config.output_file))
        {
            fprintf(stderr, "خطأ: أمر الربط طويل جداً (تجاوز السعة).\n");
            return 1;
        }
    }

    if (config.verbose)
        printf("[CMD] %s\n", cmd_link);
    double link_t0 = 0.0;
    if (config.time_phases) link_t0 = get_time_seconds();
    if (system(cmd_link) != 0)
    {
        printf("Error: Linker failed.\n");
        return 1;
    }
    if (config.time_phases) phase_times.link_s += (get_time_seconds() - link_t0);

    // تنظيف ملفات الكائنات المؤقتة
    if (!config.verbose)
    {
        for (int i = 0; i < obj_count; i++)
        {
            remove(obj_files_to_link[i]);
            // free(obj_files_to_link[i]); // تم تخصيصه بواسطة change_extension
        }
    }

    // ملخص التحذيرات
    int warn_count = warning_get_count();
    if (warn_count > 0 && config.verbose)
    {
        printf("[INFO] Compilation completed with %d warning(s).\n", warn_count);
    }

    // عرض وقت الترجمة
    if (config.verbose)
    {
        double elapsed = get_time_seconds() - config.start_time;
        printf("[INFO] Build successful: %s\n", config.output_file);
        printf("[INFO] Compilation time: %.3f seconds\n", elapsed);
    }

    if (config.time_phases)
    {
        double total = get_time_seconds() - config.start_time;
        fprintf(stderr,
                "[TIME] read=%.6f parse=%.6f analyze=%.6f lower=%.6f opt=%.6f verify_ir=%.6f verify_ssa=%.6f outssa=%.6f unroll=%.6f isel=%.6f regalloc=%.6f emit=%.6f assemble=%.6f link=%.6f total=%.6f\n",
                phase_times.read_file_s,
                phase_times.parse_s,
                phase_times.analyze_s,
                phase_times.lower_ir_s,
                phase_times.optimize_s,
                phase_times.verify_ir_s,
                phase_times.verify_ssa_s,
                phase_times.outssa_s,
                phase_times.unroll_s,
                phase_times.isel_s,
                phase_times.regalloc_s,
                phase_times.emit_s,
                phase_times.assemble_s,
                phase_times.link_s,
                total);
        fprintf(stderr,
                "[MEM] ir_arena_used_max=%zu ir_arena_cap_max=%zu ir_arena_chunks_max=%zu\n",
                phase_times.ir_arena_used_max,
                phase_times.ir_arena_cap_max,
                phase_times.ir_arena_chunks_max);
    }

    return 0;
}
