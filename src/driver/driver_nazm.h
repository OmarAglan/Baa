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
                                            const char *current_input,
                                            const char *output_path);

/**
 * @brief إصدار مصدر الظل وتجميعه عبر ملف نظم التنفيذي المحدد صراحة.
 * @param out_object_path مسار مملوك للمستدعي عند النجاح.
 */
BaaCompilerExitCode driver_emit_nazm_shadow_object(const CompilerConfig *config,
                                                   MachineModule *module,
                                                   const char *current_input,
                                                   char **out_object_path);

#endif
