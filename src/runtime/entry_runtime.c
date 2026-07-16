#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

extern int64_t baa_arabic_program_entry(int64_t argc, char** argv)
    __asm__("الرئيسية");

static char* baa_entry_utf8_from_wide(const wchar_t* value)
{
    if (!value) return NULL;
    int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                                     NULL, 0, NULL, NULL);
    if (needed <= 0) return NULL;
    char* result = (char*)malloc((size_t)needed);
    if (!result) return NULL;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            result, needed, NULL, NULL) <= 0)
    {
        free(result);
        return NULL;
    }
    return result;
}

__attribute__((noreturn)) void baa_windows_arabic_entry(void)
    __asm__("بدء_ويندوز");

__attribute__((noreturn)) void baa_windows_arabic_entry(void)
{
    int argc = 0;
    wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wide_argv || argc < 0) ExitProcess(5u);

    char** argv = (char**)calloc((size_t)argc + 1u, sizeof(char*));
    if (!argv)
    {
        LocalFree(wide_argv);
        ExitProcess(5u);
    }

    for (int i = 0; i < argc; ++i)
    {
        argv[i] = baa_entry_utf8_from_wide(wide_argv[i]);
        if (!argv[i])
        {
            for (int j = 0; j < i; ++j) free(argv[j]);
            free(argv);
            LocalFree(wide_argv);
            ExitProcess(5u);
        }
    }
    LocalFree(wide_argv);

    int64_t result = baa_arabic_program_entry((int64_t)argc, argv);
    for (int i = 0; i < argc; ++i) free(argv[i]);
    free(argv);
    ExitProcess((UINT)result);
}

__attribute__((noreturn)) void baa_windows_arabic_start(void)
    __asm__("الرئيسية_بدء");

__attribute__((noreturn)) void baa_windows_arabic_start(void)
{
    baa_windows_arabic_entry();
}
#else
typedef int (*BaaHostedMain)(int, char **, char **);
typedef void (*BaaHostedHook)(void);

extern int __libc_start_main(BaaHostedMain main_function,
                             int argc,
                             char **argv,
                             BaaHostedHook init,
                             BaaHostedHook fini,
                             BaaHostedHook runtime_loader_fini,
                             void *stack_end);

int baa_linux_hosted_start(BaaHostedMain main_function,
                           int argc,
                           char **argv,
                           BaaHostedHook init,
                           BaaHostedHook fini,
                           BaaHostedHook runtime_loader_fini,
                           void *stack_end)
    __asm__("ابدأ_المكتبة_المستضافة");

int baa_linux_hosted_start(BaaHostedMain main_function,
                           int argc,
                           char **argv,
                           BaaHostedHook init,
                           BaaHostedHook fini,
                           BaaHostedHook runtime_loader_fini,
                           void *stack_end)
{
    return __libc_start_main(main_function,
                             argc,
                             argv,
                             init,
                             fini,
                             runtime_loader_fini,
                             stack_end);
}
#endif
