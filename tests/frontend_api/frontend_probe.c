/**
 * @file frontend_probe.c
 * @brief مسبار baa-frontend-api-v1: يشغل نداء أدوات داخل العملية بوسائط baa.
 *
 * الاستعمال:
 *   baa_frontend_probe [--repeat N] [--executable PATH] [--cwd DIR]
 *                      [--memory-report FILE] -- <وسائط baa...>
 *
 * يقرأ الدخل القياسي كاملاً ويمرره للنداء، ثم يكتب مخرجات النداء الأول
 * ورمز خروجه كما لو كان baa نفسه. مع --repeat يعيد النداء N مرة ويخرج
 * بالرمز 99 إن اختلفت أي نتيجة عن الأولى، ومع --memory-report يكتب حجم
 * ذاكرة العملية بعد النداء الأول وبعد الأخير.
 */

#include "baa_frontend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

static unsigned long long probe_resident_kib(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return 0;
    return (unsigned long long)counters.WorkingSetSize / 1024u;
#else
    FILE *statm = fopen("/proc/self/statm", "r");
    if (!statm) return 0;
    unsigned long long size = 0, resident = 0;
    int read = fscanf(statm, "%llu %llu", &size, &resident);
    fclose(statm);
    if (read != 2) return 0;
    return resident * (unsigned long long)sysconf(_SC_PAGESIZE) / 1024u;
#endif
}

static char *probe_read_stdin(size_t *out_size)
{
    size_t capacity = 65536u, size = 0;
    char *buffer = (char *)malloc(capacity);
    if (!buffer) return NULL;
    for (;;)
    {
        if (size == capacity)
        {
            char *grown = (char *)realloc(buffer, capacity * 2u);
            if (!grown)
            {
                free(buffer);
                return NULL;
            }
            buffer = grown;
            capacity *= 2u;
        }
        size_t got = fread(buffer + size, 1u, capacity - size, stdin);
        size += got;
        if (got == 0) break;
    }
    *out_size = size;
    return buffer;
}

static int probe_same(const BaaFrontendResult *a, const BaaFrontendResult *b)
{
    return a->exit_code == b->exit_code &&
           a->standard_output_size == b->standard_output_size &&
           a->standard_error_size == b->standard_error_size &&
           memcmp(a->standard_output, b->standard_output, a->standard_output_size) == 0 &&
           memcmp(a->standard_error, b->standard_error, a->standard_error_size) == 0;
}

static int probe_main(int argc, char **argv)
{
    long repeat = 1;
    const char *executable = NULL;
    const char *directory = NULL;
    const char *memory_report = NULL;
    int index = 1;
    for (; index < argc; ++index)
    {
        if (strcmp(argv[index], "--") == 0)
        {
            ++index;
            break;
        }
        if (index + 1 >= argc)
        {
            fprintf(stderr, "probe: missing value for %s\n", argv[index]);
            return 98;
        }
        if (strcmp(argv[index], "--repeat") == 0) repeat = strtol(argv[++index], NULL, 10);
        else if (strcmp(argv[index], "--executable") == 0) executable = argv[++index];
        else if (strcmp(argv[index], "--cwd") == 0) directory = argv[++index];
        else if (strcmp(argv[index], "--memory-report") == 0) memory_report = argv[++index];
        else
        {
            fprintf(stderr, "probe: unknown option %s\n", argv[index]);
            return 98;
        }
    }
    if (repeat < 1) repeat = 1;

    size_t input_size = 0;
    char *input = probe_read_stdin(&input_size);
    if (!input)
    {
        fprintf(stderr, "probe: cannot read stdin\n");
        return 98;
    }

    BaaFrontendRequest request = {
        executable,
        directory,
        (const char *const *)(argv + index),
        (size_t)(argc - index),
        input,
        input_size,
    };

    BaaFrontendResult first;
    if (baa_frontend_run(&request, &first) != BAA_FRONTEND_OK)
    {
        fprintf(stderr, "probe: baa_frontend_run failed\n");
        free(input);
        return 97;
    }
    unsigned long long first_kib = probe_resident_kib();

    int differs = 0;
    for (long run = 1; run < repeat && !differs; ++run)
    {
        BaaFrontendResult next;
        if (baa_frontend_run(&request, &next) != BAA_FRONTEND_OK)
        {
            fprintf(stderr, "probe: baa_frontend_run failed on run %ld\n", run);
            baa_frontend_result_free(&first);
            free(input);
            return 97;
        }
        if (!probe_same(&first, &next))
        {
            fprintf(stderr, "probe: run %ld differs from the first run\n", run);
            differs = 1;
        }
        baa_frontend_result_free(&next);
    }
    unsigned long long last_kib = probe_resident_kib();

    if (memory_report)
    {
        FILE *report = fopen(memory_report, "w");
        if (report)
        {
            fprintf(report, "%llu %llu\n", first_kib, last_kib);
            fclose(report);
        }
    }

    fwrite(first.standard_output, 1u, first.standard_output_size, stdout);
    fwrite(first.standard_error, 1u, first.standard_error_size, stderr);
    fflush(stdout);
    fflush(stderr);
    int code = differs ? 99 : first.exit_code;
    baa_frontend_result_free(&first);
    free(input);
    return code;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv)
{
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    char **argv = (char **)calloc((size_t)argc + 1u, sizeof(char *));
    if (!argv) return 98;
    for (int i = 0; i < argc; ++i)
    {
        int needed = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = (char *)malloc(needed > 0 ? (size_t)needed : 1u);
        if (!argv[i]) return 98;
        if (WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, argv[i], needed, NULL, NULL) <= 0)
            argv[i][0] = '\0';
    }
    int code = probe_main(argc, argv);
    for (int i = 0; i < argc; ++i) free(argv[i]);
    free(argv);
    return code;
}
#else
int main(int argc, char **argv)
{
    return probe_main(argc, argv);
}
#endif
