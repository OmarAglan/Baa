/**
 * @file formatter.h
 * @brief منسق مصدر باء المحافظ والمملوك للمصرف.
 */

#ifndef BAA_FRONTEND_FORMATTER_H
#define BAA_FRONTEND_FORMATTER_H

#include <stdbool.h>

typedef enum
{
    BAA_FORMAT_OK = 0,
    BAA_FORMAT_INVALID_UTF8,
    BAA_FORMAT_OUT_OF_MEMORY,
} BaaFormatStatus;

typedef struct
{
    char *text;
    bool changed;
} BaaFormatOutput;

/**
 * @brief تنسيق مصدر باء دون تشغيل المعالج القبلي أو تغيير النصوص والتعليقات.
 *
 * يعمل المنسق على النص غير المحفوظ، ويتحمل البنى غير المكتملة أثناء التحرير.
 * يملك المستدعي النص الناتج ويحرره عبر baa_format_output_free().
 */
BaaFormatStatus baa_format_source(const char *source, BaaFormatOutput *output);

void baa_format_output_free(BaaFormatOutput *output);

#endif
