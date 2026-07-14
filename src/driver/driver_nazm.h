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

#endif
