#include "preprocessor_internal.h"

// Helper function to perform one pass of macro scanning and substitution for conditional expressions.
// This is identical to scan_and_substitute_macros_one_pass() WITH the special 'معرف' operator handling.
// This allows function-like macros to be fully expanded in conditional expressions (#إذا/#وإلا_إذا)
// while still preserving the correct behavior of the 'معرف' operator.
bool scan_and_expand_macros_for_expressions(
    BaaPreprocessor *pp_state,
    const wchar_t *input_line_content,
    size_t original_line_number_for_errors,
    DynamicWcharBuffer *one_pass_buffer,
    bool *overall_success,
    wchar_t **error_message)
{
    bool expansion_occurred_this_pass = false;
    const wchar_t *scan_ptr = input_line_content;

    size_t current_col_in_this_scan_pass = 1;

    while (*scan_ptr != L'\0' && *overall_success)
    {
        size_t token_start_col_for_error = current_col_in_this_scan_pass;

        if (iswalpha(*scan_ptr) || *scan_ptr == L'_')
        { // Potential identifier
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
                PP_REPORT_FATAL(pp_state, &error_loc_data, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في تخصيص ذاكرة للمعرف للاستبدال.");
                *overall_success = false;
                break;
            }

            bool predefined_expanded = false;
            if (wcscmp(identifier, L"__الملف__") == 0)
            {
                wchar_t quoted_file_path[MAX_PATH_LEN + 3];
                PpSourceLocation orig_loc = get_current_original_location(pp_state);
                const char *path_for_macro = orig_loc.file_path ? orig_loc.file_path : "unknown_file";
                wchar_t w_path[MAX_PATH_LEN];
                mbstowcs(w_path, path_for_macro, MAX_PATH_LEN);
                wchar_t escaped_path[MAX_PATH_LEN * 2];
                wchar_t *ep = escaped_path;
                for (const wchar_t *p = w_path; *p; ++p)
                {
                    if (*p == L'\\')
                        *ep++ = L'\\';
                    *ep++ = *p;
                }
                *ep = L'\0';
                swprintf(quoted_file_path, sizeof(quoted_file_path) / sizeof(wchar_t), L"\"%ls\"", escaped_path);
                if (!append_to_dynamic_buffer(one_pass_buffer, quoted_file_path))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مسار الملف المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__السطر__") == 0)
            {
                wchar_t line_str[20];
                swprintf(line_str, sizeof(line_str) / sizeof(wchar_t), L"%zu", original_line_number_for_errors);
                if (!append_to_dynamic_buffer(one_pass_buffer, line_str))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق رقم السطر المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__الدالة__") == 0)
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, L"\"__BAA_FUNCTION_PLACEHOLDER__\""))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق اسم الدالة المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__إصدار_المعيار_باء__") == 0)
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, L"10010L"))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق إصدار المعيار المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }

            if (predefined_expanded)
            {
                free(identifier);
                if (!*overall_success)
                    break;
                continue;
            }

            // Special handling for 'معرف' operator to prevent argument expansion
            if (wcscmp(identifier, L"معرف") == 0)
            {
                // Append 'معرف' literally to the output buffer
                if (!append_to_dynamic_buffer(one_pass_buffer, L"معرف"))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مشغل معرف.");
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                // Skip and copy whitespace after 'معرف'
                while (iswspace(*scan_ptr))
                {
                    if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = current_col_in_this_scan_pass;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مساحة بيضاء بعد معرف.");
                        *overall_success = false;
                        break;
                    }
                    scan_ptr++;
                    current_col_in_this_scan_pass++;
                }

                if (!*overall_success)
                {
                    free(identifier);
                    break;
                }

                bool has_parens = false;
                if (*scan_ptr == L'(')
                {
                    has_parens = true;
                    if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = current_col_in_this_scan_pass;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق قوس فتح لمعرف.");
                        *overall_success = false;
                        free(identifier);
                        break;
                    }
                    scan_ptr++;
                    current_col_in_this_scan_pass++;

                    // Copy whitespace inside parentheses
                    while (iswspace(*scan_ptr))
                    {
                        if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = current_col_in_this_scan_pass;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مساحة بيضاء داخل أقواس معرف.");
                            *overall_success = false;
                            break;
                        }
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }

                    if (!*overall_success)
                    {
                        free(identifier);
                        break;
                    }
                }

                // Copy the identifier argument without expansion
                const wchar_t *arg_start = scan_ptr;
                if (iswalpha(*scan_ptr) || *scan_ptr == L'_')
                {
                    while (iswalnum(*scan_ptr) || *scan_ptr == L'_')
                    {
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }
                    size_t arg_len = scan_ptr - arg_start;
                    if (!append_dynamic_buffer_n(one_pass_buffer, arg_start, arg_len))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = current_col_in_this_scan_pass - arg_len;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق وسيطة معرف.");
                        *overall_success = false;
                        free(identifier);
                        break;
                    }
                }
                // If not an identifier, let expression evaluator catch the error later

                if (has_parens)
                {
                    // Copy whitespace before closing paren
                    while (iswspace(*scan_ptr))
                    {
                        if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = current_col_in_this_scan_pass;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مساحة بيضاء قبل قوس الإغلاق لمعرف.");
                            *overall_success = false;
                            break;
                        }
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }

                    if (!*overall_success)
                    {
                        free(identifier);
                        break;
                    }

                    if (*scan_ptr == L')')
                    {
                        if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = current_col_in_this_scan_pass;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق قوس الإغلاق لمعرف.");
                            *overall_success = false;
                            free(identifier);
                            break;
                        }
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }
                    // If ')' missing, let expression evaluator handle the error
                }

                // Don't set expansion_occurred_this_pass = true here
                // because we didn't actually change anything - we just preserved it
                free(identifier);
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
                    PP_REPORT_FATAL(pp_state, &invocation_loc_data, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في دفع موقع استدعاء الماكرو.");
                    *overall_success = false;
                    free(identifier);
                    break;
                }
                if (!push_macro_expansion(pp_state, macro))
                {
                    PP_REPORT_FATAL(pp_state, &invocation_loc_data, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في دفع الماكرو '%ls' إلى مكدس التوسيع.", macro->name);
                    pop_location(pp_state);
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                DynamicWcharBuffer single_expansion_result;
                if (!init_dynamic_buffer(&single_expansion_result, 128))
                {
                    PpSourceLocation err_loc_init = get_current_original_location(pp_state);
                    PP_REPORT_FATAL(pp_state, &err_loc_init, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل تهيئة مخزن التوسيع.");
                    pop_macro_expansion(pp_state);
                    pop_location(pp_state);
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                bool current_expansion_succeeded = true;
                const wchar_t *ptr_after_invocation_args = scan_ptr;

                if (macro->is_function_like)
                {
                    const wchar_t *arg_scan_ptr = scan_ptr;
                    size_t col_at_arg_scan_start = current_col_in_this_scan_pass;
                    while (iswspace(*arg_scan_ptr))
                    {
                        arg_scan_ptr++;
                        col_at_arg_scan_start++;
                    }

                    if (*arg_scan_ptr == L'(')
                    {
                        arg_scan_ptr++;
                        col_at_arg_scan_start++;

                        size_t original_pp_state_col = pp_state->current_column_number;
                        pp_state->current_column_number = col_at_arg_scan_start;

                        size_t actual_arg_count = 0;
                        wchar_t **arguments = parse_macro_arguments(pp_state, &arg_scan_ptr, macro, &actual_arg_count, error_message);

                        ptr_after_invocation_args = arg_scan_ptr;
                        current_col_in_this_scan_pass = pp_state->current_column_number;
                        pp_state->current_column_number = original_pp_state_col;

                        if (!arguments)
                        {
                            current_expansion_succeeded = false;
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
                    }
                    else
                    { // Function-like macro name not followed by '(', treat as plain identifier
                        if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = token_start_col_for_error;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق اسم الماكرو كمعرف عادي.");
                            *overall_success = false;
                        }
                        current_expansion_succeeded = false;
                    }
                }
                else
                { // Object-like macro
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
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = token_start_col_for_error;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق نتيجة توسيع الماكرو.");
                        *overall_success = false;
                    }
                    else
                    {
                        if (macro->is_function_like || wcscmp(identifier, single_expansion_result.buffer) != 0)
                        {
                            expansion_occurred_this_pass = true;
                        }
                    }
                }
                else if (*overall_success && !*error_message && !macro->is_function_like)
                {
                    if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = token_start_col_for_error;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق المعرف غير الموسع.");
                        *overall_success = false;
                    }
                }
                else if (*overall_success && !*error_message && macro->is_function_like && (scan_ptr == ptr_after_invocation_args))
                {
                    // Function-like macro name was not followed by '(', already handled
                }
                free_dynamic_buffer(&single_expansion_result);
                scan_ptr = ptr_after_invocation_args;
            }
            else
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق المعرف العادي.");
                    *overall_success = false;
                }
            }
            free(identifier);
        }
        else
        { // Not an identifier start
            if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
            {
                PpSourceLocation temp_loc = get_current_original_location(pp_state);
                temp_loc.line = original_line_number_for_errors;
                temp_loc.column = current_col_in_this_scan_pass;
                PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق الحرف غير المعرف.");
                *overall_success = false;
            }
            scan_ptr++;
            current_col_in_this_scan_pass++;
        }
        if (!*overall_success)
            break;
    }
    return expansion_occurred_this_pass;
}

