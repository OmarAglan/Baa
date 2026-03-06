/**
 * @file ir_pass.h
 * @brief IR pass interface (optimizer infrastructure).
 * @version 0.3.1.1
 *
 * Defines a minimal pass interface for IR transformations/analyses (v0.3.1.x).
 * Passes return whether they changed the IR (so the pipeline can iterate).
 */

#ifndef BAA_IR_PASS_H
#define BAA_IR_PASS_H

#include <stdbool.h>
#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function signature for an IR pass.
 *
 * @param module Target IR module to operate on.
 * @return true if the pass modified the IR; false otherwise.
 */
typedef bool (*IRPassRunFn)(IRModule* module);

/**
 * @struct IRPass
 * @brief Describes a single IR pass.
 */
typedef struct IRPass {
    const char* name;
    IRPassRunFn run;
} IRPass;

/**
 * @brief Run a pass (NULL-safe).
 * @return true if changed; false otherwise.
 */
bool ir_pass_run(IRPass* pass, IRModule* module);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_PASS_H