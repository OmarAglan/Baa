#ifndef BAA_UTILS_H
#define BAA_UTILS_H

#include <stddef.h>
#include <wchar.h>

// دوال إدارة الذاكرة
void* baa_malloc(size_t size);
void* baa_realloc(void* ptr, size_t size);
void baa_free(void* ptr);

// دوال معالجة النصوص
wchar_t* baa_strdup(const wchar_t* str);
wchar_t* baa_strndup(const wchar_t* str, size_t n);
int baa_strcmp(const wchar_t* s1, const wchar_t* s2);

// دوال معالجة الملفات
wchar_t* baa_read_file(const wchar_t* filename);

#endif /* BAA_UTILS_H */
