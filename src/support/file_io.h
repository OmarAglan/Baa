/**
 * @file file_io.h
 * @brief فتح ملفات بمسارات UTF-8 بصورة محمولة.
 */

#ifndef BAA_FILE_IO_H
#define BAA_FILE_IO_H

#include <stdio.h>

/**
 * @brief فتح ملف؛ المسار والنمط UTF-8، والنتيجة يملكها المستدعي عبر fclose.
 */
FILE* baa_fopen_utf8(const char* path, const char* mode);

#endif

