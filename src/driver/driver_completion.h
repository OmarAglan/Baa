/**
 * @file driver_completion.h
 * @brief إخراج عقد بيانات الإكمال الخاص بأدوات باء.
 */

#ifndef BAA_DRIVER_COMPLETION_H
#define BAA_DRIVER_COMPLETION_H

#include <stdbool.h>
#include <stdio.h>

bool driver_completion_data_json_write(FILE *out, const char *compiler_version);

#endif
