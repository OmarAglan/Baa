/**
 * @file read_file.h
 * @brief واجهة قراءة الملفات وخدمات تضمين lexer المكتوب بباء.
 */

#ifndef BAA_READ_FILE_H
#define BAA_READ_FILE_H

#include <stddef.h>

char* read_file(const char* path);
void دعمالتضمينهيئ(const char* const* include_dirs, size_t include_dir_count);

#endif