// Helper function to perform one pass of macro scanning and substitution.
// - input_line_content: The current version of the line to be scanned.
// - original_line_number_for_errors: The original line number in the source file, for error reporting.
// - one_pass_buffer: The dynamic buffer where the output of this single pass is built.
// - overall_success: Pointer to a boolean that will be set to false on critical errors.
// - error_message: Pointer to the error message string.
// Returns true if at least one macro expansion occurred during this pass, false otherwise.
// Made non-static to be callable from preprocessor_expr_eval.c
bool scan_and_substitute_macros_one_pass(
    BaaPreprocessor *pp_state,
    const wchar_t *input_line_content,
    size_t original_line_number_for_errors, // Keep this for context of the original line
    DynamicWcharBuffer *one_pass_buffer,
    bool *overall_success,
    wchar_t **error_message)
{
    bool expansion_occurred_this_pass = false;
    const wchar_t *scan_ptr = input_line_content;

    size_t current_col_in_this_scan_pass = 1;

    while (*scan_ptr != L'\0' && *overall_success)
    {
        size_t token_start_col_for_error = current_col_in_this_scan_pass;

        if (iswalpha(*scan_ptr) || *scan_ptr == L'_')
        { // Potential identifier
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
                PP_REPORT_FATAL(pp_state, &error_loc_data, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في تخصيص ذاكرة للمعرف للاستبدال.");
                *overall_success = false;
                break;
            }

            bool predefined_expanded = false;
            if (wcscmp(identifier, L"__الملف__") == 0)
            {
                wchar_t quoted_file_path[MAX_PATH_LEN + 3];
                PpSourceLocation orig_loc = get_current_original_location(pp_state);
                const char *path_for_macro = orig_loc.file_path ? orig_loc.file_path : "unknown_file";
                wchar_t w_path[MAX_PATH_LEN];
                mbstowcs(w_path, path_for_macro, MAX_PATH_LEN);
                wchar_t escaped_path[MAX_PATH_LEN * 2];
                wchar_t *ep = escaped_path;
                for (const wchar_t *p = w_path; *p; ++p)
                {
                    if (*p == L'\\')
                        *ep++ = L'\\';
                    *ep++ = *p;
                }
                *ep = L'\0';
                swprintf(quoted_file_path, sizeof(quoted_file_path) / sizeof(wchar_t), L"\"%ls\"", escaped_path);
                if (!append_to_dynamic_buffer(one_pass_buffer, quoted_file_path))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مسار الملف المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__السطر__") == 0)
            {
                wchar_t line_str[20];
                // __السطر__ should refer to the line number of the current line being processed by process_code_line_for_macros
                swprintf(line_str, sizeof(line_str) / sizeof(wchar_t), L"%zu", original_line_number_for_errors); // Standard: integer
                if (!append_to_dynamic_buffer(one_pass_buffer, line_str))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق رقم السطر المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__الدالة__") == 0)
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, L"\"__BAA_FUNCTION_PLACEHOLDER__\""))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق اسم الدالة المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }
            else if (wcscmp(identifier, L"__إصدار_المعيار_باء__") == 0)
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, L"10010L"))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق إصدار المعيار المحدد مسبقاً.");
                    *overall_success = false;
                }
                expansion_occurred_this_pass = true;
                predefined_expanded = true;
            }

            if (predefined_expanded)
            {
                free(identifier);
                if (!*overall_success)
                    break;
                continue;
            }

            // Special handling for 'معرف' operator to prevent argument expansion
            if (wcscmp(identifier, L"معرف") == 0)
            {
                // Append 'معرف' literally to the output buffer
                if (!append_to_dynamic_buffer(one_pass_buffer, L"معرف"))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مشغل معرف.");
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                // Skip and copy whitespace after 'معرف'
                while (iswspace(*scan_ptr))
                {
                    if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = current_col_in_this_scan_pass;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مساحة بيضاء بعد معرف.");
                        *overall_success = false;
                        break;
                    }
                    scan_ptr++;
                    current_col_in_this_scan_pass++;
                }

                if (!*overall_success)
                {
                    free(identifier);
                    break;
                }

                bool has_parens = false;
                if (*scan_ptr == L'(')
                {
                    has_parens = true;
                    if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = current_col_in_this_scan_pass;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق قوس فتح لمعرف.");
                        *overall_success = false;
                        free(identifier);
                        break;
                    }
                    scan_ptr++;
                    current_col_in_this_scan_pass++;

                    // Copy whitespace inside parentheses
                    while (iswspace(*scan_ptr))
                    {
                        if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = current_col_in_this_scan_pass;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مساحة بيضاء داخل أقواس معرف.");
                            *overall_success = false;
                            break;
                        }
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }

                    if (!*overall_success)
                    {
                        free(identifier);
                        break;
                    }
                }

                // Copy the identifier argument without expansion
                const wchar_t *arg_start = scan_ptr;
                if (iswalpha(*scan_ptr) || *scan_ptr == L'_')
                {
                    while (iswalnum(*scan_ptr) || *scan_ptr == L'_')
                    {
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }
                    size_t arg_len = scan_ptr - arg_start;
                    if (!append_dynamic_buffer_n(one_pass_buffer, arg_start, arg_len))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = current_col_in_this_scan_pass - arg_len;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق وسيطة معرف.");
                        *overall_success = false;
                        free(identifier);
                        break;
                    }
                }
                // If not an identifier, let expression evaluator catch the error later

                if (has_parens)
                {
                    // Copy whitespace before closing paren
                    while (iswspace(*scan_ptr))
                    {
                        if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = current_col_in_this_scan_pass;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق مساحة بيضاء قبل قوس الإغلاق لمعرف.");
                            *overall_success = false;
                            break;
                        }
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }

                    if (!*overall_success)
                    {
                        free(identifier);
                        break;
                    }

                    if (*scan_ptr == L')')
                    {
                        if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = current_col_in_this_scan_pass;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق قوس الإغلاق لمعرف.");
                            *overall_success = false;
                            free(identifier);
                            break;
                        }
                        scan_ptr++;
                        current_col_in_this_scan_pass++;
                    }
                    // If ')' missing, let expression evaluator handle the error
                }

                // Don't set expansion_occurred_this_pass = true here
                // because we didn't actually change anything - we just preserved it
                free(identifier);
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
                    PP_REPORT_FATAL(pp_state, &invocation_loc_data, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في دفع موقع استدعاء الماكرو.");
                    *overall_success = false;
                    free(identifier);
                    break;
                }
                if (!push_macro_expansion(pp_state, macro))
                {
                    PP_REPORT_FATAL(pp_state, &invocation_loc_data, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في دفع الماكرو '%ls' إلى مكدس التوسيع.", macro->name);
                    pop_location(pp_state);
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                DynamicWcharBuffer single_expansion_result;
                if (!init_dynamic_buffer(&single_expansion_result, 128))
                {
                    PpSourceLocation err_loc_init = get_current_original_location(pp_state); // Should be invocation_loc_data
                    PP_REPORT_FATAL(pp_state, &err_loc_init, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل تهيئة مخزن التوسيع.");
                    pop_macro_expansion(pp_state);
                    pop_location(pp_state);
                    *overall_success = false;
                    free(identifier);
                    break;
                }

                bool current_expansion_succeeded = true;
                const wchar_t *ptr_after_invocation_args = scan_ptr;

                if (macro->is_function_like)
                {
                    const wchar_t *arg_scan_ptr = scan_ptr;
                    size_t col_at_arg_scan_start = current_col_in_this_scan_pass;
                    while (iswspace(*arg_scan_ptr))
                    {
                        arg_scan_ptr++;
                        col_at_arg_scan_start++;
                    }

                    if (*arg_scan_ptr == L'(')
                    {
                        arg_scan_ptr++;
                        col_at_arg_scan_start++;

                        size_t original_pp_state_col = pp_state->current_column_number;
                        pp_state->current_column_number = col_at_arg_scan_start;

                        size_t actual_arg_count = 0;
                        // Pass the macro itself to handle variadic argument parsing
                        wchar_t **arguments = parse_macro_arguments(pp_state, &arg_scan_ptr, macro, &actual_arg_count, error_message);

                        ptr_after_invocation_args = arg_scan_ptr;
                        current_col_in_this_scan_pass = pp_state->current_column_number; // This might need adjustment based on how parse_macro_arguments updates column
                        pp_state->current_column_number = original_pp_state_col;

                        if (!arguments)
                        {
                            current_expansion_succeeded = false; // Error message already set by parse_macro_arguments
                        }
                        else
                        {
                            // parse_macro_arguments now handles count validation for non-variadic
                            // and ensures actual_arg_count is param_count + 1 for variadic (even if VA_ARGS is empty)
                            // So, no explicit count check needed here if arguments is not NULL.
                            if (!substitute_macro_body(pp_state, &single_expansion_result, macro, arguments, actual_arg_count, error_message))
                            {
                                current_expansion_succeeded = false;
                            }
                            for (size_t i = 0; i < actual_arg_count; ++i)
                                free(arguments[i]);
                            free(arguments);
                        }
                    }
                    else
                    { // Function-like macro name not followed by '(', treat as object-like or plain identifier
                        if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                        {
                            PpSourceLocation temp_loc = get_current_original_location(pp_state);
                            temp_loc.line = original_line_number_for_errors;
                            temp_loc.column = token_start_col_for_error;
                            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق اسم الماكرو كمعرف عادي.");
                            *overall_success = false;
                        }
                        current_expansion_succeeded = false; // Not a valid function-like macro expansion
                    }
                }
                else
                { // Object-like macro
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
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = token_start_col_for_error;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق نتيجة توسيع الماكرو.");
                        *overall_success = false;
                    }
                    else
                    {
                        if (macro->is_function_like || wcscmp(identifier, single_expansion_result.buffer) != 0)
                        {
                            expansion_occurred_this_pass = true;
                        }
                    }
                }
                else if (*overall_success && !*error_message && !macro->is_function_like)
                {
                    if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                    {
                        PpSourceLocation temp_loc = get_current_original_location(pp_state);
                        temp_loc.line = original_line_number_for_errors;
                        temp_loc.column = token_start_col_for_error;
                        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق المعرف غير الموسع.");
                        *overall_success = false;
                    }
                }
                else if (*overall_success && !*error_message && macro->is_function_like && (scan_ptr == ptr_after_invocation_args))
                {
                    // This means function-like macro name was not followed by '(', already handled by appending identifier.
                }
                free_dynamic_buffer(&single_expansion_result);
                scan_ptr = ptr_after_invocation_args;
            }
            else
            {
                if (!append_to_dynamic_buffer(one_pass_buffer, identifier))
                {
                    PpSourceLocation temp_loc = get_current_original_location(pp_state);
                    temp_loc.line = original_line_number_for_errors;
                    temp_loc.column = token_start_col_for_error;
                    PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق المعرف العادي.");
                    *overall_success = false;
                }
            }
            free(identifier);
        }
        else
        { // Not an identifier start
            // Re-typed call to append_dynamic_buffer_n
            if (!append_dynamic_buffer_n(one_pass_buffer, scan_ptr, 1))
            {
                PpSourceLocation temp_loc = get_current_original_location(pp_state);
                temp_loc.line = original_line_number_for_errors;
                temp_loc.column = current_col_in_this_scan_pass;
                PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق الحرف غير المعرف.");
                *overall_success = false;
            }
            scan_ptr++;
            current_col_in_this_scan_pass++;
        }
        if (!*overall_success)
            break;
    }
    return expansion_occurred_this_pass;
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
        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في تهيئة المخزن المؤقت لسطر المعالجة (الإدخال).");
        return false;
    }
    if (!append_to_dynamic_buffer(&current_pass_input_buffer, initial_current_line))
    {
        PpSourceLocation temp_loc = get_current_original_location(pp_state);
        PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في نسخ السطر الأولي إلى مخزن المعالجة (الإدخال).");
        free_dynamic_buffer(&current_pass_input_buffer);
        return false;
    }

    size_t original_line_number = pp_state->current_line_number;
    bool overall_success_for_line = true;
    int pass_count = 0;
    const int MAX_RESCAN_PASSES = 256;

    bool expansion_made_this_pass;
    do
    {
        if (!init_dynamic_buffer(&current_pass_output_buffer, current_pass_input_buffer.length + 128))
        {
            overall_success_for_line = false;
            PpSourceLocation temp_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في تهيئة المخزن المؤقت لجولة الفحص (الإخراج).");
            break;
        }

        expansion_made_this_pass = scan_and_substitute_macros_one_pass(
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
            PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إعادة تهيئة مخزن الإدخال للجولة التالية.");
            free_dynamic_buffer(&current_pass_output_buffer);
            break;
        }
        if (current_pass_output_buffer.length > 0)
        {
            if (!append_to_dynamic_buffer(&current_pass_input_buffer, current_pass_output_buffer.buffer))
            {
                overall_success_for_line = false;
                PpSourceLocation temp_loc = get_current_original_location(pp_state);
                PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في نسخ محتوى الجولة إلى مخزن الإدخال التالي.");
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
            PP_REPORT_ERROR(pp_state, &err_loc_data, PP_ERROR_MACRO_TOO_COMPLEX, "line_processing", L"تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لسطر واحد (%d).", MAX_RESCAN_PASSES);
            overall_success_for_line = false;
            break;
        }

    } while (expansion_made_this_pass && overall_success_for_line);

    if (overall_success_for_line)
    {
        if (!append_to_dynamic_buffer(output_buffer, current_pass_input_buffer.buffer))
        {
            if (!*error_message)
            {
                PpSourceLocation temp_loc = get_current_original_location(pp_state);
                PP_REPORT_FATAL(pp_state, &temp_loc, PP_ERROR_ALLOCATION_FAILED, "line_processing", L"فشل في إلحاق السطر النهائي بمخزن الإخراج المؤقت.");
            }
            overall_success_for_line = false;
        }
    }

    free_dynamic_buffer(&current_pass_input_buffer);
    return overall_success_for_line;
}
