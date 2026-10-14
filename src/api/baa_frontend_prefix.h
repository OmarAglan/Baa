/**
 * @file baa_frontend_prefix.h
 * @brief ترويسة مفروضة على كل مصادر مكتبة baa_frontend (وليس على baa نفسه).
 *
 * تعيد توجيه ما يجعل المصرّف عملية مستقلة إلى الواجهة داخل العملية:
 * - stdout وstderr وstdin إلى ملفات مؤقتة يملكها النداء الحالي؛
 * - exit إلى رجوع إلى baa_frontend_run بدل إنهاء العملية المضيفة؛
 * - malloc وcalloc وrealloc وfree وstrdup إلى متتبع يحرر ما بقي بعد كل نداء.
 *
 * تضمَّن ترويسات النظام أولاً حتى تبقى تصريحاتها بأسمائها الأصلية،
 * ثم تعرَّف الماكروهات لما بعدها.
 */

#ifndef BAA_FRONTEND_PREFIX_H
#define BAA_FRONTEND_PREFIX_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
/* windows.h ليست هنا: ماكروهاتها (TOKEN_READ مثلاً) تصطدم بأسماء المصرّف،
 * ولا تعيد تصريح دوال التخصيص فلا يضر تضمينها بعد الماكروهات. */
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <dirent.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define BAA_FRONTEND_LIBRARY 1

#ifdef __cplusplus
extern "C" {
#endif

FILE *baa_frontend_io_stdin(void);
FILE *baa_frontend_io_stdout(void);
FILE *baa_frontend_io_stderr(void);

void *baa_frontend_track_malloc(size_t size);
void *baa_frontend_track_calloc(size_t count, size_t size);
void *baa_frontend_track_realloc(void *pointer, size_t size);
void baa_frontend_track_free(void *pointer);
char *baa_frontend_track_strdup(const char *text);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#endif
void baa_frontend_exit(int code);

#ifdef __cplusplus
}
#endif

/* baa_frontend.c يستعيد التعريفات الأصلية بـ pop_macro. */
#pragma push_macro("stdin")
#pragma push_macro("stdout")
#pragma push_macro("stderr")
#undef stdin
#undef stdout
#undef stderr
#define stdin (baa_frontend_io_stdin())
#define stdout (baa_frontend_io_stdout())
#define stderr (baa_frontend_io_stderr())

#define printf(...) fprintf(stdout, __VA_ARGS__)
#define vprintf(format, args) vfprintf(stdout, (format), (args))
#define puts(text) (fputs((text), stdout) < 0 ? EOF : fputc('\n', stdout))
#define putchar(c) fputc((c), stdout)

#define malloc baa_frontend_track_malloc
#define calloc baa_frontend_track_calloc
#define realloc baa_frontend_track_realloc
#define free baa_frontend_track_free
#define strdup baa_frontend_track_strdup
#define _strdup baa_frontend_track_strdup

#define exit baa_frontend_exit

#endif /* BAA_FRONTEND_PREFIX_H */
