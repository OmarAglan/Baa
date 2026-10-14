/**
 * @file baa_frontend.c
 * @brief تنفيذ baa-frontend-api-v1: تشغيل أوضاع أدوات باء داخل العملية.
 *
 * يُبنى هذا الملف مع بقية المصرّف تحت baa_frontend_prefix.h، ثم يستعيد
 * هنا الأسماء الأصلية لأنه هو من يملك ما تعيد الترويسة توجيهه.
 */

#include "baa_frontend.h"
#include "baa_frontend_internal.h"
#include "../support/version.h"

#include <setjmp.h>

#undef printf
#undef vprintf
#undef puts
#undef putchar
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef _strdup
#undef exit
#pragma pop_macro("stdin")
#pragma pop_macro("stdout")
#pragma pop_macro("stderr")

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// ============================================================================
// متتبع التخصيصات
// ============================================================================

/*
 * مجموعة مؤشرات بعنونة مفتوحة: لا ترويسة قبل الكتلة، فيبقى free() على
 * مؤشر خصصته مكتبة C (realpath مثلاً) تحريراً عادياً بلا قراءة خارج حدوده.
 */
typedef struct FrontendPointerSet
{
    void **slots;
    size_t capacity;
    size_t count;
} FrontendPointerSet;

enum { FRONTEND_SET_MIN_CAPACITY = 1024, FRONTEND_SET_KEEP_CAPACITY = 65536 };

static FrontendPointerSet g_tracked = {NULL, 0, 0};

static size_t frontend_pointer_hash(const void *pointer, size_t capacity)
{
    uint64_t value = (uint64_t)(uintptr_t)pointer;
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    return (size_t)value & (capacity - 1u);
}

static bool frontend_set_place(void **slots, size_t capacity, void *pointer)
{
    size_t index = frontend_pointer_hash(pointer, capacity);
    while (slots[index])
    {
        if (slots[index] == pointer) return false;
        index = (index + 1u) & (capacity - 1u);
    }
    slots[index] = pointer;
    return true;
}

static bool frontend_set_grow(void)
{
    size_t capacity = g_tracked.capacity ? g_tracked.capacity * 2u
                                         : (size_t)FRONTEND_SET_MIN_CAPACITY;
    void **slots = (void **)calloc(capacity, sizeof(void *));
    if (!slots) return false;
    for (size_t i = 0; i < g_tracked.capacity; ++i)
    {
        if (g_tracked.slots[i]) (void)frontend_set_place(slots, capacity, g_tracked.slots[i]);
    }
    free(g_tracked.slots);
    g_tracked.slots = slots;
    g_tracked.capacity = capacity;
    return true;
}

static bool frontend_set_insert(void *pointer)
{
    if ((g_tracked.count + 1u) * 2u > g_tracked.capacity && !frontend_set_grow())
        return false;
    if (frontend_set_place(g_tracked.slots, g_tracked.capacity, pointer))
        g_tracked.count++;
    return true;
}

static bool frontend_set_remove(void *pointer)
{
    if (!g_tracked.count) return false;
    const size_t mask = g_tracked.capacity - 1u;
    size_t index = frontend_pointer_hash(pointer, g_tracked.capacity);
    while (g_tracked.slots[index] != pointer)
    {
        if (!g_tracked.slots[index]) return false;
        index = (index + 1u) & mask;
    }

    // حذف بالإزاحة للخلف حتى تبقى سلاسل البحث متصلة بلا شواهد.
    size_t hole = index;
    size_t next = (hole + 1u) & mask;
    while (g_tracked.slots[next])
    {
        const size_t home = frontend_pointer_hash(g_tracked.slots[next], g_tracked.capacity);
        const bool movable = (hole <= next) ? (home <= hole || home > next)
                                            : (home <= hole && home > next);
        if (movable)
        {
            g_tracked.slots[hole] = g_tracked.slots[next];
            hole = next;
        }
        next = (next + 1u) & mask;
    }
    g_tracked.slots[hole] = NULL;
    g_tracked.count--;
    return true;
}

static void frontend_set_sweep(void)
{
    for (size_t i = 0; i < g_tracked.capacity; ++i)
    {
        if (g_tracked.slots[i])
        {
            free(g_tracked.slots[i]);
            g_tracked.slots[i] = NULL;
        }
    }
    g_tracked.count = 0;
    if (g_tracked.capacity > (size_t)FRONTEND_SET_KEEP_CAPACITY)
    {
        free(g_tracked.slots);
        g_tracked.slots = NULL;
        g_tracked.capacity = 0;
    }
}

