#include "preprocessor_internal.h"

// Helper function to perform one pass of macro scanning and substitution.
// - input_line_content: The current version of the line to be scanned.
// - original_line_number_for_errors: The original line number in the source file, for error reporting.
// - one_pass_buffer: The dynamic buffer where the output of this single pass is built.
// - overall_success: Pointer to a boolean that will be set to false on critical errors.
// - error_message: Pointer to the error message string.
// Returns true if at least one macro expansion OR token pasting occurred during this pass, false otherwise.
static bool scan_and_substitute_macros_one_pass(
    BaaPreprocessor *pp_state,
    const wchar_t *input_line_content,
    size_t original_line_number_for_errors,
    DynamicWcharBuffer *one_pass_buffer,
    bool *overall_success,
    wchar_t **error_message)
{
    bool transformation_occurred_this_pass = false; // Renamed for clarity (includes pasting)
    const wchar_t *scan_ptr = input_line_content;

    size_t current_col_in_this_scan_pass = 1; // Tracks column within the current input_line_content

    while (*scan_ptr != L'\0' && *overall_success)
    {
        size_t token_start_col_for_error = current_col_in_this_scan_pass;

        // --- Check for ## operator appearing in the rescanned line ---
        if (*scan_ptr == L'#' && *(scan_ptr + 1) == L'#')
        {
            wchar_t *lhs_str = NULL;
            wchar_t *rhs_str = NULL;
            wchar_t *pasted_str = NULL;
            PpSourceLocation error_loc_data = get_current_original_location(pp_state);
            error_loc_data.line = original_line_number_for_errors;
            error_loc_data.column = token_start_col_for_error; // Column of '##'

            // 1. Extract LHS from one_pass_buffer
            wchar_t *buffer_ptr = one_pass_buffer->buffer;
            size_t buffer_len = one_pass_buffer->length;
            wchar_t *lhs_token_end_in_buffer = buffer_ptr + buffer_len;
            while (lhs_token_end_in_buffer > buffer_ptr && iswspace(*(lhs_token_end_in_buffer - 1)))
            {
                lhs_token_end_in_buffer--;
            }
            wchar_t *lhs_token_start_in_buffer = lhs_token_end_in_buffer;
            while (lhs_token_start_in_buffer > buffer_ptr && !iswspace(*(lhs_token_start_in_buffer - 1)))
            {
                lhs_token_start_in_buffer--;
            }
            size_t lhs_len = lhs_token_end_in_buffer - lhs_token_start_in_buffer;

            if (lhs_len == 0 && one_pass_buffer->length > 0)
            { // If buffer has content but no token found before ##
              // This implies the LHS for ## is empty (placemarker).
              // This is valid, e.g. #define FOO ##BAR or #define FOO X##
              // In this case, lhs_str should be an empty string.
              // The previous logic might have issues if the buffer isn't just whitespace.
              // If buffer_len > 0 and lhs_len == 0, it implies buffer is all whitespace.
              // Or, it means the ## is at the very start of the line after macro expansion, e.g. MACRO_EMPTY##TOKEN
              // In this case, LHS is an empty token.
              // The standard: "If the token preceding the ## operator in a macro expansion is empty (placemarker)
              // the behavior is undefined." and "If the token following ## is empty, the behavior is undefined."
              // Let's handle it as an empty string for now, which is what `wcsndup_internal` with length 0 will do.
              // If lhs_len is 0, lhs_str will be an empty string. This is fine.
            }

            lhs_str = wcsndup_internal(lhs_token_start_in_buffer, lhs_len);
            if (!lhs_str)
            {
                *error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في تخصيص ذاكرة للطرف الأيسر من عامل اللصق ##.");
                *overall_success = false;
                break;
            }

            // Truncate one_pass_buffer to remove the LHS and any trailing whitespace before it.
            one_pass_buffer->length = lhs_token_start_in_buffer - buffer_ptr;
            one_pass_buffer->buffer[one_pass_buffer->length] = L'\0';

            // 2. Extract RHS from input_line_content (scan_ptr)
            const wchar_t *rhs_scan_temp = scan_ptr + 2; // Skip '##'
            current_col_in_this_scan_pass += 2;          // Account for '##'

            while (iswspace(*rhs_scan_temp))
            { // Skip leading whitespace for RHS
                rhs_scan_temp++;
                current_col_in_this_scan_pass++;
            }
            const wchar_t *rhs_token_start = rhs_scan_temp;
            const wchar_t *rhs_token_end = rhs_token_start;

            if (*rhs_token_start == L'\0')
            { // RHS is empty (placemarker)
                // This is generally undefined behavior in C if both are placemarkers, or one is.
                // For our implementation, we'll allow pasting with an empty string.
                rhs_len = 0;
            }
            else if (iswalpha(*rhs_token_end) || *rhs_token_end == L'_')
            { // Identifier
                while (iswalnum(*rhs_token_end) || *rhs_token_end == L'_')
                    rhs_token_end++;
            }
            else if (iswdigit(*rhs_token_end))
            { // PP-Number (simplified)
                // Allow sequence of digits. More complex numbers (float, hex) might need more robust tokenization.
                while (iswdigit(*rhs_token_end))
                    rhs_token_end++;
            }
            else if (*rhs_token_end != L'\0' && !iswspace(*rhs_token_end))
            {
                // Other non-whitespace token, e.g., punctuator. Take one char.
                // If multi-char punctuators need to be RHS of ##, this needs expansion.
                // For example, if one_pass_buffer="A" and input_line_content="##==", we want rhs_str to be "==".
                // For now, simplified:
                rhs_token_end++;
            }
            size_t rhs_len = rhs_token_end - rhs_token_start;

            rhs_str = wcsndup_internal(rhs_token_start, rhs_len);
            if (!rhs_str)
            {
                *error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في تخصيص ذاكرة للطرف الأيمن من عامل اللصق ##.");
                free(lhs_str);
                *overall_success = false;
                break;
            }
            current_col_in_this_scan_pass += rhs_len; // Update column by length of RHS

            // 3. Perform pasting
            // C standard: "If the result is not a valid preprocessing token, the behavior is undefined."
            // We just concatenate. Further rescanning will determine validity.
            size_t pasted_len_val = lhs_len + rhs_len;
            pasted_str = malloc((pasted_len_val + 1) * sizeof(wchar_t));
            if (!pasted_str)
            {
                *error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في تخصيص ذاكرة للرمز الملصق.");
                free(lhs_str);
                free(rhs_str);
                *overall_success = false;
                break;
            }
            wcscpy(pasted_str, lhs_str);
            wcscat(pasted_str, rhs_str);

            // 4. Append pasted result to one_pass_buffer
            if (!append_to_dynamic_buffer(one_pass_buffer, pasted_str))
            {
                *error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في إلحاق الرمز الملصق بالمخزن المؤقت.");
                free(lhs_str);
                free(rhs_str);
                free(pasted_str);
                *overall_success = false;
                break;
            }

            // 5. Advance main scan_ptr past '##' and the consumed RHS token
            scan_ptr = rhs_token_end;

            // 6. Mark that a transformation occurred
            transformation_occurred_this_pass = true;

            free(lhs_str);
            free(rhs_str);
            free(pasted_str);
            continue; // Continue the main loop from the new scan_ptr position
        }
        // --- END ## Handling ---
        else if (iswalpha(*scan_ptr) || *scan_ptr == L'_')
        { // Potential identifier (existing logic)
            const wchar_t *id_start = scan_ptr;
            while (iswalnum(*scan_ptr) || *scan_ptr == L'_')
            {
                scan_ptr++;
                current_col_in_this_scan_pass++;
            }
            size_t id_len = scan_ptr - id_start;
            wchar_t *identifier = wcsndup_internal(id_start, id_len);
            if (!identifier)
            {
                PpSourceLocation error_loc_data = get_current_original_location(pp_state);
                error_loc_data.line = original_line_number_for_errors;
                error_loc_data.column = token_start_col_for_error;
                *error_message = format_preprocessor_error_at_location(&error_loc_data, L"فشل في تخصيص ذاكرة للمعرف للاستبدال.");
                *overall_success = false;
                break;
            }

            bool predefined_expanded = false;
            if (wcscmp(identifier, L"__الملف__") == 0)
            {
                wchar_t quoted_file_path[MAX_PATH_LEN + 3]; // +2 for quotes, +1 for null
                PpSourceLocation orig_loc = get_current_original_location(pp_state);
                const char *path_for_macro = orig_loc.file_path ? orig_loc.file_path : "unknown_file";

                wchar_t w_path[MAX_PATH_LEN];
                size_t converted_len = mbstowcs(w_path, path_for_macro, MAX_PATH_LEN - 1); // -1 for null terminator
                if (converted_len == (size_t)-1)
                {
                    w_path[0] = L'\0'; /* Handle error or use placeholder */
                }
                else
                {
                    w_path[converted_len] = L'\0';
                }

                wchar_t escaped_path[MAX_PATH_LEN * 2 + 1]; // Max double length for all backslashes + null
                wchar_t *ep = escaped_path;
                for (const wchar_t *p = w_path; *p; ++p)
                {
                    if (ep >= escaped_path + (MAX_PATH_LEN * 2 - 1))
                        break; // Prevent overflow
                    if (*p == L'\\')
                    {
                        if (ep >= escaped_path + (MAX_PATH_LEN * 2 - 2))
                            break; // Need space for two '\'
                        *ep++ = L'\\';
                        *ep++ = L'\\';
                    }
                    else
                    {
                        *ep++ = *p;
                    }
                }
                *ep = L'\0';

                swprintf(quoted_file_path, sizeof(quoted_file_path) / sizeof(wchar_t), L"\"%ls\"", escaped_path);
                if (!append_to_dynamic_buffer(one_pass_buffer, quoted_file_path))
                {
                    *overall_success = false;
                }
                transformation_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__السطر__") == 0)
            {
                wchar_t line_str[20];
                swprintf(line_str, sizeof(line_str) / sizeof(wchar_t), L"%zu", original_line_number_for_errors);
                if (!append_to_dynamic_buffer(one_pass_buffer, line_str))
                {
                    *overall_success = false;
                }
                transformation_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__الدالة__") == 0)
            {
                // Ensure it's a wide string literal L"..."
                if (!append_to_dynamic_buffer(one_pass_buffer, L"L\"__BAA_FUNCTION_PLACEHOLDER__\""))
                {
                    *overall_success = false;
                }
                transformation_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__إصدار_المعيار_باء__") == 0)
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, L"10010L"))
                {
                    *overall_success = false;
                }
                transformation_occurred_this_pass = true;
                predefined_expanded = true;
            }

            if (predefined_expanded)
            {
                free(identifier);
                if (!*overall_success)
                    break;
                continue;
            }

            const BaaMacro *macro = find_macro(pp_state, identifier);
            if (macro && !is_macro_expanding(pp_state, macro))
            {
                PpSourceLocation invocation_loc_data = get_current_original_location(pp_state);
                invocation_loc_data.line = original_line_number_for_errors;
                invocation_loc_data.column = token_start_col_for_error;

                if (!push_location(pp_state, &invocation_loc_data))
                {
                    *error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"فشل في دفع موقع استدعاء الماكرو.");
                    *overall_success = false;
                    free(identifier);
                    break;
                }
                if (!push_macro_expansion(pp_state, macro))
                {
                    *error_message = format_preprocessor_error_at_location(&invocation_loc_data, L"فشل في دفع الماكرو '%ls' إلى مكدس التوسيع.", macro->name);
                    pop_location(pp_state);
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                DynamicWcharBuffer single_expansion_result;
                if (!init_dynamic_buffer(&single_expansion_result, 128))
                {
                    PpSourceLocation err_loc_init = get_current_original_location(pp_state);
                    *error_message = format_preprocessor_error_at_location(&err_loc_init, L"فشل تهيئة مخزن التوسيع.");
                    pop_macro_expansion(pp_state);
                    pop_location(pp_state);
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                bool current_expansion_succeeded = true;
                const wchar_t *ptr_after_invocation_args = scan_ptr; // Current position after identifier
                const wchar_t *arg_scan_ptr_start = scan_ptr;        // For function-like, where args would start

                if (macro->is_function_like)
                {
                    const wchar_t *arg_scan_ptr_local = scan_ptr;                 // Use a local copy for scanning args
                    size_t col_at_arg_scan_start = current_col_in_this_scan_pass; // Col *after* macro name

                    while (iswspace(*arg_scan_ptr_local))
                    { // Skip whitespace before expected '('
                        arg_scan_ptr_local++;
                        col_at_arg_scan_start++;
                    }

                    if (*arg_scan_ptr_local == L'(')
                    {
                        arg_scan_ptr_local++; // Consume '('
                        col_at_arg_scan_start++;

                        size_t original_pp_state_col_backup = pp_state->current_column_number; // Backup global column
                        // For errors *within* parse_macro_arguments, the column should be relative
                        // to the start of the arguments in the *current rescan line*.
                        // We need to pass the effective starting column of the arguments to it,
                        // or parse_macro_arguments needs to be more aware of its context.
                        // Let's assume parse_macro_arguments handles its internal column tracking for errors
                        // using the context from pp_state (which points to original file/line).
                        // The column for the #error itself will be based on `invocation_loc_data`.

                        size_t actual_arg_count = 0;
                        wchar_t **arguments = parse_macro_arguments(pp_state, &arg_scan_ptr_local, macro, &actual_arg_count, error_message);

                        ptr_after_invocation_args = arg_scan_ptr_local; // Update based on how much parse_macro_arguments consumed
                        // Adjust current_col_in_this_scan_pass to reflect consumed arguments
                        current_col_in_this_scan_pass = col_at_arg_scan_start + (arg_scan_ptr_local - (arg_scan_ptr_start + (col_at_arg_scan_start - current_col_in_this_scan_pass))); // This is complex, needs exact tracking in parse_macro_arguments or simpler approximation
                        // Simpler: update by total length consumed by args from original scan_ptr
                        // current_col_in_this_scan_pass = token_start_col_for_error + id_len + (ptr_after_invocation_args - scan_ptr);

                        if (!arguments)
                        {
                            current_expansion_succeeded = false; // Error message set by parse_macro_arguments
                        }
                        else
                        {
                            if (!substitute_macro_body(pp_state, &single_expansion_result, macro, arguments, actual_arg_count, error_message))
                            {
                                current_expansion_succeeded = false;
                            }
                            for (size_t i = 0; i < actual_arg_count; ++i)
                                free(arguments[i]);
                            free(arguments);
                        }
                        pp_state->current_column_number = original_pp_state_col_backup; // Restore global column
                    }
                    else
                    { // Function-like macro name not followed by '(', treat as object-like or plain identifier
                        if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                        {
                            *overall_success = false;
                        }
                        current_expansion_succeeded = false; // Not a real expansion in this case
                    }
                }
                else // Object-like macro
                {
                    if (!substitute_macro_body(pp_state, &single_expansion_result, macro, NULL, 0, error_message))
                    {
                        current_expansion_succeeded = false;
                    }
                }

                pop_macro_expansion(pp_state);
                pop_location(pp_state);

                if (current_expansion_succeeded)
                {
                    if (!append_to_dynamic_buffer(one_pass_buffer, single_expansion_result.buffer))
                    {
                        *overall_success = false;
                    }
                    else
                    {
                        transformation_occurred_this_pass = true;
                    }
                }
                else if (*overall_success && !*error_message)
                { // If an error occurred, error_message is set.
                    // If no error but expansion failed (e.g. func-like without '()'), append original.
                    if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                    {
                        *overall_success = false;
                    }
                }
                free_dynamic_buffer(&single_expansion_result);
                scan_ptr = ptr_after_invocation_args; // Update main scan_ptr
            }
            else // Not a defined macro, or is currently expanding (recursion guard)
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                {
                    *overall_success = false;
                }
            }
            free(identifier);
        }
        else // Not an identifier start, not ##
        {
            if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
            {
                *overall_success = false;
            }
            scan_ptr++;
            current_col_in_this_scan_pass++;
        }

        if (!*overall_success)
            break;
    }
    return transformation_occurred_this_pass;
}

