/**
 * @file driver_format.h
 * @brief عقد إخراج تنسيق مصدر باء للأدوات.
 */

#ifndef BAA_DRIVER_FORMAT_H
#define BAA_DRIVER_FORMAT_H

#include <stdio.h>

#include "../frontend/formatter.h"

BaaFormatStatus driver_format_json_write(FILE *out,
                                         const char *compiler_version,
                                         const char *file,
                                         const char *source);

#endif
