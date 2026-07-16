/**
 * @file driver_nazm.h
 * @brief ربط مُصدّر نظم بعقد الخروج والتشخيص في سائق باء.
 */

#ifndef BAA_DRIVER_NAZM_H
#define BAA_DRIVER_NAZM_H

#include "driver.h"
#include "../backend/isel.h"

BaaCompilerExitCode driver_emit_nazm_source(const CompilerConfig *config,
                                            MachineModule *module,
                                            const char *output_path);

/**
 * @brief إرجاع ملف نظم التنفيذي للمسار الطبيعي.
 *
 * الأولوية: --nazm-path ثم BAA_NAZM ثم الاسم `nazm` من PATH.
 */
const char *driver_nazm_get_executable(const CompilerConfig *config);

/**
 * @brief إصدار وحدة الآلة كمصدر نظم وتجميعها إلى كائن للمسار الطبيعي.
 */
BaaCompilerExitCode driver_assemble_nazm_module(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    MachineModule *module,
    const char *source_path,
    const char *object_path,
    bool keep_source);

/**
 * @brief تجميع نقطة البدء المستضافة العربية لهدف ELF عبر نظم.
 */
BaaCompilerExitCode driver_assemble_nazm_startup(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    const char *source_path,
    const char *object_path,
    bool keep_source);

/**
 * @brief إصدار مصدر الظل وتجميعه عبر ملف نظم التنفيذي المحدد صراحة.
 * @param out_object_path مسار مملوك للمستدعي عند النجاح.
 */
BaaCompilerExitCode driver_emit_nazm_shadow_object(const CompilerConfig *config,
                                                   MachineModule *module,
                                                   char **out_object_path);

#endif
