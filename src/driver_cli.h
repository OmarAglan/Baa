/**
 * @file driver_cli.h
 * @brief تحليل معاملات سطر الأوامر للـ Driver.
 * @version 0.3.3
 */

#ifndef BAA_DRIVER_CLI_H
#define BAA_DRIVER_CLI_H

#include <stdbool.h>

#include "driver.h"

typedef enum
{
    DRIVER_CMD_COMPILE = 0,
    DRIVER_CMD_HELP = 1,
    DRIVER_CMD_VERSION = 2,
    DRIVER_CMD_UPDATE = 3,
} DriverCommand;

typedef struct
{
    DriverCommand cmd;
    char **input_files; // مؤشرات إلى argv (لا تُملك)
    int input_count;
} DriverParseResult;

/**
 * @brief تحليل معاملات CLI وتعبئة إعدادات المترجم.
 * @return true عند النجاح، false عند وجود خطأ.
 */
bool driver_parse_cli(int argc, char **argv, CompilerConfig *config, DriverParseResult *out);

/**
 * @brief تحرير موارد DriverParseResult (لا يحرر أسماء الملفات نفسها).
 */
void driver_parse_result_free(DriverParseResult *r);

void driver_print_help(void);
void driver_print_version(void);

#endif // BAA_DRIVER_CLI_H
