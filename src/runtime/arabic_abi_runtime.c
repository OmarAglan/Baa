/**
 * @file arabic_abi_runtime.c
 * @brief Arabic link-visible adapters for compiler-owned platform ABIs.
 *
 * Generated Nazm objects reference only Arabic symbols.  These narrow
 * adapters contain the platform spellings inside the runtime boundary.
 */

#include "process_runtime.h"

#include "../support/file_io.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BAA_ABI_NAME(symbol) __asm__(symbol)

int baa_abi_printf(const char *format, ...) BAA_ABI_NAME("اطبع_منسقا");
int baa_abi_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

int baa_abi_snprintf(char *buffer, size_t size, const char *format, ...)
    BAA_ABI_NAME("نسق_في_مخزن");
int baa_abi_snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int baa_abi_scanf(const char *format, ...) BAA_ABI_NAME("اقرأ_منسقا");
int baa_abi_scanf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vscanf(format, args);
    va_end(args);
    return result;
}

int baa_abi_getchar(void) BAA_ABI_NAME("اقرأ_محرف_سي");
int baa_abi_getchar(void) { return getchar(); }
int baa_abi_puts(const char *value) BAA_ABI_NAME("اطبع_سطر_سي");
int baa_abi_puts(const char *value) { return puts(value); }
size_t baa_abi_strlen(const char *value) BAA_ABI_NAME("طول_سلسلة_سي");
size_t baa_abi_strlen(const char *value) { return strlen(value); }

void *baa_abi_malloc(size_t size) BAA_ABI_NAME("ذاكرة_احجز");
void *baa_abi_malloc(size_t size) { return malloc(size); }
void *baa_abi_calloc(size_t count, size_t size)
    BAA_ABI_NAME("ذاكرة_احجز_مصفرة");
void *baa_abi_calloc(size_t count, size_t size) { return calloc(count, size); }
void *baa_abi_realloc(void *memory, size_t size)
    BAA_ABI_NAME("ذاكرة_اعد_الحجز");
void *baa_abi_realloc(void *memory, size_t size) { return realloc(memory, size); }
void baa_abi_free(void *memory) BAA_ABI_NAME("ذاكرة_حرر");
void baa_abi_free(void *memory) { free(memory); }
void *baa_abi_memcpy(void *destination, const void *source, size_t size)
    BAA_ABI_NAME("ذاكرة_انسخ");
void *baa_abi_memcpy(void *destination, const void *source, size_t size)
{
    return memcpy(destination, source, size);
}
void *baa_abi_memset(void *destination, int value, size_t size)
    BAA_ABI_NAME("ذاكرة_املأ");
void *baa_abi_memset(void *destination, int value, size_t size)
{
    return memset(destination, value, size);
}

double baa_abi_sqrt(double value) BAA_ABI_NAME("جذر_تربيعي_سي");
double baa_abi_sqrt(double value) { return sqrt(value); }
double baa_abi_pow(double base, double exponent) BAA_ABI_NAME("اس_سي");
double baa_abi_pow(double base, double exponent) { return pow(base, exponent); }
double baa_abi_sin(double value) BAA_ABI_NAME("جيب_سي");
double baa_abi_sin(double value) { return sin(value); }
double baa_abi_cos(double value) BAA_ABI_NAME("جيب_تمام_سي");
double baa_abi_cos(double value) { return cos(value); }
double baa_abi_tan(double value) BAA_ABI_NAME("ظل_سي");
double baa_abi_tan(double value) { return tan(value); }
long long baa_abi_llabs(long long value) BAA_ABI_NAME("مطلق_صحيح_سي");
long long baa_abi_llabs(long long value) { return llabs(value); }
int baa_abi_rand(void) BAA_ABI_NAME("عشوائي_سي");
int baa_abi_rand(void) { return rand(); }

char *baa_abi_getenv(const char *name) BAA_ABI_NAME("متغير_بيئة_سي");
char *baa_abi_getenv(const char *name) { return getenv(name); }
int baa_abi_system(const char *command) BAA_ABI_NAME("نفذ_نظام_سي");
int baa_abi_system(const char *command) { return system(command); }
int64_t baa_abi_time(int64_t *destination) BAA_ABI_NAME("وقت_حالي_سي");
int64_t baa_abi_time(int64_t *destination)
{
    time_t value = time(NULL);
    if (destination) *destination = (int64_t)value;
    return (int64_t)value;
}
char *baa_abi_ctime(const int64_t *source) BAA_ABI_NAME("وقت_كنص_سي");
char *baa_abi_ctime(const int64_t *source)
{
    if (!source) return NULL;
    time_t value = (time_t)*source;
    return ctime(&value);
}

FILE *baa_abi_fopen_utf8(const char *path, const char *mode)
    BAA_ABI_NAME("افتح_ملف_بترميز_موحد");
FILE *baa_abi_fopen_utf8(const char *path, const char *mode)
{
    return baa_fopen_utf8(path, mode);
}
int baa_abi_fclose(FILE *file) BAA_ABI_NAME("اغلق_ملف_سي");
int baa_abi_fclose(FILE *file) { return fclose(file); }
int baa_abi_fgetc(FILE *file) BAA_ABI_NAME("اقرأ_محرف_ملف_سي");
int baa_abi_fgetc(FILE *file) { return fgetc(file); }
int baa_abi_fputc(int value, FILE *file) BAA_ABI_NAME("اكتب_محرف_ملف_سي");
int baa_abi_fputc(int value, FILE *file) { return fputc(value, file); }
size_t baa_abi_fread(void *buffer, size_t size, size_t count, FILE *file)
    BAA_ABI_NAME("اقرأ_كتلة_ملف_سي");
