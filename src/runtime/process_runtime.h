/**
 * @file process_runtime.h
 * @brief واجهة وقت التشغيل للعمليات الخارجية ونظام الملفات.
 */

#ifndef BAA_PROCESS_RUNTIME_H
#define BAA_PROCESS_RUNTIME_H

#include <stdint.h>

typedef uint64_t BaaRuntimeChar;

void* baa_runtime_process_start(const BaaRuntimeChar* const* argv,
                                int64_t argc,
                                const BaaRuntimeChar* cwd,
                                const BaaRuntimeChar* const* env,
                                int64_t env_count,
                                const BaaRuntimeChar* stdout_path,
                                const BaaRuntimeChar* stderr_path);
int64_t baa_runtime_process_poll(void* handle);
int64_t baa_runtime_process_wait(void* handle);
int64_t baa_runtime_process_cancel(void* handle);
int64_t baa_runtime_process_exit_code(void* handle);
void baa_runtime_process_free(void* handle);

int64_t baa_runtime_make_dirs(const BaaRuntimeChar* path);
int64_t baa_runtime_remove_tree(const BaaRuntimeChar* path);
BaaRuntimeChar* baa_runtime_sha256_file(const BaaRuntimeChar* path);

#endif