void *baa_frontend_track_malloc(size_t size)
{
    void *pointer = malloc(size ? size : 1u);
    if (pointer && !frontend_set_insert(pointer))
    {
        free(pointer);
        return NULL;
    }
    return pointer;
}

void *baa_frontend_track_calloc(size_t count, size_t size)
{
    void *pointer = calloc(count ? count : 1u, size ? size : 1u);
    if (pointer && !frontend_set_insert(pointer))
    {
        free(pointer);
        return NULL;
    }
    return pointer;
}

void *baa_frontend_track_realloc(void *pointer, size_t size)
{
    if (!pointer) return baa_frontend_track_malloc(size);
    if (!frontend_set_remove(pointer)) return realloc(pointer, size);

    void *resized = realloc(pointer, size ? size : 1u);
    if (!resized)
    {
        (void)frontend_set_insert(pointer);
        return NULL;
    }
    if (!frontend_set_insert(resized))
    {
        free(resized);
        return NULL;
    }
    return resized;
}

void baa_frontend_track_free(void *pointer)
{
    if (!pointer) return;
    (void)frontend_set_remove(pointer);
    free(pointer);
}

char *baa_frontend_track_strdup(const char *text)
{
    if (!text) return NULL;
    const size_t size = strlen(text) + 1u;
    char *copy = (char *)baa_frontend_track_malloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}

// ============================================================================
// القفل والمخارج
// ============================================================================

#ifdef _WIN32
static SRWLOCK g_frontend_lock = SRWLOCK_INIT;
static void frontend_lock(void) { AcquireSRWLockExclusive(&g_frontend_lock); }
static void frontend_unlock(void) { ReleaseSRWLockExclusive(&g_frontend_lock); }
#else
static pthread_mutex_t g_frontend_lock = PTHREAD_MUTEX_INITIALIZER;
static void frontend_lock(void) { (void)pthread_mutex_lock(&g_frontend_lock); }
static void frontend_unlock(void) { (void)pthread_mutex_unlock(&g_frontend_lock); }
#endif

/*
 * longjmp في MinGW-w64 يفك المكدس عبر SEH ويفشل بـ STATUS_BAD_STACK حين
 * يعبر إطارات المصرّف؛ نسخة GCC المدمجة تستعيد السجلات فقط، وهذا يكفي
 * لأن مصادر باء C لا تملك ما يحتاج فكاً. خارج ويندوز يبقى longjmp القياسي
 * حتى يرى AddressSanitizer القفزة وينظف المكدس المتروك.
 */
#if defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
typedef void *FrontendJumpBuffer[5];
#define FRONTEND_SETJMP(buffer) __builtin_setjmp(buffer)
#define FRONTEND_LONGJMP(buffer) __builtin_longjmp((buffer), 1)
#else
typedef jmp_buf FrontendJumpBuffer;
#define FRONTEND_SETJMP(buffer) setjmp(buffer)
#define FRONTEND_LONGJMP(buffer) longjmp((buffer), 1)
#endif

static bool g_in_call = false;
static int g_exit_code = 0;
static FrontendJumpBuffer g_exit_target;

void baa_frontend_exit(int code)
{
    if (!g_in_call) exit(code);
    // ما كان معالج atexit يكتبه للعملية المنفصلة يُكتب هنا قبل فك المكدس.
    baa_driver_on_frontend_exit();
    g_exit_code = code;
    FRONTEND_LONGJMP(g_exit_target);
}

// ============================================================================
// الدخل والمخرجات
// ============================================================================

enum { FRONTEND_STDIN = 0, FRONTEND_STDOUT = 1, FRONTEND_STDERR = 2 };
static FILE *g_streams[3] = {NULL, NULL, NULL};

FILE *baa_frontend_io_stdin(void)
{
    return g_in_call ? g_streams[FRONTEND_STDIN] : stdin;
}

FILE *baa_frontend_io_stdout(void)
{
    return g_in_call ? g_streams[FRONTEND_STDOUT] : stdout;
}

FILE *baa_frontend_io_stderr(void)
{
    return g_in_call ? g_streams[FRONTEND_STDERR] : stderr;
}

static FILE *frontend_open_temp(void)
{
#ifdef _WIN32
    // tmpfile() في msvcrt ينشئ في جذر القرص، فيُفتح ملف في مجلد المؤقتات
    // يُحذف عند إغلاقه أو انتهاء العملية.
    wchar_t directory[MAX_PATH + 1];
    DWORD length = GetTempPathW(MAX_PATH + 1, directory);
    if (length == 0 || length > MAX_PATH) return NULL;
    wchar_t path[MAX_PATH];
    if (!GetTempFileNameW(directory, L"baa", 0, path)) return NULL;
    FILE *file = _wfopen(path, L"w+bTD");
    if (!file) (void)DeleteFileW(path);
    return file;
#else
    return tmpfile();
#endif
}