size_t baa_abi_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    return fread(buffer, size, count, file);
}
size_t baa_abi_fwrite(const void *buffer, size_t size, size_t count, FILE *file)
    BAA_ABI_NAME("اكتب_كتلة_ملف_سي");
size_t baa_abi_fwrite(const void *buffer, size_t size, size_t count, FILE *file)
{
    return fwrite(buffer, size, count, file);
}
int baa_abi_feof(FILE *file) BAA_ABI_NAME("نهاية_ملف_سي");
int baa_abi_feof(FILE *file) { return feof(file); }
int64_t baa_abi_ftell(FILE *file) BAA_ABI_NAME("موقع_ملف_سي");
int64_t baa_abi_ftell(FILE *file)
{
#ifdef _WIN32
    return (int64_t)_ftelli64(file);
#else
    return (int64_t)ftello(file);
#endif
}
int baa_abi_fseek(FILE *file, int64_t offset, int origin)
    BAA_ABI_NAME("اذهب_لموقع_ملف_سي");
int baa_abi_fseek(FILE *file, int64_t offset, int origin)
{
#ifdef _WIN32
    return _fseeki64(file, offset, origin);
#else
    return fseeko(file, (off_t)offset, origin);
#endif
}
int baa_abi_fputs(const char *value, FILE *file)
    BAA_ABI_NAME("اكتب_سلسلة_ملف_سي");
int baa_abi_fputs(const char *value, FILE *file) { return fputs(value, file); }

void baa_abi_exit(int code) BAA_ABI_NAME("انه_العملية_سي");
void baa_abi_exit(int code) { exit(code); }
char *baa_abi_strerror(int code) BAA_ABI_NAME("نص_خطأ_النظام_سي");
char *baa_abi_strerror(int code) { return strerror(code); }
int *baa_abi_errno(void) BAA_ABI_NAME("موقع_خطأ_النظام_سي");
int *baa_abi_errno(void) { return &errno; }

void *baa_abi_process_start(const BaaRuntimeChar *const *argv,
                            int64_t argc,
                            const BaaRuntimeChar *cwd,
                            const BaaRuntimeChar *const *environment,
                            int64_t environment_count,
                            const BaaRuntimeChar *stdout_path,
                            const BaaRuntimeChar *stderr_path)
    BAA_ABI_NAME("وقت_تشغيل_ابدأ_عملية");
void *baa_abi_process_start(const BaaRuntimeChar *const *argv,
                            int64_t argc,
                            const BaaRuntimeChar *cwd,
                            const BaaRuntimeChar *const *environment,
                            int64_t environment_count,
                            const BaaRuntimeChar *stdout_path,
                            const BaaRuntimeChar *stderr_path)
{
    return baa_runtime_process_start(argv, argc, cwd, environment,
                                     environment_count, stdout_path,
                                     stderr_path);
}
int64_t baa_abi_process_poll(void *handle)
    BAA_ABI_NAME("وقت_تشغيل_حالة_عملية");
int64_t baa_abi_process_poll(void *handle) { return baa_runtime_process_poll(handle); }
int64_t baa_abi_process_wait(void *handle)
    BAA_ABI_NAME("وقت_تشغيل_انتظر_عملية");
int64_t baa_abi_process_wait(void *handle) { return baa_runtime_process_wait(handle); }
int64_t baa_abi_process_cancel(void *handle)
    BAA_ABI_NAME("وقت_تشغيل_الغ_عملية");
int64_t baa_abi_process_cancel(void *handle) { return baa_runtime_process_cancel(handle); }
int64_t baa_abi_process_exit_code(void *handle)
    BAA_ABI_NAME("وقت_تشغيل_كود_خروج_عملية");
int64_t baa_abi_process_exit_code(void *handle)
{
    return baa_runtime_process_exit_code(handle);
}
void baa_abi_process_free(void *handle)
    BAA_ABI_NAME("وقت_تشغيل_حرر_عملية");
void baa_abi_process_free(void *handle) { baa_runtime_process_free(handle); }
int64_t baa_abi_make_dirs(const BaaRuntimeChar *path)
    BAA_ABI_NAME("وقت_تشغيل_انشئ_مجلدات");
int64_t baa_abi_make_dirs(const BaaRuntimeChar *path)
{
    return baa_runtime_make_dirs(path);
}
int64_t baa_abi_remove_tree(const BaaRuntimeChar *path)
    BAA_ABI_NAME("وقت_تشغيل_احذف_شجرة");
int64_t baa_abi_remove_tree(const BaaRuntimeChar *path)
{
    return baa_runtime_remove_tree(path);
}
BaaRuntimeChar *baa_abi_sha256_file(const BaaRuntimeChar *path)
    BAA_ABI_NAME("وقت_تشغيل_تجزئة_ملف");
BaaRuntimeChar *baa_abi_sha256_file(const BaaRuntimeChar *path)
{
    return baa_runtime_sha256_file(path);
}
