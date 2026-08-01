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
    DRIVER_CMD_EXPLAIN = 4,
    DRIVER_CMD_TARGET_INFO = 5,
    DRIVER_CMD_COMPLETION_DATA = 6,
    DRIVER_CMD_FORMAT = 7,
    DRIVER_CMD_TOKEN_DUMP = 8,
    DRIVER_CMD_STRUCTURE_DUMP = 9,
} DriverCommand;

typedef struct
{
    DriverCommand cmd;
    char **input_files; // مؤشرات إلى argv (لا تُملك)
    int input_count;
    const char **include_dirs; // مسارات -I (مملوكة من Parser)
    size_t include_dir_count;
    const char *explain_code; // رمز التشخيص لـ --explain (مؤشر إلى argv)
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
void driver_print_target_info_json(const BaaTarget *selected_target);
bool driver_print_diagnostic_explain(const char *code);

#endif // BAA_DRIVER_CLI_H
