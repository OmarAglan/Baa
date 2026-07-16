/**
 * @file driver_startup.h
 * @brief Hosted Arabic startup source and object construction.
 */

#ifndef BAA_DRIVER_STARTUP_H
#define BAA_DRIVER_STARTUP_H

#include "driver.h"

/**
 * @brief Return the GAS startup source for the selected target.
 */
const char *driver_startup_gas_source(const BaaTarget *target);

/**
 * @brief Build the hosted startup object through the selected assembler.
 *
 * On success, `out_object_path` receives an allocated path owned by the caller.
 */
BaaCompilerExitCode driver_build_startup_object(
    const CompilerConfig *config,
    CompilerPhaseTimes *times,
    char **out_object_path);

#endif
