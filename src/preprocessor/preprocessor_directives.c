#include "preprocessor_internal.h"
#include <wctype.h>
// Handles a line identified as starting with a preprocessor directive '#'.
// Modifies pp_state (conditional stack, macros, skipping state).
// May append output to output_buffer (for #include).
// Returns true if successfully processed (or skipped), false on error.
// Sets error_message on error.
// Sets is_conditional_directive to true if the directive was a conditional one (#if, #ifdef, etc.)
bool handle_preprocessor_directive(BaaPreprocessor *pp_state, wchar_t *directive_start, const char *abs_path, DynamicWcharBuffer *output_buffer, wchar_t **error_message, bool *is_conditional_directive)
{
    *is_conditional_directive = false; // Default
    bool success = true;

    // Define directive keywords and lengths
    const wchar_t *include_directive = L"تضمين";
    size_t include_directive_len = wcslen(include_directive);
    const wchar_t *define_directive = L"تعريف";
    size_t define_directive_len = wcslen(define_directive);
    const wchar_t *undef_directive = L"الغاء_تعريف";
    size_t undef_directive_len = wcslen(undef_directive);
    const wchar_t *ifdef_directive = L"إذا_عرف";
    size_t ifdef_directive_len = wcslen(ifdef_directive);
    const wchar_t *ifndef_directive = L"إذا_لم_يعرف";
    size_t ifndef_directive_len = wcslen(ifndef_directive);
    const wchar_t *else_directive = L"إلا";
    size_t else_directive_len = wcslen(else_directive);
    const wchar_t *elif_directive = L"وإلا_إذا";
    size_t elif_directive_len = wcslen(elif_directive);
    const wchar_t *endif_directive = L"نهاية_إذا";
    size_t endif_directive_len = wcslen(endif_directive);
    const wchar_t *if_directive = L"إذا";
    size_t if_directive_len = wcslen(if_directive);
    const wchar_t *error_directive = L"خطأ";
    size_t error_directive_len = wcslen(error_directive);
    const wchar_t *warning_directive = L"تحذير";
    size_t warning_directive_len = wcslen(warning_directive);

    PpSourceLocation directive_loc = get_current_original_location(pp_state); // Get location for potential errors

    // --- Process Conditional Directives FIRST, regardless of skipping state ---
    if (wcsncmp(directive_start, if_directive, if_directive_len) == 0 &&
        (directive_start[if_directive_len] == L'\0' || iswspace(directive_start[if_directive_len])))
    {
        *is_conditional_directive = true;
        wchar_t *expr_start = directive_start + if_directive_len;

        // Calculate the column where the expression part starts on the original line
        size_t expr_col_offset_on_line = directive_loc.column + if_directive_len; // Column of '#' + length of "#if"
        while (iswspace(*expr_start))
        {
            expr_start++;
            expr_col_offset_on_line++; // Account for leading whitespace before expression
        }

        // Find end of expression (before potential comment)
        wchar_t *expr_end = expr_start;
        wchar_t *comment_start = NULL;

        while (*expr_end != L'\0')
        {
            if (wcsncmp(expr_end, L"//", 2) == 0)
            {
                comment_start = expr_end;
                break;
            }
            expr_end++;
        }
        // Trim trailing whitespace from expression before comment
        if (comment_start)
        {
            expr_end = comment_start; // Point to start of comment
        }
        while (expr_end > expr_start && iswspace(*(expr_end - 1)))
        {
            expr_end--;
        }
        size_t expr_len = expr_end - expr_start;
        wchar_t *expression_only = wcsndup_internal(expr_start, expr_len); // Duplicate only the expression part

        if (!expression_only || expr_len == 0) // Check if expression is empty after stripping comment/whitespace
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_INVALID_DIRECTIVE_SYNTAX, "directive", L"تنسيق #إذا غير صالح: التعبير مفقود.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            success = true; // Recoverable syntax error - continue processing
            // Push a false condition to maintain conditional stack state
            if (!push_conditional(pp_state, false))
            {
                PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا (نفاد الذاكرة؟).");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                success = false; // Keep as fatal - this is memory allocation failure
            }
            if (expression_only)
                free(expression_only); // Free if allocated but empty
        }
        else
        {
            bool expr_value = false;

            // Temporarily set pp_state's column to the start of the expression for accurate evaluation errors
            size_t original_directive_col_for_expr_eval = pp_state->current_column_number;
            pp_state->current_column_number = expr_col_offset_on_line;

            if (!evaluate_preprocessor_expression(pp_state, expression_only, &expr_value, error_message, abs_path))
            {
                if (!*error_message) // Check if evaluator set a specific error
                    PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_EXPRESSION_TOO_COMPLEX, "expression", L"خطأ في تقييم تعبير #إذا.");
                if (!*error_message) // Only set if evaluator didn't provide a specific error
                    *error_message = generate_error_summary(pp_state);
                success = true; // Recoverable expression error - continue processing
                // Push a false condition to maintain conditional stack state
                if (!push_conditional(pp_state, false))
                {
                    PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا (نفاد الذاكرة؟).");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = false;
                }
            }
            else
            {
                if (!push_conditional(pp_state, expr_value))
                {
                    PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا (نفاد الذاكرة؟).");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = false;
                }
            }
            pp_state->current_column_number = original_directive_col_for_expr_eval; // Restore column
            free(expression_only);                                                  // Free the duplicated expression string
        }
    }
    else if (wcsncmp(directive_start, ifdef_directive, ifdef_directive_len) == 0 &&
             (directive_start[ifdef_directive_len] == L'\0' || iswspace(directive_start[ifdef_directive_len])))
    {
        *is_conditional_directive = true;
        wchar_t *name_start = directive_start + ifdef_directive_len;
        while (iswspace(*name_start))
            name_start++;
        wchar_t *name_end = name_start;
        while (*name_end != L'\0' && !iswspace(*name_end))
            name_end++;

        if (name_start == name_end)
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_MISSING_MACRO_NAME, "directive", L"تنسيق #إذا_عرف غير صالح: اسم الماكرو مفقود.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            // Push a false condition to maintain conditional stack state
            if (!push_conditional(pp_state, false))
            {
                PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا_عرف (نفاد الذاكرة؟).");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                return false;
            }
            return true; // Recoverable syntax error - continue processing
        }
        else
        {
            size_t name_len = name_end - name_start;
            wchar_t *macro_name = wcsndup_internal(name_start, name_len);
            if (!macro_name)
            {
                PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لاسم الماكرو في #إذا_عرف.");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                success = false; // Keep as fatal - this is memory allocation failure
            }
            else
            {
                bool is_defined = (find_macro(pp_state, macro_name) != NULL);
                if (!push_conditional(pp_state, is_defined))
                {
                    PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا_عرف (نفاد الذاكرة؟).");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = false;
                }
                free(macro_name);
            }
        }
    }
    else if (wcsncmp(directive_start, ifndef_directive, ifndef_directive_len) == 0 &&
             (directive_start[ifndef_directive_len] == L'\0' || iswspace(directive_start[ifndef_directive_len])))
    {
        *is_conditional_directive = true;
        wchar_t *name_start = directive_start + ifndef_directive_len;
        while (iswspace(*name_start))
            name_start++;
        wchar_t *name_end = name_start;
        while (*name_end != L'\0' && !iswspace(*name_end))
            name_end++;

        if (name_start == name_end)
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_MISSING_MACRO_NAME, "directive", L"تنسيق #إذا_لم_يعرف غير صالح: اسم الماكرو مفقود.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            // Push a false condition to maintain conditional stack state
            if (!push_conditional(pp_state, false))
            {
                PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا_لم_يعرف (نفاد الذاكرة؟).");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                return false;
            }
            return true; // Recoverable syntax error - continue processing
        }
        else
        {
            size_t name_len = name_end - name_start;
            wchar_t *macro_name = wcsndup_internal(name_start, name_len);
            if (!macro_name)
            {
                PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لاسم الماكرو في #إذا_لم_يعرف.");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                success = false; // Keep as fatal - this is memory allocation failure
            }
            else
            {
                bool is_defined = (find_macro(pp_state, macro_name) != NULL);
                if (!push_conditional(pp_state, !is_defined))
                { // Note the negation
                    PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع الحالة الشرطية لـ #إذا_لم_يعرف (نفاد الذاكرة؟).");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = false;
                }
                free(macro_name);
            }
        }
    }
    else if (wcsncmp(directive_start, endif_directive, endif_directive_len) == 0 &&
             (directive_start[endif_directive_len] == L'\0' || iswspace(directive_start[endif_directive_len])))
    {
        *is_conditional_directive = true;
        if (!pop_conditional(pp_state))
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_UNTERMINATED_CONDITION, "directive", L"#نهاية_إذا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            return true; // Recoverable directive syntax error - continue processing
        }
    }
    else if (wcsncmp(directive_start, else_directive, else_directive_len) == 0 &&
             (directive_start[else_directive_len] == L'\0' || iswspace(directive_start[else_directive_len])))
    {
        *is_conditional_directive = true;
        if (pp_state->conditional_stack_count == 0)
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_UNTERMINATED_CONDITION, "directive", L"#إلا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            return true; // Recoverable directive syntax error - continue processing
        }
        else
        {
            size_t top_index = pp_state->conditional_stack_count - 1;
            if (pp_state->conditional_branch_taken_stack[top_index])
            {
                pp_state->conditional_stack[top_index] = false;
            }
            else
            {
                pp_state->conditional_stack[top_index] = true;
                pp_state->conditional_branch_taken_stack[top_index] = true;
            }
            update_skipping_state(pp_state);
        }
    }
    else if (wcsncmp(directive_start, elif_directive, elif_directive_len) == 0 &&
             (directive_start[elif_directive_len] == L'\0' || iswspace(directive_start[elif_directive_len])))
    {
        *is_conditional_directive = true;
        if (pp_state->conditional_stack_count == 0)
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_UNTERMINATED_CONDITION, "directive", L"#وإلا_إذا بدون #إذا/#إذا_عرف/#إذا_لم_يعرف مطابق.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            return true; // Recoverable directive syntax error - continue processing
        }
        else
        {
            size_t top_index = pp_state->conditional_stack_count - 1;
            if (pp_state->conditional_branch_taken_stack[top_index])
            {
                pp_state->conditional_stack[top_index] = false;
            }
            else
            {
                wchar_t *expr_start = directive_start + elif_directive_len;

                size_t elif_expr_col_offset_on_line = directive_loc.column + elif_directive_len;
                while (iswspace(*expr_start))
                {
                    expr_start++;
                    elif_expr_col_offset_on_line++;
                }
                size_t original_directive_col_for_elif_eval = pp_state->current_column_number;
                pp_state->current_column_number = elif_expr_col_offset_on_line;

                // Find end of expression (before potential comment)
                wchar_t *expr_end = expr_start;
                wchar_t *comment_start = NULL;
                while (*expr_end != L'\0')
                {
                    if (wcsncmp(expr_end, L"//", 2) == 0)
                    {
                        comment_start = expr_end;
                        break;
                    }
                    expr_end++;
                }
                if (comment_start)
                {
                    expr_end = comment_start;
                }
                while (expr_end > expr_start && iswspace(*(expr_end - 1)))
                {
                    expr_end--;
                }
                size_t expr_len = expr_end - expr_start;
                wchar_t *expression_only = wcsndup_internal(expr_start, expr_len); // Duplicate only expression

                if (!expression_only || expr_len == 0) // Check if expression is empty
                {
                    PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_INVALID_DIRECTIVE_SYNTAX, "directive", L"تنسيق #وإلا_إذا غير صالح: التعبير مفقود.");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = true; // Recoverable syntax error - continue processing
                    if (expression_only)
                        free(expression_only); // Free if allocated but empty
                }
                else
                {
                    bool condition_met = false;
                    if (!evaluate_preprocessor_expression(pp_state, expression_only, &condition_met, error_message, abs_path))
                    {
                        if (!*error_message) // Check if evaluator set a specific error
                            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_EXPRESSION_TOO_COMPLEX, "expression", L"خطأ في تقييم تعبير #وإلا_إذا.");
                        if (!*error_message) // Only set if evaluator didn't provide a specific error
                            *error_message = generate_error_summary(pp_state);
                        success = true; // Recoverable expression error - continue processing
                    }
                    else
                    {
                        if (condition_met)
                        {
                            pp_state->conditional_stack[top_index] = true;
                            pp_state->conditional_branch_taken_stack[top_index] = true;
                        }
                        else
                        {
                            pp_state->conditional_stack[top_index] = false;
                        }
                    }
                    pp_state->current_column_number = original_directive_col_for_elif_eval; // Restore column
                    free(expression_only);                                                  // Free the duplicated expression string
                }
            }
            update_skipping_state(pp_state);
        }
    }

    // --- Process #تحذير directive ALWAYS (even when skipping) ---
    // --- Process #خطأ directive ONLY when not skipping ---
    if (!(*is_conditional_directive))
    {
        if (wcsncmp(directive_start, error_directive, error_directive_len) == 0 &&
            (directive_start[error_directive_len] == L'\0' || iswspace(directive_start[error_directive_len])) &&
            !pp_state->skipping_lines) // #خطأ - only when not skipping
        {
            wchar_t *message_start = directive_start + error_directive_len;
            while (iswspace(*message_start))
                message_start++;

            // Handle quoted strings properly
            wchar_t *actual_message_start = message_start;
            wchar_t *actual_message_end = message_start;

            if (*message_start == L'"')
            {
                // Skip opening quote
                actual_message_start = message_start + 1;
                actual_message_end = actual_message_start;

                // Find closing quote
                while (*actual_message_end != L'\0' && *actual_message_end != L'"')
                {
                    actual_message_end++;
                }

                // If we found a closing quote, use the content inside
                if (*actual_message_end == L'"')
                {
                    // Content is between quotes
                }
                else
                {
                    // No closing quote found, use everything after opening quote
                    while (*actual_message_end != L'\0')
                    {
                        if (wcsncmp(actual_message_end, L"//", 2) == 0)
                            break;
                        actual_message_end++;
                    }
                    while (actual_message_end > actual_message_start && iswspace(*(actual_message_end - 1)))
                        actual_message_end--;
                }
            }
            else
            {
                // No quotes, find end of message (before comment)
                while (*actual_message_end != L'\0')
                {
                    if (wcsncmp(actual_message_end, L"//", 2) == 0)
                        break;
                    actual_message_end++;
                }
                while (actual_message_end > actual_message_start && iswspace(*(actual_message_end - 1)))
                    actual_message_end--;
            }

            size_t message_len = actual_message_end - actual_message_start;
            wchar_t *raw_message_content = wcsndup_internal(actual_message_start, message_len);

            // Expand macros in the message content
            wchar_t *actual_message_content = raw_message_content;
            if (raw_message_content && wcslen(raw_message_content) > 0)
            {
                DynamicWcharBuffer expanded_buffer;
                if (init_dynamic_buffer(&expanded_buffer, wcslen(raw_message_content) + 256))
                {
                    wchar_t *expansion_error = NULL;
                    if (process_code_line_for_macros(pp_state, raw_message_content, wcslen(raw_message_content), &expanded_buffer, &expansion_error))
                    {
                        // Use expanded content
                        actual_message_content = _wcsdup(expanded_buffer.buffer);
                        free(raw_message_content);
                    }
                    else
                    {
                        // Expansion failed, use original content
                        if (expansion_error)
                            free(expansion_error);
                    }
                    free_dynamic_buffer(&expanded_buffer);
                }
            }

            PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_DIRECTIVE_NOT_ALLOWED, "directive", L"%ls", actual_message_content ? actual_message_content : L"");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            free(actual_message_content);
            return false; // Fatal error - return immediately
        }
        else if (wcsncmp(directive_start, warning_directive, warning_directive_len) == 0 &&
                 (directive_start[warning_directive_len] == L'\0' || iswspace(directive_start[warning_directive_len]))) // #تحذير
        {
            wchar_t *message_start = directive_start + warning_directive_len;
            while (iswspace(*message_start))
                message_start++;

            // Handle quoted strings properly
            wchar_t *actual_message_start = message_start;
            wchar_t *actual_message_end = message_start;

            if (*message_start == L'"')
            {
                // Skip opening quote
                actual_message_start = message_start + 1;
                actual_message_end = actual_message_start;

                // Find closing quote
                while (*actual_message_end != L'\0' && *actual_message_end != L'"')
                {
                    actual_message_end++;
                }

                // If we found a closing quote, use the content inside
                if (*actual_message_end == L'"')
                {
                    // Content is between quotes
                }
                else
                {
                    // No closing quote found, use everything after opening quote
                    while (*actual_message_end != L'\0')
                    {
                        if (wcsncmp(actual_message_end, L"//", 2) == 0)
                            break;
                        actual_message_end++;
                    }
                    while (actual_message_end > actual_message_start && iswspace(*(actual_message_end - 1)))
                        actual_message_end--;
                }
            }
            else
            {
                // No quotes, find end of message (before comment)
                while (*actual_message_end != L'\0')
                {
                    if (wcsncmp(actual_message_end, L"//", 2) == 0)
                        break;
                    actual_message_end++;
                }
                while (actual_message_end > actual_message_start && iswspace(*(actual_message_end - 1)))
                    actual_message_end--;
            }

            size_t message_len = actual_message_end - actual_message_start;
            wchar_t *raw_message_content = wcsndup_internal(actual_message_start, message_len);

            // Expand macros in the message content
            wchar_t *actual_message_content = raw_message_content;
            if (raw_message_content && wcslen(raw_message_content) > 0)
            {
                DynamicWcharBuffer expanded_buffer;
                if (init_dynamic_buffer(&expanded_buffer, wcslen(raw_message_content) + 256))
                {
                    wchar_t *expansion_error = NULL;
                    if (process_code_line_for_macros(pp_state, raw_message_content, wcslen(raw_message_content), &expanded_buffer, &expansion_error))
                    {
                        // Use expanded content
                        actual_message_content = _wcsdup(expanded_buffer.buffer);
                        free(raw_message_content);
                    }
                    else
                    {
                        // Expansion failed, use original content
                        if (expansion_error)
                            free(expansion_error);
                    }
                    free_dynamic_buffer(&expanded_buffer);
                }
            }

            PP_REPORT_WARNING(pp_state, &directive_loc, PP_ERROR_DIRECTIVE_NOT_ALLOWED, "directive", L"%ls", actual_message_content ? actual_message_content : L"");
            wchar_t *formatted_warning = format_preprocessor_warning_at_location(&directive_loc, L"%ls", actual_message_content ? actual_message_content : L"");
            if (formatted_warning)
            {
                fwprintf(stderr, L"%ls\n", formatted_warning); // Print to stderr
                free(formatted_warning);
            }
            free(actual_message_content);
            return true; // Warning processed successfully - return immediately
        }
    }

    // --- Process other directives ONLY if not skipping AND not a conditional directive ---
    if (!(*is_conditional_directive) && !pp_state->skipping_lines)
    {
        if (wcsncmp(directive_start, include_directive, include_directive_len) == 0 &&
            (directive_start[include_directive_len] == L'\0' || iswspace(directive_start[include_directive_len])))
        {
            // Found #تضمين directive
            wchar_t *path_spec_start = directive_start + include_directive_len;
            // Column number updated by caller before calling this function

            while (iswspace(*path_spec_start))
            {
                path_spec_start++;
                // pp_state->current_column_number++; // Column tracking handled by caller/tokenizer
            }

            wchar_t start_char = path_spec_start[0];
            wchar_t end_char = 0;
            bool use_include_paths = false;
            wchar_t *path_start = NULL;
            wchar_t *path_end = NULL;

            if (start_char == L'"')
            {
                end_char = L'"';
                use_include_paths = false;
                path_start = path_spec_start + 1;
                path_end = wcschr(path_start, end_char);
            }
            else if (start_char == L'<')
            {
                end_char = L'>';
                use_include_paths = true;
                path_start = path_spec_start + 1;
                path_end = wcschr(path_start, end_char);
            }
            else
            {
                PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_INVALID_INCLUDE, "directive", L"تنسيق #تضمين غير صالح: يجب أن يتبع اسم الملف بـ \" أو <.");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                success = true; // Recoverable syntax error - continue processing
            }

            if (success && path_end != NULL)
            {
                size_t include_path_len = path_end - path_start;
                if (include_path_len == 0)
                {
                    PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_INVALID_INCLUDE, "directive", L"تنسيق #تضمين غير صالح: مسار الملف فارغ.");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = true; // Recoverable syntax error - continue processing
                }
                else
                {
                    wchar_t *include_path_w = wcsndup_internal(path_start, include_path_len);
                    if (!include_path_w)
                    {
                        PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لمسار التضمين.");
                        if (error_message)
                            *error_message = generate_error_summary(pp_state);
                        success = false;
                    }
                    else
                    {
                        char *include_path_mb = NULL;
                        int required_bytes = WideCharToMultiByte(CP_UTF8, 0, include_path_w, -1, NULL, 0, NULL, NULL);
                        if (required_bytes > 0)
                        {
                            include_path_mb = malloc(required_bytes);
                            if (include_path_mb)
                                WideCharToMultiByte(CP_UTF8, 0, include_path_w, -1, include_path_mb, required_bytes, NULL, NULL);
                            else
                            {
                                PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لمسار التضمين (MB).");
                                if (error_message)
                                    *error_message = generate_error_summary(pp_state);
                                success = false;
                            }
                        }
                        else
                        {
                            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_ENCODING_ERROR, "file", L"فشل في تحويل مسار التضمين إلى UTF-8.");
                            if (error_message)
                                *error_message = generate_error_summary(pp_state);
                            success = true; // Recoverable encoding error - continue processing
                        }

                        if (success && include_path_mb)
                        {
                            char *full_include_path = NULL;
                            bool found = false;
                            if (use_include_paths)
                            {
                                for (size_t i = 0; i < pp_state->include_path_count; ++i)
                                {
                                    char temp_path[MAX_PATH_LEN];
                                    snprintf(temp_path, MAX_PATH_LEN, "%s%c%s", pp_state->include_paths[i], PATH_SEPARATOR, include_path_mb);
                                    FILE *test_file = NULL;
#ifdef _WIN32
                                    errno_t err = fopen_s(&test_file, temp_path, "rb");
                                    if (test_file && err == 0)
#else
                                    test_file = fopen(temp_path, "rb");
                                    if (test_file)
#endif
                                    {
                                        fclose(test_file);
                                        full_include_path = _strdup(temp_path);
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_FILE_NOT_FOUND, "file", L"تعذر العثور على ملف التضمين '<%hs>' في مسارات التضمين.", include_path_mb);
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = true; // Recoverable file not found error - continue processing
                                }
                            }
                            else
                            {
                                char *current_dir = get_directory_part(pp_state->current_file_path);
                                if (!current_dir)
                                {
                                    PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_INVALID_FILE_PATH, "file", L"فشل في الحصول على دليل الملف الحالي.");
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = true; // Recoverable file path error - continue processing
                                }
                                else
                                {
                                    char temp_path[MAX_PATH_LEN];
                                    snprintf(temp_path, MAX_PATH_LEN, "%s%c%s", current_dir, PATH_SEPARATOR, include_path_mb);
                                    full_include_path = _strdup(temp_path);
                                    free(current_dir);
                                    if (!full_include_path)
                                    {
                                        PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة للمسار النسبي المدمج.");
                                        if (error_message)
                                            *error_message = generate_error_summary(pp_state);
                                        success = false;
                                    }
                                    else
                                    {
                                        found = true;
                                    }
                                }
                            }

                            if (success && found && full_include_path)
                            {
                                PpSourceLocation include_loc = {.file_path = pp_state->current_file_path, .line = pp_state->current_line_number, .column = 1};
                                if (!push_location(pp_state, &include_loc))
                                {
                                    PP_REPORT_FATAL(pp_state, &include_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في دفع موقع التضمين (نفاد الذاكرة؟).");
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = false;
                                }
                                else
                                {
                                    wchar_t *included_content = process_file(pp_state, full_include_path, error_message); // Recursive call
                                    pop_location(pp_state);
                                    if (!included_content)
                                    {
                                        success = true; // Recoverable include error - continue processing
                                    }
                                    else
                                    {
                                        if (!append_to_dynamic_buffer(output_buffer, included_content))
                                        {
                                            if (!*error_message)
                                            {
                                                PpSourceLocation current_loc = get_current_original_location(pp_state);
                                                PP_REPORT_FATAL(pp_state, &current_loc, PP_ERROR_OUT_OF_MEMORY, "memory", L"فشل في إلحاق المحتوى المضمن من '%hs'.", full_include_path);
                                                if (error_message)
                                                    *error_message = generate_error_summary(pp_state);
                                            }
                                            success = false;
                                        }
                                        free(included_content);
                                    }
                                }
                            }
                            else if (success && !found)
                            { /* Error already set */
                            }
                            free(full_include_path);
                        }
                        free(include_path_mb);
                        free(include_path_w);
                    }
                }
            }
            else if (success)
            {
                PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_INVALID_INCLUDE, "directive", L"تنسيق #تضمين غير صالح: علامة الاقتباس أو القوس الختامي مفقود.");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                success = true; // Recoverable syntax error - continue processing
            }
            // #تضمين processed, don't append original line
        }
        else if (wcsncmp(directive_start, define_directive, define_directive_len) == 0 &&
                 (directive_start[define_directive_len] == L'\0' || iswspace(directive_start[define_directive_len])))
        {
            wchar_t *name_start = directive_start + define_directive_len;
            // Column number updated by caller
            size_t name_start_col_offset = define_directive_len; // Chars from '#' to end of "تعريف"

            while (iswspace(*name_start))
            {
                name_start++; /* pp_state->current_column_number++; */
                name_start_col_offset++;
            }

            // Create a more precise location for errors related to the name
            PpSourceLocation name_error_loc = directive_loc; // Base location is the start of the directive line
            name_error_loc.column += name_start_col_offset;  // Adjust column to where name should start

            wchar_t *name_end = name_start;
            while (*name_end != L'\0' && !iswspace(*name_end) && *name_end != L'(')
                name_end++;
            if (name_start == name_end)
            {
                PP_REPORT_ERROR(pp_state, &name_error_loc, PP_ERROR_MISSING_MACRO_NAME, "directive", L"تنسيق #تعريف غير صالح: اسم الماكرو مفقود.");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                return true; // Recoverable syntax error - continue processing
            }
            else
            {
                size_t name_len = name_end - name_start;
                wchar_t *macro_name = wcsndup_internal(name_start, name_len);
                if (!macro_name)
                {
                    PP_REPORT_FATAL(pp_state, &name_error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لاسم الماكرو في #تعريف.");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = false;
                }
                else
                {
                    bool is_function_like = false;
                    bool is_variadic_macro = false;
                    size_t param_count = 0;
                    wchar_t **params = NULL;
                    wchar_t *body_start = name_end;
                    const wchar_t *variadic_keyword = L"وسائط_إضافية";
                    size_t variadic_keyword_len = wcslen(variadic_keyword);

                    PpSourceLocation param_parse_loc = name_error_loc; // Start with name loc for param errors

                    if (*name_end == L'(')
                    {
                        is_function_like = true;
                        wchar_t *param_ptr = name_end + 1;
                        size_t params_capacity = 0;

                        while (success)
                        {
                            size_t current_param_start_col_offset = param_ptr - name_start; // Relative to name_start
                            PpSourceLocation current_arg_loc = name_error_loc;
                            current_arg_loc.column += current_param_start_col_offset;

                            while (iswspace(*param_ptr))
                                param_ptr++;
                            if (*param_ptr == L')')
                            {
                                param_ptr++;
                                current_arg_loc.column++;
                                break;
                            } // End of parameter list

                            if (param_count > 0 || is_variadic_macro)
                            { // If not the first param, or if variadic was just processed
                                if (is_variadic_macro)
                                { // No params allowed after variadic
                                    PP_REPORT_ERROR(pp_state, &current_arg_loc, PP_ERROR_INVALID_MACRO_PARAM, "directive", L"تنسيق #تعريف غير صالح: لا يمكن أن يتبع 'وسائط_إضافية' معاملات أخرى.");
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = true; // Recoverable syntax error - continue processing
                                    break;
                                }
                                if (*param_ptr == L',')
                                {
                                    param_ptr++;
                                    while (iswspace(*param_ptr))
                                        param_ptr++; // Column update handled by general loop progress
                                }
                                else
                                {
                                    PP_REPORT_ERROR(pp_state, &current_arg_loc, PP_ERROR_INVALID_MACRO_PARAM, "directive", L"تنسيق #تعريف غير صالح: متوقع ',' أو ')' بين معاملات الماكرو الوظيفي.");
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = true; // Recoverable syntax error - continue processing
                                    break;
                                }
                            }

                            // Check for variadic 'وسائط_إضافية'
                            if (wcsncmp(param_ptr, variadic_keyword, variadic_keyword_len) == 0 &&
                                (iswspace(*(param_ptr + variadic_keyword_len)) || *(param_ptr + variadic_keyword_len) == L')'))
                            {
                                is_variadic_macro = true;
                                param_ptr += variadic_keyword_len;
                                // 'وسائط_إضافية' must be the last thing before ')' or separated by whitespace then ')'
                                while (iswspace(*param_ptr))
                                    param_ptr++; // Column update handled by general loop progress
                                if (*param_ptr != L')')
                                {
                                    PP_REPORT_ERROR(pp_state, &current_arg_loc, PP_ERROR_INVALID_MACRO_PARAM, "directive", L"تنسيق #تعريف غير صالح: 'وسائط_إضافية' يجب أن تكون المعامل الأخير.");
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = true; // Recoverable syntax error - continue processing
                                    break;
                                }
                                // Do not add 'وسائط_إضافية' to params list, just set flag and break from param parsing loop
                                // The ')' will be consumed by the outer loop's break condition.
                                continue; // Continue to check for ')'
                            }

                            if (!iswalpha(*param_ptr) && *param_ptr != L'_')
                            {
                                PP_REPORT_ERROR(pp_state, &current_arg_loc, PP_ERROR_INVALID_MACRO_PARAM, "directive", L"تنسيق #تعريف غير صالح: متوقع اسم معامل أو ')' أو 'وسائط_إضافية' بعد '('.");
                                if (error_message)
                                    *error_message = generate_error_summary(pp_state);
                                success = true; // Recoverable syntax error - continue processing
                                break;
                            }
                            wchar_t *param_name_start = param_ptr;
                            while (iswalnum(*param_ptr) || *param_ptr == L'_')
                                param_ptr++;
                            wchar_t *param_name_end = param_ptr;
                            size_t param_name_len = param_name_end - param_name_start;
                            if (param_name_len == 0)
                            {
                                PP_REPORT_ERROR(pp_state, &current_arg_loc, PP_ERROR_INVALID_MACRO_PARAM, "directive", L"تنسيق #تعريف غير صالح: اسم معامل فارغ.");
                                if (error_message)
                                    *error_message = generate_error_summary(pp_state);
                                success = true; // Recoverable syntax error - continue processing
                                break;
                            }
                            wchar_t *param_name = wcsndup_internal(param_name_start, param_name_len);
                            if (!param_name)
                            {
                                PP_REPORT_FATAL(pp_state, &current_arg_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لاسم المعامل في #تعريف.");
                                if (error_message)
                                    *error_message = generate_error_summary(pp_state);
                                success = false;
                                break;
                            }
                            if (param_count >= params_capacity)
                            {
                                size_t new_capacity = (params_capacity == 0) ? 4 : params_capacity * 2;
                                wchar_t **new_params = realloc(params, new_capacity * sizeof(wchar_t *));
                                if (!new_params)
                                {
                                    free(param_name);
                                    PP_REPORT_FATAL(pp_state, &current_arg_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في إعادة تخصيص الذاكرة لمعاملات الماكرو في #تعريف.");
                                    if (error_message)
                                        *error_message = generate_error_summary(pp_state);
                                    success = false;
                                    break;
                                }
                                params = new_params;
                                params_capacity = new_capacity;
                            }
                            params[param_count++] = param_name;
                        }
                        if (success)
                        {
                            body_start = param_ptr;
                        }
                        else
                        {
                            if (params)
                            {
                                for (size_t i = 0; i < param_count; ++i)
                                    free(params[i]);
                                free(params);
                                params = NULL;
                                param_count = 0;
                            }
                        }
                    }
                    if (success)
                    {
                        while (iswspace(*body_start))
                            body_start++;

                        // Strip trailing comment from macro body
                        wchar_t *actual_body_end = body_start;
                        wchar_t *comment_in_body_start = NULL;
                        while (*actual_body_end != L'\0')
                        {
                            if (wcsncmp(actual_body_end, L"//", 2) == 0)
                            {
                                comment_in_body_start = actual_body_end;
                                break;
                            }
                            actual_body_end++;
                        }
                        if (comment_in_body_start)
                        {
                            actual_body_end = comment_in_body_start; // Point to start of comment
                        }
                        // Trim trailing whitespace from body before comment
                        while (actual_body_end > body_start && iswspace(*(actual_body_end - 1)))
                        {
                            actual_body_end--;
                        }
                        // Null-terminate the body before the comment (if any)
                        // This requires body_start to be a modifiable string, or we need to wcsndup.
                        // The 'directive_start' (and thus 'body_start') points into 'current_line' from preprocessor_core,
                        // which is a wcsndup'd copy, so it's modifiable.
                        *actual_body_end = L'\0';

                        if (!add_macro(pp_state, macro_name, body_start, is_function_like, is_variadic_macro, param_count, params))
                        {
                            PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في إضافة تعريف الماكرو '%ls' (نفاد الذاكرة؟).", macro_name);
                            if (error_message)
                                *error_message = generate_error_summary(pp_state);
                            success = false;
                            // params are owned by add_macro if it fails after taking them, or freed by it.
                            // If add_macro was never called or failed before taking ownership, params might still be here.
                            // The current add_macro frees params if it fails after taking ownership.
                            // If parsing params failed, they are freed above.
                            // So, no explicit free here unless add_macro's contract changes.
                            // Make sure name_error_loc is used here if body is missing or add_macro fails.
                            // add_macro itself might need to accept a location for its internal errors.
                        }
                    }
                    free(macro_name);
                }
            }
            // #تعريف processed
        }
        else if (wcsncmp(directive_start, undef_directive, undef_directive_len) == 0 &&
                 (directive_start[undef_directive_len] == L'\0' || iswspace(directive_start[undef_directive_len]))) // #الغاء_تعريف
        {
            wchar_t *name_start = directive_start + undef_directive_len;
            // Column number updated by caller
            while (iswspace(*name_start))
            {
                name_start++; /* pp_state->current_column_number++; */
            }
            wchar_t *name_end = name_start;
            while (*name_end != L'\0' && !iswspace(*name_end))
                name_end++;
            if (name_start == name_end)
            {
                PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_MISSING_MACRO_NAME, "directive", L"تنسيق #الغاء_تعريف غير صالح: اسم الماكرو مفقود.");
                if (error_message)
                    *error_message = generate_error_summary(pp_state);
                return true; // Recoverable syntax error - continue processing
            }
            else
            {
                size_t name_len = name_end - name_start;
                wchar_t *macro_name = wcsndup_internal(name_start, name_len);
                if (!macro_name)
                {
                    PP_REPORT_FATAL(pp_state, &directive_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص ذاكرة لاسم الماكرو في #الغاء_تعريف.");
                    if (error_message)
                        *error_message = generate_error_summary(pp_state);
                    success = false;
                }
                else
                {
                    undefine_macro(pp_state, macro_name);
                    free(macro_name);
                }
            }
            // #الغاء_تعريف processed
        }
        else
        {
            PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_UNKNOWN_DIRECTIVE, "directive", L"توجيه معالج مسبق غير معروف يبدأ بـ '#'.");
            if (error_message)
                *error_message = generate_error_summary(pp_state);
            success = true; // Recoverable syntax error - continue processing
        }
    }
    // Conditional directives and skipped lines don't produce output.

    return success;
}