bool process_code_line_for_macros(BaaPreprocessor *pp_state,
                                  const wchar_t *initial_current_line,
                                  size_t initial_line_len_unused,
                                  DynamicWcharBuffer *output_buffer,
                                  wchar_t **error_message)
{
    DynamicWcharBuffer current_pass_input_buffer;
    DynamicWcharBuffer current_pass_output_buffer;

    if (!init_dynamic_buffer(&current_pass_input_buffer, wcslen(initial_current_line) + 256))
    {
        PpSourceLocation temp_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في تهيئة المخزن المؤقت لسطر المعالجة (الإدخال).");
        return false;
    }
    if (!append_to_dynamic_buffer(&current_pass_input_buffer, initial_current_line))
    {
        PpSourceLocation temp_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في نسخ السطر الأولي إلى مخزن المعالجة (الإدخال).");
        free_dynamic_buffer(&current_pass_input_buffer);
        return false;
    }

    size_t original_line_number = pp_state->current_line_number;
    bool overall_success_for_line = true;
    int pass_count = 0;
    const int MAX_RESCAN_PASSES = 256;

    bool transformation_made_this_pass; // Renamed
    do
    {
        if (!init_dynamic_buffer(&current_pass_output_buffer, current_pass_input_buffer.length + 128))
        {
            overall_success_for_line = false;
            PpSourceLocation temp_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في تهيئة المخزن المؤقت لجولة الفحص (الإخراج).");
            break;
        }

        transformation_made_this_pass = scan_and_substitute_macros_one_pass(
            pp_state,
            current_pass_input_buffer.buffer,
            original_line_number,
            &current_pass_output_buffer,
            &overall_success_for_line,
            error_message);

        free_dynamic_buffer(&current_pass_input_buffer);

        // Transfer content from current_pass_output_buffer to current_pass_input_buffer for the next pass
        if (!init_dynamic_buffer(&current_pass_input_buffer, current_pass_output_buffer.length + 128))
        {
            overall_success_for_line = false;
            PpSourceLocation temp_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إعادة تهيئة مخزن الإدخال للجولة التالية.");
            free_dynamic_buffer(&current_pass_output_buffer);
            break;
        }
        if (current_pass_output_buffer.length > 0)
        {
            if (!append_to_dynamic_buffer(&current_pass_input_buffer, current_pass_output_buffer.buffer))
            {
                overall_success_for_line = false;
                PpSourceLocation temp_loc = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في نسخ محتوى الجولة إلى مخزن الإدخال التالي.");
                free_dynamic_buffer(&current_pass_output_buffer);
                break;
            }
        }
        free_dynamic_buffer(&current_pass_output_buffer);

        if (!overall_success_for_line)
            break;

        pass_count++;
        if (pass_count > MAX_RESCAN_PASSES)
        {
            PpSourceLocation err_loc_data = get_current_original_location(pp_state);
            err_loc_data.line = original_line_number;
            err_loc_data.column = 1;
            *error_message = format_preprocessor_error_at_location(&err_loc_data, L"تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لسطر واحد (%d).", MAX_RESCAN_PASSES);
            overall_success_for_line = false;
            break;
        }

    } while (transformation_made_this_pass && overall_success_for_line);

    if (overall_success_for_line)
    {
        if (current_pass_input_buffer.length > 0)
        {
            if (!append_to_dynamic_buffer(output_buffer, current_pass_input_buffer.buffer))
            {
                if (!*error_message)
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إلحاق السطر النهائي بمخزن الإخراج المؤقت.");
                }
                overall_success_for_line = false;
            }
        }

        bool initial_line_was_empty_or_all_whitespace = true;
        for (const wchar_t *p = initial_current_line; *p; ++p)
        {
            if (!iswspace(*p))
            {
                initial_line_was_empty_or_all_whitespace = false;
                break;
            }
        }

        if (!initial_line_was_empty_or_all_whitespace || current_pass_input_buffer.length > 0)
        {
            if (!append_to_dynamic_buffer(output_buffer, L"\n"))
            {
                if (!*error_message)
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    *error_message = format_preprocessor_error_at_location(&temp_loc, L"فشل في إلحاق سطر جديد بعد معالجة السطر.");
                }
                overall_success_for_line = false;
            }
        }
    }

    free_dynamic_buffer(&current_pass_input_buffer);
    return overall_success_for_line;
}
