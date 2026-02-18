/**
 * @file process.c
 * @brief تشغيل عمليات خارجية (بديل system()) مع دعم إعادة توجيه المخرجات.
 * @version 0.3.4
 */

#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static void result_init(BaaProcessResult* r)
{
    if (!r) return;
    r->exit_code = -1;
    r->started = false;
}

#ifdef _WIN32

static bool win_is_path(const char* s)
{
    if (!s) return false;
    return strchr(s, '\\') != NULL || strchr(s, '/') != NULL;
}

static bool win_build_cmdline(char* out, size_t out_cap, const char* const* argv)
{
    if (!out || out_cap == 0 || !argv || !argv[0]) return false;
    out[0] = '\0';

    size_t len = 0;

    for (size_t i = 0; argv[i]; i++)
    {
        const char* a = argv[i];
        if (!a) a = "";

        if (i != 0)
        {
            if (len + 1 >= out_cap) return false;
            out[len++] = ' ';
            out[len] = '\0';
        }

        // خوارزمية اقتباس مبسطة متوافقة مع CreateProcess: نقتبس إذا وُجدت مسافة/تبويب/علامة اقتباس.
        bool need_quote = false;
        for (const char* p = a; *p; p++)
        {
            if (*p == ' ' || *p == '\t' || *p == '"') { need_quote = true; break; }
        }

        if (!need_quote)
        {
            size_t alen = strlen(a);
            if (alen > out_cap - 1 - len) return false;
            memcpy(out + len, a, alen);
            len += alen;
            out[len] = '\0';
            continue;
        }

        if (len + 1 >= out_cap) return false;
        out[len++] = '"';
        out[len] = '\0';

        // عند وجود backslashes قبل اقتباس، يجب مضاعفتها. نطبّق القاعدة الأساسية.
        size_t bs = 0;
        for (const char* p = a; *p; p++)
        {
            if (*p == '\\')
            {
                bs++;
                continue;
            }
            if (*p == '"')
            {
                // اكتب backslashes مضاعفة + backslash للهروب ثم الاقتباس.
                for (size_t k = 0; k < bs * 2 + 1; k++)
                {
                    if (len + 1 >= out_cap) return false;
                    out[len++] = '\\';
                }
                bs = 0;
                if (len + 1 >= out_cap) return false;
                out[len++] = '"';
                out[len] = '\0';
                continue;
            }

            // اكتب backslashes كما هي.
            for (size_t k = 0; k < bs; k++)
            {
                if (len + 1 >= out_cap) return false;
                out[len++] = '\\';
            }
            bs = 0;

            if (len + 1 >= out_cap) return false;
            out[len++] = *p;
            out[len] = '\0';
        }

        // backslashes في نهاية الوسيط قبل إغلاق الاقتباس يجب مضاعفتها.
        for (size_t k = 0; k < bs * 2; k++)
        {
            if (len + 1 >= out_cap) return false;
            out[len++] = '\\';
        }

        if (len + 1 >= out_cap) return false;
        out[len++] = '"';
        out[len] = '\0';
    }

    return true;
}

static bool win_run_createprocess(const char* const* argv,
                                  const char* cwd,
                                  const char* stdout_path,
                                  const char* stderr_path,
                                  BaaProcessResult* out_result)
{
    result_init(out_result);
    if (!argv || !argv[0]) return false;

    char cmdline[8192];
    if (!win_build_cmdline(cmdline, sizeof(cmdline), argv))
        return false;

    BOOL inherit = (stdout_path || stderr_path) ? TRUE : FALSE;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = inherit;

    HANDLE h_out = NULL;
    HANDLE h_err = NULL;
    bool same_file = false;

    if (stdout_path && stderr_path && strcmp(stdout_path, stderr_path) == 0)
        same_file = true;

    if (inherit && stdout_path)
    {
        h_out = CreateFileA(stdout_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_out == INVALID_HANDLE_VALUE) return false;
    }
    if (inherit && stderr_path)
    {
        if (same_file)
        {
            h_err = h_out;
        }
        else
        {
            h_err = CreateFileA(stderr_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h_err == INVALID_HANDLE_VALUE)
            {
                if (h_out) CloseHandle(h_out);
                return false;
            }
        }
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    if (inherit)
    {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = stdout_path ? h_out : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = stderr_path ? h_err : GetStdHandle(STD_ERROR_HANDLE);
    }

    const char* app = NULL;
    if (win_is_path(argv[0])) {
        app = argv[0];
    }

    BOOL ok = CreateProcessA(
        app,
        cmdline,
        NULL,
        NULL,
        inherit,
        0,
        NULL,
        cwd,
        &si,
        &pi
    );

    if (h_out) CloseHandle(h_out);
    if (h_err && h_err != h_out) CloseHandle(h_err);

    if (!ok) return false;

    out_result->started = true;
    (void)WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &code))
        code = 1;
    out_result->exit_code = (int)code;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool baa_process_run(const char* const* argv, const char* cwd, BaaProcessResult* out_result)
{
    return win_run_createprocess(argv, cwd, NULL, NULL, out_result);
}

bool baa_process_run_redirect(const char* const* argv,
                              const char* cwd,
                              const char* stdout_path,
                              const char* stderr_path,
                              BaaProcessResult* out_result)
{
    return win_run_createprocess(argv, cwd, stdout_path, stderr_path, out_result);
}

#else

static bool posix_spawn_wait(const char* const* argv,
                             const char* cwd,
                             const char* stdout_path,
                             const char* stderr_path,
                             BaaProcessResult* out_result)
{
    result_init(out_result);
    if (!argv || !argv[0]) return false;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0)
    {
        if (cwd && chdir(cwd) != 0)
            _exit(127);

        if (stdout_path && stderr_path && strcmp(stdout_path, stderr_path) == 0)
        {
            int fd = open(stdout_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            if (fd < 0) _exit(127);
            (void)dup2(fd, STDOUT_FILENO);
            (void)dup2(fd, STDERR_FILENO);
            close(fd);
        }
        else
        {
            if (stdout_path)
            {
                int fd = open(stdout_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                if (fd < 0) _exit(127);
                (void)dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (stderr_path)
            {
                int fd = open(stderr_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                if (fd < 0) _exit(127);
                (void)dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        execvp(argv[0], (char* const*)argv);
        _exit(127);
    }

    out_result->started = true;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return false;

    if (WIFEXITED(status))
        out_result->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        out_result->exit_code = 128 + WTERMSIG(status);
    else
        out_result->exit_code = 1;

    return true;
}

bool baa_process_run(const char* const* argv, const char* cwd, BaaProcessResult* out_result)
{
    return posix_spawn_wait(argv, cwd, NULL, NULL, out_result);
}

bool baa_process_run_redirect(const char* const* argv,
                              const char* cwd,
                              const char* stdout_path,
                              const char* stderr_path,
                              BaaProcessResult* out_result)
{
    return posix_spawn_wait(argv, cwd, stdout_path, stderr_path, out_result);
}

#endif
