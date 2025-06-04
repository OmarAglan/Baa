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
    // Check for excessive nesting depth
    if (pp_state->conditional_stack_count >= 100) // Reasonable limit
    {
        PpSourceLocation current_loc = get_current_original_location(pp_state);
        add_error_with_suggestion(pp_state, &current_loc,
            L"تحقق من أن كل #إذا، #إذا_عرف، أو #إذا_لم_يعرف له #نهاية_إذا مطابق",
            L"تم تجاوز الحد الأقصى لعمق التداخل الشرطي (%zu)", pp_state->conditional_stack_count);
        return false;
    }
    
    // Resize main stack if needed
    if (pp_state->conditional_stack_count >= pp_state->conditional_stack_capacity)
    {
        size_t new_capacity = (pp_state->conditional_stack_capacity == 0) ? 4 : pp_state->conditional_stack_capacity * 2;
        bool *new_main_stack = realloc(pp_state->conditional_stack, new_capacity * sizeof(bool));
        if (!new_main_stack)
        {
            PpSourceLocation current_loc = get_current_original_location(pp_state);
            add_fatal_diagnostic(pp_state, &current_loc,
                L"فشل في تخصيص الذاكرة للمكدس الشرطي (نفاد الذاكرة)");
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
            PpSourceLocation current_loc = get_current_original_location(pp_state);
            add_fatal_diagnostic(pp_state, &current_loc,
                L"فشل في تخصيص الذاكرة لمكدس الفروع الشرطية (نفاد الذاكرة)");
            return false;
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
        // Stack underflow - this indicates unmatched #endif
        return false; // Error handling done at caller level
    }
    
    // Ensure counts match before decrementing (should always be true if logic is correct)
    if (pp_state->conditional_branch_taken_stack_count != pp_state->conditional_stack_count)
    {
        // Internal error state - stack corruption
        PpSourceLocation current_loc = get_current_original_location(pp_state);
        add_fatal_diagnostic(pp_state, &current_loc,
            L"فساد في حالة المكدس الشرطي الداخلية - تضارب في أعداد المكدسات (%zu != %zu)",
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

// Enhanced conditional stack validation and recovery
bool validate_and_recover_conditional_stack(BaaPreprocessor *pp_state)
{
    if (!pp_state) return false;
    
    // Check for unmatched conditionals at end of processing
    if (pp_state->conditional_stack_count > 0)
    {
        PpSourceLocation current_loc = get_current_original_location(pp_state);
        
        // Report detailed information about unmatched conditionals
        add_error_with_suggestion(pp_state, &current_loc,
            L"أضف #نهاية_إذا لكل #إذا، #إذا_عرف، أو #إذا_لم_يعرف في الملف",
            L"توجيهات شرطية غير مطابقة: %zu كتلة شرطية لم يتم إنهاؤها",
            pp_state->conditional_stack_count);
        
        // Attempt recovery by clearing the stack
        add_info_diagnostic(pp_state, &current_loc,
            L"تم إفراغ المكدس الشرطي للسماح بالمتابعة");
        
        // Clear the conditional stack to allow continued processing
        pp_state->conditional_stack_count = 0;
        pp_state->conditional_branch_taken_stack_count = 0;
        pp_state->skipping_lines = false;
        
        return false; // Return false to indicate there were issues
    }
    
    // Validate stack consistency
    if (pp_state->conditional_branch_taken_stack_count != pp_state->conditional_stack_count)
    {
        PpSourceLocation current_loc = get_current_original_location(pp_state);
        add_warning_with_suggestion(pp_state, &current_loc,
            L"هذا خطأ داخلي، يرجى الإبلاغ عنه للمطورين",
            L"عدم تطابق في حالة المكدس الشرطي تم إصلاحه تلقائياً");
        
        // Fix the inconsistency
        pp_state->conditional_branch_taken_stack_count = pp_state->conditional_stack_count;
        return false; // Return false to indicate there were issues
    }
    
    return true; // All validation passed
}