static bool frontend_stream_reset(FILE *file, bool text)
{
    if (fflush(file) != 0) return false;
    rewind(file);
    clearerr(file);
#ifdef _WIN32
    // مخرجات baa القياسية نصية في ويندوز (LF يصير CRLF) ما لم يجعلها وضع
    // ثنائية بنفسه؛ الملف المؤقت يبدأ كل نداء بالحالة نفسها فتتطابق البايتات.
    if (_setmode(_fileno(file), text ? _O_TEXT : _O_BINARY) == -1) return false;
    return _chsize(_fileno(file), 0) == 0;
#else
    (void)text;
    return ftruncate(fileno(file), 0) == 0;
#endif
}

static bool frontend_streams_prepare(const char *input, size_t input_size)
{
    for (int i = 0; i < 3; ++i)
    {
        if (!g_streams[i]) g_streams[i] = frontend_open_temp();
        if (!g_streams[i] || !frontend_stream_reset(g_streams[i], i != FRONTEND_STDIN))
            return false;
    }
    FILE *in = g_streams[FRONTEND_STDIN];
    if (input_size && fwrite(input, 1u, input_size, in) != input_size) return false;
    if (fflush(in) != 0) return false;
    rewind(in);
    return true;
}

static bool frontend_stream_take(FILE *file, char **out, size_t *out_size)
{
    *out = NULL;
    *out_size = 0;
    if (fflush(file) != 0) return false;
#ifdef _WIN32
    if (_setmode(_fileno(file), _O_BINARY) == -1) return false;
#endif
    if (fseek(file, 0, SEEK_END) != 0) return false;
    long size = ftell(file);
    if (size < 0) return false;
    rewind(file);
    char *buffer = (char *)malloc((size_t)size + 1u);
    if (!buffer) return false;
    if (size > 0 && fread(buffer, 1u, (size_t)size, file) != (size_t)size)
    {
        free(buffer);
        return false;
    }
    buffer[size] = '\0';
    *out = buffer;
    *out_size = (size_t)size;
    return true;
}

// ============================================================================
// المسارات ومجلد العمل
// ============================================================================

static char *frontend_parent_directory(const char *path)
{
    if (!path) return NULL;
    const char *separator = NULL;
    for (const char *p = path; *p; ++p)
    {
        if (*p == '/' || *p == '\\') separator = p;
    }
    if (!separator) return NULL;
    size_t length = (size_t)(separator - path);
    if (length == 0) length = 1u;
    char *directory = (char *)malloc(length + 1u);
    if (!directory) return NULL;
    memcpy(directory, path, length);
    directory[length] = '\0';
    return directory;
}

static char *frontend_process_directory(void)
{
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return NULL;
    int needed = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) return NULL;
    char *utf8 = (char *)malloc((size_t)needed);
    if (!utf8) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, needed, NULL, NULL) <= 0)
    {
        free(utf8);
        return NULL;
    }
    char *directory = frontend_parent_directory(utf8);
    free(utf8);
    return directory;
#else
    char path[4096];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1u);
    if (length <= 0) return NULL;
    path[length] = '\0';
    return frontend_parent_directory(path);
#endif
}

#ifdef _WIN32
typedef wchar_t FrontendNativeChar;

static wchar_t *frontend_wide_from_utf8(const char *text)
{
    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (needed <= 0) return NULL;
    wchar_t *wide = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
    if (!wide) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, needed) <= 0)
    {
        free(wide);
        return NULL;
    }
    return wide;
}
#else
typedef char FrontendNativeChar;
#endif

/** @brief الانتقال إلى مجلد العمل وحفظ السابق؛ NULL في *saved يعني لا تغيير. */
static bool frontend_enter_directory(const char *directory, FrontendNativeChar **saved)
{
    *saved = NULL;
    if (!directory || !directory[0]) return true;
#ifdef _WIN32
    wchar_t *target = frontend_wide_from_utf8(directory);
    if (!target) return false;
    wchar_t *previous = _wgetcwd(NULL, 0);
    if (!previous || _wchdir(target) != 0)
    {
        free(previous);
        free(target);
        return false;
    }
    free(target);
#else
    char *previous = getcwd(NULL, 0);
    if (!previous || chdir(directory) != 0)
    {
        free(previous);
        return false;
    }
#endif
    *saved = previous;
    return true;
}

