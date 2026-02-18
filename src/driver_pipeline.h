/**
 * @file driver_pipeline.h
 * @brief تنفيذ خط أنابيب الترجمة لكل ملف مصدر.
 * @version 0.3.3
 */

#ifndef BAA_DRIVER_PIPELINE_H
#define BAA_DRIVER_PIPELINE_H

#include "driver.h"

/**
 * @brief ترجمة قائمة ملفات مصدر وفق إعدادات CompilerConfig.
 *
 * - عند -S: لا تُنتج ملفات كائنات (out_obj_count = 0).
 * - عند -c: تُنتج ملفات كائنات لكل ملف (أو ملف واحد باسم -o).
 * - عند البناء الكامل: تُنتج ملفات كائنات للربط النهائي.
 */
int driver_compile_files(const CompilerConfig *config,
                         char **input_files,
                         int input_count,
                         CompilerPhaseTimes *phase_times,
                         char ***out_obj_files,
                         int *out_obj_count);

/**
 * @brief تحرير قائمة ملفات الكائنات المُخرجة.
 *
 * لا يحرر output_file (يُستثنى إذا كان ضمن القائمة).
 */
void driver_free_obj_files(char **obj_files, int obj_count, const char *output_file);

#endif // BAA_DRIVER_PIPELINE_H
