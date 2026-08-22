/**
 * @file process_runtime.c
 * @brief تنفيذ متعدد المنصات لعمليات باء المهيكلة دون غلاف أوامر.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "process_runtime.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

typedef struct {
    bool finished;
    int64_t exit_code;
#ifdef _WIN32
    HANDLE process;
#else
    pid_t pid;
#endif
} BaaRuntimeProcess;

static char* baa_text_to_utf8(const BaaRuntimeChar* text)
{
    if (!text) return NULL;

    size_t bytes = 0;
    for (size_t i = 0; text[i] != 0; ++i) {
        uint64_t packed = text[i];
        unsigned len = (unsigned)((packed >> 32) & 0xffu);
        if (len < 1u || len > 4u || bytes > SIZE_MAX - len) return NULL;
        bytes += len;
    }

    char* out = (char*)malloc(bytes + 1u);
    if (!out) return NULL;

    size_t pos = 0;
    for (size_t i = 0; text[i] != 0; ++i) {
        uint64_t packed = text[i];
        unsigned len = (unsigned)((packed >> 32) & 0xffu);
        uint32_t raw = (uint32_t)(packed & UINT32_C(0xffffffff));
        for (unsigned j = 0; j < len; ++j) {
            out[pos++] = (char)((raw >> (j * 8u)) & 0xffu);
        }
    }
    out[pos] = '\0';
    return out;
}

static void free_utf8_vector(char** values, int64_t count)
{
    if (!values) return;
    for (int64_t i = 0; i < count; ++i) free(values[i]);
    free(values);
}

static char** baa_text_vector_to_utf8(const BaaRuntimeChar* const* values, int64_t count)
{
    if (!values || count <= 0 || count > 65535) return NULL;
    char** out = (char**)calloc((size_t)count + 1u, sizeof(char*));
    if (!out) return NULL;

    for (int64_t i = 0; i < count; ++i) {
        if (!values[i]) {
            free_utf8_vector(out, i);
            return NULL;
        }
        out[i] = baa_text_to_utf8(values[i]);
        if (!out[i]) {
            free_utf8_vector(out, i);
            return NULL;
        }
    }
    return out;
}

static bool path_has_parent_segment(const char* path)
{
    if (!path) return true;
    const char* p = path;
    while (*p) {
        while (*p == '/' || *p == '\\') ++p;
        const char* start = p;
        while (*p && *p != '/' && *p != '\\') ++p;
        if ((size_t)(p - start) == 2u && start[0] == '.' && start[1] == '.') return true;
    }
    return false;
}

static bool path_is_dangerous(const char* path)
{
    if (!path || !path[0]) return true;
    if (strcmp(path, ".") == 0 || strcmp(path, "./") == 0 || strcmp(path, ".\\") == 0) return true;
    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0) return true;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '/' || path[2] == '\\') && path[3] == '\0') return true;
    return path_has_parent_segment(path);
}

#ifdef _WIN32

typedef struct {
    wchar_t* data;
    size_t len;
    size_t cap;
} WideBuffer;

static wchar_t* utf8_to_wide(const char* text)
{
    if (!text) return NULL;
    int need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (need <= 0) return NULL;
    wchar_t* out = (wchar_t*)calloc((size_t)need, sizeof(wchar_t));
    if (!out) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, out, need) <= 0) {
        free(out);
        return NULL;
    }
    return out;
}

static wchar_t* utf8_to_extended_path(const char* path)
{
    wchar_t* wide = utf8_to_wide(path);
    if (!wide) return NULL;
    if (wcsncmp(wide, L"\\\\?\\", 4) == 0 ||
        wcsncmp(wide, L"\\\\.\\", 4) == 0) return wide;

    DWORD needed = GetFullPathNameW(wide, 0, NULL, NULL);
    if (needed == 0) {
        free(wide);
        return NULL;
    }
    wchar_t* absolute = (wchar_t*)calloc((size_t)needed + 1u, sizeof(wchar_t));
    if (!absolute) {
        free(wide);
        return NULL;
    }
    DWORD written = GetFullPathNameW(wide, needed, absolute, NULL);
    free(wide);
    if (written == 0 || written >= needed) {
        free(absolute);
        return NULL;
    }
    for (wchar_t* cursor = absolute; *cursor; ++cursor) {
        if (*cursor == L'/') *cursor = L'\\';
    }

    bool unc = absolute[0] == L'\\' && absolute[1] == L'\\';
    const wchar_t* prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
    const wchar_t* suffix = unc ? absolute + 2 : absolute;
    size_t prefix_len = wcslen(prefix);
    size_t suffix_len = wcslen(suffix);
    wchar_t* extended = (wchar_t*)malloc(
        (prefix_len + suffix_len + 1u) * sizeof(wchar_t));
    if (!extended) {
        free(absolute);
        return NULL;
    }
    memcpy(extended, prefix, prefix_len * sizeof(wchar_t));
    memcpy(extended + prefix_len, suffix, (suffix_len + 1u) * sizeof(wchar_t));
    free(absolute);
    return extended;
}

static bool wide_buffer_reserve(WideBuffer* b, size_t extra)
{
    if (!b || extra > SIZE_MAX - b->len - 1u) return false;
    size_t need = b->len + extra + 1u;
    if (need <= b->cap) return true;
    size_t cap = b->cap ? b->cap : 64u;
    while (cap < need) {
        if (cap > SIZE_MAX / 2u) { cap = need; break; }
        cap *= 2u;
    }
    wchar_t* grown = (wchar_t*)realloc(b->data, cap * sizeof(wchar_t));
    if (!grown) return false;
    b->data = grown;
    b->cap = cap;
    return true;
}

static bool wide_buffer_char(WideBuffer* b, wchar_t value)
{
    if (!wide_buffer_reserve(b, 1u)) return false;
    b->data[b->len++] = value;
    b->data[b->len] = L'\0';
    return true;
}

static bool wide_buffer_repeat(WideBuffer* b, wchar_t value, size_t count)
{
    if (!wide_buffer_reserve(b, count)) return false;
    for (size_t i = 0; i < count; ++i) b->data[b->len++] = value;
    b->data[b->len] = L'\0';
    return true;
}

static bool wide_buffer_arg(WideBuffer* b, const wchar_t* arg)
{
    bool quote = !arg[0] || wcspbrk(arg, L" \t\"") != NULL;
    if (!quote) {
        size_t n = wcslen(arg);
        if (!wide_buffer_reserve(b, n)) return false;
        memcpy(b->data + b->len, arg, n * sizeof(wchar_t));
        b->len += n;
        b->data[b->len] = L'\0';
        return true;
    }

    if (!wide_buffer_char(b, L'"')) return false;
    size_t slashes = 0;
    for (const wchar_t* p = arg; ; ++p) {
        if (*p == L'\\') { ++slashes; continue; }
        if (*p == L'"') {
            if (!wide_buffer_repeat(b, L'\\', slashes * 2u + 1u) ||
                !wide_buffer_char(b, L'"')) return false;
            slashes = 0;
            continue;
        }
        if (*p == L'\0') {
            if (!wide_buffer_repeat(b, L'\\', slashes * 2u)) return false;
            break;
        }
        if (!wide_buffer_repeat(b, L'\\', slashes) || !wide_buffer_char(b, *p)) return false;
        slashes = 0;
    }
    return wide_buffer_char(b, L'"');
}

static wchar_t* build_windows_cmdline(char* const* argv, int64_t argc)
{
    WideBuffer b = {0};
    for (int64_t i = 0; i < argc; ++i) {
        wchar_t* wide = utf8_to_wide(argv[i]);
        if (!wide || (i > 0 && !wide_buffer_char(&b, L' ')) || !wide_buffer_arg(&b, wide)) {
            free(wide);
            free(b.data);
            return NULL;
        }
        free(wide);
    }
    return b.data;
}

static wchar_t* build_windows_env(char* const* env, int64_t env_count)
{
    if (!env || env_count <= 0) return NULL;
    size_t total = 1u;
    wchar_t** entries = (wchar_t**)calloc((size_t)env_count, sizeof(wchar_t*));
    if (!entries) return NULL;
    for (int64_t i = 0; i < env_count; ++i) {
        if (!strchr(env[i], '=')) goto fail;
        entries[i] = utf8_to_wide(env[i]);
        if (!entries[i]) goto fail;
        size_t n = wcslen(entries[i]) + 1u;
        if (total > SIZE_MAX - n) goto fail;
        total += n;
    }
    wchar_t* block = (wchar_t*)calloc(total, sizeof(wchar_t));
    if (!block) goto fail;
    size_t pos = 0;
    for (int64_t i = 0; i < env_count; ++i) {
        size_t n = wcslen(entries[i]) + 1u;
        memcpy(block + pos, entries[i], n * sizeof(wchar_t));
        pos += n;
        free(entries[i]);
    }
    free(entries);
    block[pos] = L'\0';
    return block;
fail:
    for (int64_t i = 0; i < env_count; ++i) free(entries[i]);
    free(entries);
    return NULL;
}

static HANDLE open_redirect(const char* path)
{
    wchar_t* wide = utf8_to_wide(path);
    if (!wide) return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE h = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    return h;
}

void* baa_runtime_process_start(const BaaRuntimeChar* const* baa_argv,
                                int64_t argc,
                                const BaaRuntimeChar* baa_cwd,
                                const BaaRuntimeChar* const* baa_env,
                                int64_t env_count,
                                const BaaRuntimeChar* baa_stdout,
                                const BaaRuntimeChar* baa_stderr)
{
    if (!baa_argv || argc <= 0 || env_count < 0) return NULL;
    char** argv = baa_text_vector_to_utf8(baa_argv, argc);
    char** env = env_count ? baa_text_vector_to_utf8(baa_env, env_count) : NULL;
    char* cwd = baa_text_to_utf8(baa_cwd);
    char* stdout_path = baa_text_to_utf8(baa_stdout);
    char* stderr_path = baa_text_to_utf8(baa_stderr);
    if (!argv || (env_count && !env) || (baa_cwd && !cwd) ||
        (baa_stdout && !stdout_path) || (baa_stderr && !stderr_path)) goto fail;

    wchar_t* cmdline = build_windows_cmdline(argv, argc);
    wchar_t* cwd_w = cwd ? utf8_to_wide(cwd) : NULL;
    wchar_t* env_w = env_count ? build_windows_env(env, env_count) : NULL;
    wchar_t* app_w = (strchr(argv[0], '/') || strchr(argv[0], '\\')) ? utf8_to_wide(argv[0]) : NULL;
    if (!cmdline || (cwd && !cwd_w) || (env_count && !env_w) ||
        ((strchr(argv[0], '/') || strchr(argv[0], '\\')) && !app_w)) {
        free(cmdline); free(cwd_w); free(env_w); free(app_w);
        goto fail;
    }

    bool same_redirect = stdout_path && stderr_path && strcmp(stdout_path, stderr_path) == 0;
    HANDLE out = stdout_path ? open_redirect(stdout_path) : NULL;
    HANDLE err = same_redirect ? out : (stderr_path ? open_redirect(stderr_path) : NULL);
    if ((stdout_path && out == INVALID_HANDLE_VALUE) ||
        (stderr_path && err == INVALID_HANDLE_VALUE)) {
        if (out && out != INVALID_HANDLE_VALUE) CloseHandle(out);
        if (err && err != out && err != INVALID_HANDLE_VALUE) CloseHandle(err);
        free(cmdline); free(cwd_w); free(env_w); free(app_w);
        goto fail;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (stdout_path || stderr_path) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = stdout_path ? out : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = stderr_path ? err : GetStdHandle(STD_ERROR_HANDLE);
    }

    DWORD flags = env_w ? CREATE_UNICODE_ENVIRONMENT : 0;
    BOOL ok = CreateProcessW(app_w, cmdline, NULL, NULL,
                             (stdout_path || stderr_path) ? TRUE : FALSE,
                             flags, env_w, cwd_w, &si, &pi);
    if (out) CloseHandle(out);
    if (err && err != out) CloseHandle(err);
    free(cmdline); free(cwd_w); free(env_w); free(app_w);
    if (!ok) goto fail;

    CloseHandle(pi.hThread);
    BaaRuntimeProcess* process = (BaaRuntimeProcess*)calloc(1u, sizeof(*process));
    if (!process) {
        TerminateProcess(pi.hProcess, 130u);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        goto fail;
    }
    process->process = pi.hProcess;
    process->exit_code = -1;
    free_utf8_vector(argv, argc);
    free_utf8_vector(env, env_count);
    free(cwd); free(stdout_path); free(stderr_path);
    return process;

fail:
    free_utf8_vector(argv, argc);
    free_utf8_vector(env, env_count);
    free(cwd); free(stdout_path); free(stderr_path);
    return NULL;
}

static int64_t windows_collect(BaaRuntimeProcess* process, DWORD timeout)
{
    if (!process || !process->process) return -1;
    if (process->finished) return 1;
    DWORD waited = WaitForSingleObject(process->process, timeout);
    if (waited == WAIT_TIMEOUT) return 0;
    if (waited != WAIT_OBJECT_0) return -1;
    DWORD code = 1;
    if (!GetExitCodeProcess(process->process, &code)) return -1;
    process->finished = true;
    process->exit_code = (int64_t)code;
    return 1;
}

static bool wide_is_dangerous(const wchar_t* path)
{
    char utf8[4096];
    int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, (int)sizeof(utf8), NULL, NULL);
    return n <= 0 || path_is_dangerous(utf8);
}

static wchar_t* first_creatable_component_w(wchar_t* path)
{
    if (!path) return NULL;
    if (wcsncmp(path, L"\\\\?\\UNC\\", 8) == 0) {
        wchar_t* server_end = wcschr(path + 8, L'\\');
        return server_end ? wcschr(server_end + 1, L'\\') : NULL;
    }
    if (wcsncmp(path, L"\\\\?\\", 4) == 0 &&
        path[4] && path[5] == L':' && path[6] == L'\\') return path + 6;
    if (path[0] == L'\\' && path[1] == L'\\') {
        wchar_t* server_end = wcschr(path + 2, L'\\');
        return server_end ? wcschr(server_end + 1, L'\\') : NULL;
    }
    if (path[0] && path[1] == L':' &&
        (path[2] == L'\\' || path[2] == L'/')) return path + 2;
    return path;
}

static int make_dirs_w(wchar_t* path)
{
    size_t len = wcslen(path);
    while (len > 1u && (path[len - 1u] == L'/' || path[len - 1u] == L'\\')) path[--len] = L'\0';
    wchar_t* first = first_creatable_component_w(path);
    if (!first) return -1;
    for (wchar_t* p = first + (*first != L'\0'); *p; ++p) {
        if (*p != L'/' && *p != L'\\') continue;
        wchar_t saved = *p;
        *p = L'\0';
        if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return -1;
        *p = saved;
    }
    if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return -1;
    return 0;
}

static int remove_tree_w(const wchar_t* path)
{
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) ? 0 : -1;
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY) || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
        if (attrs & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(path, attrs & ~FILE_ATTRIBUTE_READONLY);
        return DeleteFileW(path) ? 0 : -1;
    }

    size_t len = wcslen(path);
    wchar_t* pattern = (wchar_t*)malloc((len + 3u) * sizeof(wchar_t));
    if (!pattern) return -1;
    wcscpy(pattern, path);
    if (len && pattern[len - 1u] != L'/' && pattern[len - 1u] != L'\\') pattern[len++] = L'\\';
    pattern[len++] = L'*';
    pattern[len] = L'\0';

    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern, &data);
    free(pattern);
    if (find == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) return -1;
        if (attrs & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(path, attrs & ~FILE_ATTRIBUTE_READONLY);
        return RemoveDirectoryW(path) ? 0 : -1;
    }
    int result = 0;
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        size_t name_len = wcslen(data.cFileName);
        wchar_t* child = (wchar_t*)malloc((len + name_len + 1u) * sizeof(wchar_t));
        if (!child) { result = -1; break; }
        wmemcpy(child, path, wcslen(path));
        size_t pos = wcslen(path);
        if (pos && child[pos - 1u] != L'/' && child[pos - 1u] != L'\\') child[pos++] = L'\\';
        wcscpy(child + pos, data.cFileName);
        if (remove_tree_w(child) != 0) result = -1;
        free(child);
        if (result != 0) break;
    } while (FindNextFileW(find, &data));
    FindClose(find);
    if (result != 0) return result;
    if (attrs & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(path, attrs & ~FILE_ATTRIBUTE_READONLY);
    return RemoveDirectoryW(path) ? 0 : -1;
}

#else

static int redirect_fd(const char* path, int target)
{
    if (!path) return 0;
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return -1;
    int result = dup2(fd, target);
    close(fd);
    return result < 0 ? -1 : 0;
}

static const char* env_path(char* const* env)
{
    if (!env) return NULL;
    for (size_t i = 0; env[i]; ++i) {
        if (strncmp(env[i], "PATH=", 5u) == 0) return env[i] + 5u;
    }
    return NULL;
}

static void exec_with_env(char* const* argv, char* const* env)
{
    if (strchr(argv[0], '/')) {
        execve(argv[0], argv, env);
        return;
    }
    const char* path = env_path(env);
    if (!path || !path[0]) path = "/bin:/usr/bin";
    const char* start = path;
    for (const char* p = path; ; ++p) {
        if (*p != ':' && *p != '\0') continue;
        size_t dir_len = (size_t)(p - start);
        size_t exe_len = strlen(argv[0]);
        size_t total = (dir_len ? dir_len : 1u) + 1u + exe_len + 1u;
        char* candidate = (char*)malloc(total);
        if (!candidate) _exit(127);
        if (dir_len) memcpy(candidate, start, dir_len);
        else candidate[0] = '.';
        size_t pos = dir_len ? dir_len : 1u;
        candidate[pos++] = '/';
        memcpy(candidate + pos, argv[0], exe_len + 1u);
        execve(candidate, argv, env);
        int saved = errno;
        free(candidate);
        if (saved != ENOENT && saved != ENOTDIR) _exit(126);
        if (*p == '\0') break;
        start = p + 1;
    }
}

void* baa_runtime_process_start(const BaaRuntimeChar* const* baa_argv,
                                int64_t argc,
                                const BaaRuntimeChar* baa_cwd,
                                const BaaRuntimeChar* const* baa_env,
                                int64_t env_count,
                                const BaaRuntimeChar* baa_stdout,
                                const BaaRuntimeChar* baa_stderr)
{
    if (!baa_argv || argc <= 0 || env_count < 0) return NULL;
    char** argv = baa_text_vector_to_utf8(baa_argv, argc);
    char** env = env_count ? baa_text_vector_to_utf8(baa_env, env_count) : NULL;
    char* cwd = baa_text_to_utf8(baa_cwd);
    char* stdout_path = baa_text_to_utf8(baa_stdout);
    char* stderr_path = baa_text_to_utf8(baa_stderr);
    if (!argv || (env_count && !env) || (baa_cwd && !cwd) ||
        (baa_stdout && !stdout_path) || (baa_stderr && !stderr_path)) goto fail;

    pid_t pid = fork();
    if (pid < 0) goto fail;
    if (pid == 0) {
        if (cwd && chdir(cwd) != 0) _exit(127);
        if (stdout_path && stderr_path && strcmp(stdout_path, stderr_path) == 0) {
            int fd = open(stdout_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) _exit(127);
            close(fd);
        } else if (redirect_fd(stdout_path, STDOUT_FILENO) != 0 ||
                   redirect_fd(stderr_path, STDERR_FILENO) != 0) {
            _exit(127);
        }
        if (env_count) exec_with_env(argv, env);
        else execvp(argv[0], argv);
        _exit(errno == EACCES ? 126 : 127);
    }

    BaaRuntimeProcess* process = (BaaRuntimeProcess*)calloc(1u, sizeof(*process));
    if (!process) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        goto fail;
    }
    process->pid = pid;
    process->exit_code = -1;
    free_utf8_vector(argv, argc);
    free_utf8_vector(env, env_count);
    free(cwd); free(stdout_path); free(stderr_path);
    return process;

fail:
    free_utf8_vector(argv, argc);
    free_utf8_vector(env, env_count);
    free(cwd); free(stdout_path); free(stderr_path);
    return NULL;
}

static int64_t posix_collect(BaaRuntimeProcess* process, int options)
{
    if (!process || process->pid <= 0) return -1;
    if (process->finished) return 1;
    int status = 0;
    pid_t result;
    do { result = waitpid(process->pid, &status, options); } while (result < 0 && errno == EINTR);
    if (result == 0) return 0;
    if (result < 0) return -1;
    process->finished = true;
    if (WIFEXITED(status)) process->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) process->exit_code = 128 + WTERMSIG(status);
    else process->exit_code = 1;
    return 1;
}

static int make_dirs_utf8(char* path)
{
    size_t len = strlen(path);
    while (len > 1u && path[len - 1u] == '/') path[--len] = '\0';
    for (char* p = path; *p; ++p) {
        if (*p != '/' || p == path) continue;
        *p = '\0';
        if (mkdir(path, 0777) != 0 && errno != EEXIST) return -1;
        *p = '/';
    }
    return (mkdir(path, 0777) == 0 || errno == EEXIST) ? 0 : -1;
}

static int remove_tree_utf8(const char* path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) return unlink(path) == 0 ? 0 : -1;
    DIR* dir = opendir(path);
    if (!dir) return -1;
    int result = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        size_t need = strlen(path) + 1u + strlen(entry->d_name) + 1u;
        char* child = (char*)malloc(need);
        if (!child) { result = -1; break; }
        snprintf(child, need, "%s/%s", path, entry->d_name);
        if (remove_tree_utf8(child) != 0) result = -1;
        free(child);
        if (result != 0) break;
    }
    closedir(dir);
    return result == 0 && rmdir(path) == 0 ? 0 : -1;
}

#endif

int64_t baa_runtime_process_poll(void* handle)
{
    BaaRuntimeProcess* process = (BaaRuntimeProcess*)handle;
#ifdef _WIN32
    return windows_collect(process, 0u);
#else
    return posix_collect(process, WNOHANG);
#endif
}

int64_t baa_runtime_process_wait(void* handle)
{
    BaaRuntimeProcess* process = (BaaRuntimeProcess*)handle;
#ifdef _WIN32
    if (windows_collect(process, INFINITE) < 0) return -1;
#else
    if (posix_collect(process, 0) < 0) return -1;
#endif
    return process->exit_code;
}

int64_t baa_runtime_process_cancel(void* handle)
{
    BaaRuntimeProcess* process = (BaaRuntimeProcess*)handle;
    if (!process) return 0;
    if (baa_runtime_process_poll(process) == 1) return 1;
#ifdef _WIN32
    return TerminateProcess(process->process, 130u) ? 1 : 0;
#else
    return kill(process->pid, SIGTERM) == 0 ? 1 : 0;
#endif
}

int64_t baa_runtime_process_exit_code(void* handle)
{
    BaaRuntimeProcess* process = (BaaRuntimeProcess*)handle;
    if (!process) return -1;
    (void)baa_runtime_process_poll(process);
    return process->finished ? process->exit_code : -1;
}

void baa_runtime_process_free(void* handle)
{
    BaaRuntimeProcess* process = (BaaRuntimeProcess*)handle;
    if (!process) return;
    if (baa_runtime_process_poll(process) == 0) {
        (void)baa_runtime_process_cancel(process);
        (void)baa_runtime_process_wait(process);
    }
#ifdef _WIN32
    CloseHandle(process->process);
#endif
    free(process);
}

int64_t baa_runtime_make_dirs(const BaaRuntimeChar* baa_path)
{
    char* path = baa_text_to_utf8(baa_path);
    if (!path || path_is_dangerous(path)) { free(path); return -1; }
#ifdef _WIN32
    wchar_t* safety_path = utf8_to_wide(path);
    wchar_t* extended_path = utf8_to_extended_path(path);
    int result = (!safety_path || wide_is_dangerous(safety_path) ||
                  !extended_path) ? -1 : make_dirs_w(extended_path);
    free(safety_path);
    free(extended_path);
#else
    int result = make_dirs_utf8(path);
#endif
    free(path);
    return result;
}

int64_t baa_runtime_remove_tree(const BaaRuntimeChar* baa_path)
{
    char* path = baa_text_to_utf8(baa_path);
    if (!path || path_is_dangerous(path)) { free(path); return -1; }
#ifdef _WIN32
    wchar_t* safety_path = utf8_to_wide(path);
    wchar_t* extended_path = utf8_to_extended_path(path);
    int result = (!safety_path || wide_is_dangerous(safety_path) ||
                  !extended_path) ? -1 : remove_tree_w(extended_path);
    free(safety_path);
    free(extended_path);
#else
    int result = remove_tree_utf8(path);
#endif
    free(path);
    return result;
}