static void frontend_leave_directory(FrontendNativeChar *saved)
{
    if (!saved) return;
#ifdef _WIN32
    (void)_wchdir(saved);
#else
    // لا بديل إن تعذرت العودة؛ النداء التالي يحدد مجلده من جديد.
    if (chdir(saved) != 0) errno = 0;
#endif
    free(saved);
}

// ============================================================================
// الواجهة العامة
// ============================================================================

const char *baa_frontend_contract(void)
{
    return BAA_FRONTEND_CONTRACT;
}

const char *baa_frontend_compiler_version(void)
{
    return BAA_VERSION;
}

void baa_frontend_result_free(BaaFrontendResult *result)
{
    if (!result) return;
    free(result->standard_output);
    free(result->standard_error);
    memset(result, 0, sizeof(*result));
}

static int g_call_argc = 0;
static char **g_call_argv = NULL;

/*
 * setjmp في إطار صغير لا يحمل متغيرات محلية عبره: الوسائط تمر بمتغيرات
 * ساكنة، فلا شيء يمكن أن يفسده longjmp من baa_frontend_exit.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int frontend_invoke_driver(void)
{
    if (FRONTEND_SETJMP(g_exit_target) != 0) return g_exit_code;
    return baa_driver_main(g_call_argc, g_call_argv);
}

static void frontend_release_call_state(void)
{
    analysis_forget_state();
    parser_forget_state();
    error_forget_state();
    ir_forget_state();
    frontend_set_sweep();
}

BaaFrontendStatus baa_frontend_run(const BaaFrontendRequest *request,
                                   BaaFrontendResult *result)
{
    if (result) memset(result, 0, sizeof(*result));
    if (!request || !result) return BAA_FRONTEND_INVALID_REQUEST;
    if (request->argument_count > 0 && !request->arguments)
        return BAA_FRONTEND_INVALID_REQUEST;
    if (request->standard_input_size > 0 && !request->standard_input)
        return BAA_FRONTEND_INVALID_REQUEST;
    if (request->argument_count >= (size_t)INT_MAX)
        return BAA_FRONTEND_INVALID_REQUEST;
    for (size_t i = 0; i < request->argument_count; ++i)
    {
        if (!request->arguments[i]) return BAA_FRONTEND_INVALID_REQUEST;
    }

    const int argc = (int)request->argument_count + 1;
    char **argv = (char **)calloc((size_t)argc + 1u, sizeof(char *));
    if (!argv) return BAA_FRONTEND_OUT_OF_MEMORY;
    argv[0] = (char *)"baa";
    for (size_t i = 0; i < request->argument_count; ++i)
        argv[i + 1] = (char *)request->arguments[i];

    char *executable_directory = request->executable_path
        ? frontend_parent_directory(request->executable_path)
        : frontend_process_directory();

    frontend_lock();

    BaaFrontendStatus status = BAA_FRONTEND_OK;
    FrontendNativeChar *saved_directory = NULL;
    if (!frontend_streams_prepare(request->standard_input,
                                  request->standard_input_size))
    {
        status = BAA_FRONTEND_IO_ERROR;
    }
    else if (!frontend_enter_directory(request->working_directory, &saved_directory))
    {
        status = BAA_FRONTEND_INVALID_REQUEST;
    }

    if (status == BAA_FRONTEND_OK)
    {
        driver_toolchain_set_frontend_executable_dir(executable_directory);
        g_call_argc = argc;
        g_call_argv = argv;
        g_in_call = true;
        const int exit_code = frontend_invoke_driver();
        fflush(g_streams[FRONTEND_STDOUT]);
        fflush(g_streams[FRONTEND_STDERR]);
        g_in_call = false;
        g_call_argc = 0;
        g_call_argv = NULL;
        driver_toolchain_set_frontend_executable_dir(NULL);
        frontend_leave_directory(saved_directory);
        frontend_release_call_state();

        result->exit_code = exit_code;
        if (!frontend_stream_take(g_streams[FRONTEND_STDOUT],
                                  &result->standard_output,
                                  &result->standard_output_size) ||
            !frontend_stream_take(g_streams[FRONTEND_STDERR],
                                  &result->standard_error,
                                  &result->standard_error_size))
        {
            baa_frontend_result_free(result);
            status = BAA_FRONTEND_IO_ERROR;
        }
    }

    frontend_unlock();
    free(executable_directory);
    free(argv);
    return status;
}
