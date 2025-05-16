#include "baa/analysis/flow_analysis.h"
// #include "baa/ast/statements.h" // Removed as AST is being removed
// #include "baa/ast/expressions.h" // Removed as AST is being removed
#include "baa/types/types.h"
#include "baa/utils/utils.h" // For memory allocation if needed
#include "baa/analysis/flow_errors.h"
#include <stdlib.h>
#include <stdbool.h>

// --- Moved from src/control_flow/control_flow.c (Analysis Parts) ---

// Example implementation stub for analysis (Commented out as it depends on AST types)
/*
bool baa_analyze_control_flow(BaaBlock *block, BaaFlowContext *context, BaaType *expected_return_type)
{
    if (!block || !context)
    {
        baa_set_flow_analysis_error(BAA_FLOW_NULL_BODY); // Example error
        return false;
    }

    bool path_returns = false; // Track if all paths return (if necessary)

    for (size_t i = 0; i < block->count; ++i)
    {
        BaaStmt *stmt = block->statements[i];
        if (!stmt)
            continue;

        switch (stmt->kind)
        {
        case BAA_STMT_IF:
        {
            // Analyze condition
            BaaIfStmt *if_stmt = (BaaIfStmt *)stmt->data;
            // baa_analyze_expression(if_stmt->condition); // Analyze expression type/validity

            // Analyze bodies recursively
            BaaFlowContext if_context = *context; // Inherit context
            bool then_returns = baa_analyze_control_flow(if_stmt->if_body, &if_context, expected_return_type);
            bool else_returns = false;
            if (if_stmt->else_body)
            {
                else_returns = baa_analyze_control_flow(if_stmt->else_body, &if_context, expected_return_type);
            }
            // Determine if all paths return based on then/else
            path_returns = then_returns && else_returns; // Simplistic view
            break;
        }
        case BAA_STMT_WHILE:
        {
            BaaWhileStmt *while_stmt = (BaaWhileStmt *)stmt->data;
            // Analyze condition
            // baa_analyze_expression(while_stmt->condition);

            // Analyze body with updated context allowing break/continue
            BaaFlowContext loop_context = *context;
            loop_context.can_break = true;
            loop_context.can_continue = true;
            loop_context.parent = context;
            baa_analyze_control_flow(while_stmt->body, &loop_context, expected_return_type);
            // A while loop doesn't guarantee return unless condition is false & path_returns was already true
            break;
        }
        case BAA_STMT_FOR:
        {
            BaaForStmt *for_stmt = (BaaForStmt *)stmt->data;
            // Analyze initializer, condition, increment
            // baa_analyze_statement(for_stmt->initializer);
            // baa_analyze_expression(for_stmt->condition);
            // baa_analyze_expression(for_stmt->increment);

            // Analyze body with updated context allowing break/continue
            BaaFlowContext loop_context = *context;
            loop_context.can_break = true;
            loop_context.can_continue = true;
            loop_context.parent = context;
            baa_analyze_control_flow(for_stmt->body, &loop_context, expected_return_type);
            // Like while, doesn't guarantee return
            break;
        }
        case BAA_STMT_RETURN:
        {
            BaaReturnStmt *ret_stmt = (BaaReturnStmt *)stmt->data;
            // Check return value type against expected_return_type
            // baa_analyze_expression(ret_stmt->value);
            // if (!type_compatible(ret_stmt->value->type, expected_return_type)) { ... set error ... }
            path_returns = true; // This path definitely returns
            break;
        }
        case BAA_STMT_BREAK:
            if (!context->can_break)
            {
                baa_set_flow_analysis_error(BAA_FLOW_INVALID_BREAK);
                return false; // Error
            }
            // Break interrupts flow, might affect path_returns analysis below
            break;
        case BAA_STMT_CONTINUE:
            if (!context->can_continue)
            {
                baa_set_flow_analysis_error(BAA_FLOW_INVALID_CONTINUE);
                return false; // Error
            }
            // Continue interrupts flow, might affect path_returns analysis below
            break;
        case BAA_STMT_BLOCK:
            // Analyze nested block
            path_returns = baa_analyze_control_flow((BaaBlock *)stmt->data, context, expected_return_type);
            break;
        // Add cases for other statements (VarDecl, ExprStmt, Switch, etc.)
        default:
            // Handle other statement types if necessary
            break;
        }

        // Add unreachable code detection logic here if needed
        // If path_returns becomes true, any subsequent statements are unreachable
    }

    return path_returns; // Return whether this block guarantees a return
}

bool baa_validate_function_control_flow(BaaBlock *function_body, BaaType *return_type)
{
    BaaFlowContext top_context = {.parent = NULL, .can_break = false, .can_continue = false};
    bool analysis_ok = baa_analyze_control_flow(function_body, &top_context, return_type);

    // Check if analysis itself reported an error (like invalid break)
    if (baa_get_last_flow_analysis_error() != BAA_FLOW_SUCCESS)
    {
        return false;
    }

    // After analysis, check if a return is required and if all paths guarantee one
    bool return_required = (return_type != NULL && return_type->kind != BAA_TYPE_VOID);
    if (return_required)
    {
        // This needs a more sophisticated analysis than the simple path_returns bool
        // Need to track if *all* possible execution paths end in a return.
        // For now, we can use a placeholder check.
        if (!baa_block_has_return_path(function_body))
        { // Assuming this function is implemented
            baa_set_flow_analysis_error(BAA_FLOW_MISSING_RETURN);
            return false;
        }
    }

    return analysis_ok; // Return true if analysis passed and returns are ok
}

// Stub implementations for other analysis functions (replace with actual logic)
bool baa_block_has_return_path(BaaBlock *block)
{
    // TODO: Implement actual logic to check if all paths return
    return true; // Placeholder
}

bool baa_block_has_unreachable_code(BaaBlock *block)
{
    // TODO: Implement actual logic to detect unreachable code
    return false; // Placeholder
}

bool baa_loop_condition_is_valid(BaaExpr *condition)
{
    // TODO: Check if condition expression is boolean-compatible
    return true; // Placeholder
}

bool baa_while_is_infinite_loop(BaaWhileStmt *while_stmt)
{
    // TODO: Implement logic to detect obvious infinite loops (e.g., while(true))
    return false; // Placeholder
}
*/
// --- End of moved code ---
