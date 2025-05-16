#ifndef BAA_FLOW_ANALYSIS_H
#define BAA_FLOW_ANALYSIS_H

#include "baa/types/types.h"          // Include type definitions
#include "baa/analysis/flow_errors.h" // Include specific error types
#include <stdbool.h>
#include <wchar.h>
#include <stddef.h>

// --- Moved from include/baa/control_flow.h (Analysis Parts) ---

// Structure to manage break/continue context during analysis
typedef struct BaaFlowContext
{
    struct BaaFlowContext *parent; // Parent context (for nesting)
    bool can_break;                // Is break allowed in this context?
    bool can_continue;             // Is continue allowed in this context?
    // Potentially add target labels if needed for labeled break/continue
} BaaFlowContext;

// --- End of moved code ---

#endif /* BAA_FLOW_ANALYSIS_H */
