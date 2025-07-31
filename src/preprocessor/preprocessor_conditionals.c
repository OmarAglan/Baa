// preprocessor_conditionals.c
#include "preprocessor_internal.h"

// --- Conditional Compilation Stack Helpers ---

// Updates the skipping_lines flag based on the current conditional stack.
// We skip if *any* level on the stack is currently inactive (false).
void update_skipping_state(BaaPreprocessor *pp_state)
{
    pp_state->skipping_lines = false;
    for (size_t i = 0; i < pp_state->conditional_stack_count; ++i)
    {
        if (!pp_state->conditional_stack[i])
        {
            pp_state->skipping_lines = true;
            return; // No need to check further levels
        }
    }
}

// Pushes a new state onto both conditional stacks
bool push_conditional(BaaPreprocessor *pp_state, bool condition_met)
{
    // Resize main stack if needed
    if (pp_state->conditional_stack_count >= pp_state->conditional_stack_capacity)
    {
        size_t new_capacity = (pp_state->conditional_stack_capacity == 0) ? 4 : pp_state->conditional_stack_capacity * 2;
        bool *new_main_stack = realloc(pp_state->conditional_stack, new_capacity * sizeof(bool));
        if (!new_main_stack)
        {
            PpSourceLocation error_loc = {
                .file_path = pp_state->current_file_path,
                .line = pp_state->current_line_number,
                .column = pp_state->current_column_number
            };
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_OUT_OF_MEMORY, "memory",
                           L"فشل في تخصيص الذاكرة لمكدس الشروط الرئيسي.");
            return false;
        }
        pp_state->conditional_stack = new_main_stack;
        pp_state->conditional_stack_capacity = new_capacity;
    }
    // Resize branch taken stack if needed
    if (pp_state->conditional_branch_taken_stack_count >= pp_state->conditional_branch_taken_stack_capacity)
    {
        size_t new_capacity = (pp_state->conditional_branch_taken_stack_capacity == 0) ? 4 : pp_state->conditional_branch_taken_stack_capacity * 2;
        bool *new_branch_stack = realloc(pp_state->conditional_branch_taken_stack, new_capacity * sizeof(bool));
        if (!new_branch_stack)
        {
            PpSourceLocation error_loc = {
                .file_path = pp_state->current_file_path,
                .line = pp_state->current_line_number,
                .column = pp_state->current_column_number
            };
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_OUT_OF_MEMORY, "memory",
                           L"فشل في تخصيص الذاكرة لمكدس الفروع الشرطية.");
            return false; // Should ideally handle potential mismatch if one realloc fails
        }
        pp_state->conditional_branch_taken_stack = new_branch_stack;
        pp_state->conditional_branch_taken_stack_capacity = new_capacity;
    }

    // Determine if this new branch (#if, #ifdef, #ifndef) is taken
    bool branch_taken = condition_met;

    // Push the condition result onto the main stack
    pp_state->conditional_stack[pp_state->conditional_stack_count++] = condition_met;
    // Push whether this specific branch was taken onto the branch stack
    pp_state->conditional_branch_taken_stack[pp_state->conditional_branch_taken_stack_count++] = branch_taken;

    update_skipping_state(pp_state); // Update overall skipping state based on the stacks
    return true;
}

// Pops the top state from both conditional stacks
bool pop_conditional(BaaPreprocessor *pp_state)
{
    if (pp_state->conditional_stack_count == 0)
    {
        PpSourceLocation error_loc = {
            .file_path = pp_state->current_file_path,
            .line = pp_state->current_line_number,
            .column = pp_state->current_column_number
        };
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_UNTERMINATED_CONDITION, "directive",
                       L"محاولة إغلاق كتلة شرطية بدون كتلة مفتوحة (تجاوز سفلي في المكدس).");
        return false; // Stack underflow
    }
    // Ensure counts match before decrementing (should always be true if logic is correct)
    if (pp_state->conditional_branch_taken_stack_count != pp_state->conditional_stack_count)
    {
        PpSourceLocation error_loc = {
            .file_path = pp_state->current_file_path,
            .line = pp_state->current_line_number,
            .column = pp_state->current_column_number
        };
        PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory",
                       L"حالة خطأ داخلية: عدم تطابق أعداد مكدسات الشروط (%zu != %zu).",
                       pp_state->conditional_branch_taken_stack_count, pp_state->conditional_stack_count);
        return false;
    }
    pp_state->conditional_stack_count--;
    pp_state->conditional_branch_taken_stack_count--;
    update_skipping_state(pp_state); // Update skipping state
    return true;
}

// Frees both conditional stack memories
void free_conditional_stack(BaaPreprocessor *pp)
{
    free(pp->conditional_stack);
    pp->conditional_stack = NULL;
    pp->conditional_stack_count = 0;
    pp->conditional_stack_capacity = 0;

    free(pp->conditional_branch_taken_stack);
    pp->conditional_branch_taken_stack = NULL;
    pp->conditional_branch_taken_stack_count = 0;
    pp->conditional_branch_taken_stack_capacity = 0;

    pp->skipping_lines = false;
}
