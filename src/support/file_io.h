/**
 * @file file_io.h
 * @brief فتح ملفات بمسارات UTF-8 بصورة محمولة.
 */

#ifndef BAA_FILE_IO_H
#define BAA_FILE_IO_H

#include <stdio.h>

#ifdef _WIN32
#include <wchar.h>
#endif

/**
 * @brief فتح ملف؛ المسار والنمط UTF-8، والنتيجة يملكها المستدعي عبر fclose.
 */
FILE* baa_fopen_utf8(const char* path, const char* mode);

/**
 * @brief إنشاء مجلد واحد من مسار UTF-8، مع دلالة رجوع mkdir المعتادة.
 */
int baa_mkdir_utf8(const char* path);

/**
 * @brief إرجاع مسار مطلق مطبّع من مسار UTF-8.
 *
 * يملك المستدعي النص المعاد ويحرره عبر free(). تعاد NULL عند تعذر الحل.
 */
char* baa_fullpath_utf8(const char* path);

#ifdef _WIN32
/**
 * @brief تحويل مسار UTF-8 إلى مسار Windows مطلق ببادئة المسارات الطويلة.
 *
 * يملك المستدعي المخزن المعاد ويحرره عبر free().
 */
wchar_t* baa_windows_extended_path_utf8(const char* path);
#endif

#endif
