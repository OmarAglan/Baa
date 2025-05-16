#include "preprocessor_internal.h"

// --- Core Recursive Processing Function ---

// Processes a single file, handling includes recursively.
// Returns a dynamically allocated string with processed content (caller frees), or NULL on error.
wchar_t *process_file(BaaPreprocessor *pp_state, const char *file_path, wchar_t **error_message)
{
    *error_message = NULL;

    // Store previous context for restoration after include/error
    const char *prev_file_path = pp_state->current_file_path;
    size_t prev_line_number = pp_state->current_line_number;

    char *abs_path = get_absolute_path(file_path);
    if (!abs_path)
    {
        // Get the location of the #include directive that caused this error
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في الحصول على المسار المطلق لملف التضمين '%hs'.", file_path);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // Set current context for this file
    pp_state->current_file_path = abs_path;
    pp_state->current_line_number = 1;
    pp_state->current_column_number = 1;

    // 1. Circular Include Check
    if (!push_file_stack(pp_state, abs_path))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"تم اكتشاف تضمين دائري: الملف '%hs' مضمن بالفعل.", abs_path);
        free(abs_path);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // 2. Read File Content
    wchar_t *file_content = read_file_content(pp_state, abs_path, error_message);
    if (!file_content)
    {
        pop_file_stack(pp_state);
        free(abs_path);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    DynamicWcharBuffer output_buffer;
    if (!init_dynamic_buffer(&output_buffer, wcslen(file_content) + 1024))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت.");
        free(file_content);
        pop_file_stack(pp_state);
        free(abs_path);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // 3. Process Lines and Directives
    wchar_t *line_start_in_file_content = file_content; // Iterator for the original file_content
    wchar_t *next_line_start_in_file_content;
    bool success = true;

    while (*line_start_in_file_content != L'\0' && success)
    {
        next_line_start_in_file_content = wcschr(line_start_in_file_content, L'\n');
        size_t current_line_physical_len;
        bool has_newline_char;

        if (next_line_start_in_file_content != NULL)
        {
            current_line_physical_len = next_line_start_in_file_content - line_start_in_file_content;
            has_newline_char = true;
        }
        else
        {
            current_line_physical_len = wcslen(line_start_in_file_content);
            has_newline_char = false;
        }

        // Duplicate the current physical line segment for analysis and potential modification.
        // This is the line that will be passed to macro processing if it's not a directive.
        wchar_t *current_physical_line_buffer = wcsndup_internal(line_start_in_file_content, current_line_physical_len);
        if (!current_physical_line_buffer)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لسطر.");
            success = false;
            break;
        }
        // current_physical_line_buffer is now null-terminated.

        // fwprintf(stderr, L"DEBUG CORE: Raw physical line %zu: [%ls]\n", pp_state->current_line_number, current_physical_line_buffer);

        pp_state->current_column_number = 1; // Reset column for scanning this physical line

        const wchar_t *scan_ptr = current_physical_line_buffer;
        wchar_t *effective_directive_char_location = NULL; // Will point to '#' if found

        // Scan the current physical line to find if it's a directive line,
        // while correctly managing in_multiline_comment state for this line.
        while (*scan_ptr)
        {
            if (pp_state->in_multiline_comment)
            {
                const wchar_t *comment_end_marker = wcsstr(scan_ptr, L"*/");
                if (comment_end_marker)
                {
                    scan_ptr = comment_end_marker + 2; // Move past "*/"
                    pp_state->in_multiline_comment = false;
                }
                else
                {
                    scan_ptr += wcslen(scan_ptr); // Rest of line is part of multi-line comment
                }
            }
            else
            { // Not currently in a multi-line comment (that started on a previous line)
                if (iswspace(*scan_ptr))
                {
                    scan_ptr++; // Skip whitespace
                }
                else if (wcsncmp(scan_ptr, L"/*", 2) == 0)
                {
                    pp_state->in_multiline_comment = true;
                    const wchar_t *comment_end_marker = wcsstr(scan_ptr + 2, L"*/");
                    if (comment_end_marker)
                    {
                        scan_ptr = comment_end_marker + 2; // Move past "*/"
                        pp_state->in_multiline_comment = false;
                    }
                    else
                    {
                        scan_ptr += wcslen(scan_ptr); // Rest of line starts multi-line comment
                    }
                }
                else if (wcsncmp(scan_ptr, L"//", 2) == 0)
                {
                    scan_ptr += wcslen(scan_ptr); // Rest of line is single-line comment
                }
                else
                {
                    // Found a non-whitespace, non-comment character. This is where a directive or code would start.
                    effective_directive_char_location = (wchar_t *)scan_ptr;
                    break;
                }
            }
        }

        // fwprintf(stderr, L"DEBUG CORE: Effective directive start for line %zu: [%ls]\n", pp_state->current_line_number, effective_directive_char_location ? effective_directive_char_location : L"NULL (all comment/ws or ended comment)");

        if (effective_directive_char_location && effective_directive_char_location[0] == L'#')
        {
            // fwprintf(stderr, L"DEBUG CORE PF: Directive found. Line for directive: [%ls]\n", effective_directive_char_location);
            bool local_is_conditional_directive_pf;
            // Pass the part of the line from '#' onwards.
            // handle_preprocessor_directive needs to be comment-aware for its arguments.
            if (!handle_preprocessor_directive(pp_state, effective_directive_char_location + 1, abs_path, &output_buffer, error_message, &local_is_conditional_directive_pf))
            {
                success = false;
            }
            // Most directives don't output their own line to the final preprocessed code.
            // #include's output is handled recursively by appending to output_buffer.
            // A newline is added to maintain line count for the lexer.
            // If the directive itself was conditional and caused skipping to start/stop,
            // process_code_line_for_macros will correctly not be called or be called.
            // We just need to ensure a newline is outputted to represent this line.
            if (has_newline_char && !append_to_dynamic_buffer(&output_buffer, L"\n"))
            {
                success = false;
                if (!*error_message && success)
                { // Only set error if not already set and still successful
                    *error_message = _wcsdup(L"Failed to append newline after directive processing.");
                }
            }
        }
        else if (!pp_state->skipping_lines)
        {
            // Not a directive and not skipping: pass the original physical line to macro processing.
            // This function is now responsible for outputting comments and expanded macros, AND the final newline.
            // fwprintf(stderr, L"DEBUG CORE PF: Code line. Processing for macros: [%ls]\n", current_physical_line_buffer);
            if (!process_code_line_for_macros(pp_state, current_physical_line_buffer, wcslen(current_physical_line_buffer), &output_buffer, error_message))
            {
                success = false;
            }
            // process_code_line_for_macros is expected to add its own newline if the line wasn't empty after processing.
        }
        else // Skipping lines due to conditional compilation
        {
            // fwprintf(stderr, L"DEBUG CORE PF: Skipping line %zu: [%ls]\n", pp_state->current_line_number, current_physical_line_buffer);
            // Append a newline to preserve line count for the lexer if the original line had one.
            if (has_newline_char && !append_to_dynamic_buffer(&output_buffer, L"\n"))
            {
                success = false;
                if (!*error_message && success)
                {
                    *error_message = _wcsdup(L"Failed to append newline for skipped line.");
                }
            }
        }

        free(current_physical_line_buffer); // Free the duplicated line segment

        if (!success)
            break;

        if (has_newline_char)
        {
            line_start_in_file_content = next_line_start_in_file_content + 1; // Move to the start of the next line
            pp_state->current_line_number++;
        }
        else
        {
            break; // End of file content (last line had no newline)
        }
    }

    // After processing all lines of a file, if still in_multiline_comment, it's an error
    if (success && pp_state->in_multiline_comment)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        // Correct the line number for the error to be the last processed line
        error_loc.line = pp_state->current_line_number;
        *error_message = format_preprocessor_error_at_location(&error_loc, L"تعليق متعدد الأسطر غير منتهٍ في نهاية الملف '%hs'.", pp_state->current_file_path);
        success = false;
    }

    if (!success)
    {
        free_dynamic_buffer(&output_buffer);
        free(file_content);
        pop_file_stack(pp_state);
        free(abs_path);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    free(file_content);

    pop_file_stack(pp_state);
    free(abs_path);
    pp_state->current_file_path = prev_file_path;
    pp_state->current_line_number = prev_line_number;

    return output_buffer.buffer;
}

// Processes a string directly, handling directives and macros.
// Returns a dynamically allocated string with processed content (caller frees), or NULL on error.
wchar_t *process_string(BaaPreprocessor *pp_state, const wchar_t *source_string, wchar_t **error_message)
{
    *error_message = NULL;

    const char *prev_file_path = pp_state->current_file_path;
    size_t prev_line_number = pp_state->current_line_number;

    // For string input, the "file path" in location stack is the source_name (e.g., "<input_string>")
    // Ensure current_file_path is set based on the location stack, which should have been pushed by baa_preprocess
    if (pp_state->location_stack_count > 0)
    {
        pp_state->current_file_path = pp_state->location_stack[pp_state->location_stack_count - 1].file_path;
    }
    else
    {
        // Fallback, though location stack should always be pushed by baa_preprocess
        pp_state->current_file_path = "<string_source_no_loc_stack>";
    }
    pp_state->current_line_number = 1;
    pp_state->current_column_number = 1;

    DynamicWcharBuffer output_buffer;
    if (!init_dynamic_buffer(&output_buffer, wcslen(source_string) + 1024))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت للسلسلة.");
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    wchar_t *line_start_in_source_string = (wchar_t *)source_string;
    wchar_t *next_line_start_in_source_string;
    bool success = true;

    while (*line_start_in_source_string != L'\0' && success)
    {
        next_line_start_in_source_string = wcschr(line_start_in_source_string, L'\n');
        size_t current_line_physical_len;
        bool has_newline_char;

        if (next_line_start_in_source_string != NULL)
        {
            current_line_physical_len = next_line_start_in_source_string - line_start_in_source_string;
            has_newline_char = true;
        }
        else
        {
            current_line_physical_len = wcslen(line_start_in_source_string);
            has_newline_char = false;
        }

        wchar_t *current_physical_line_buffer = wcsndup_internal(line_start_in_source_string, current_line_physical_len);
        if (!current_physical_line_buffer)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لسطر من السلسلة.");
            success = false;
            break;
        }

        pp_state->current_column_number = 1;

        const wchar_t *scan_ptr = current_physical_line_buffer;
        wchar_t *effective_directive_start = NULL;

        // Scan logic copied from process_file
        while (*scan_ptr)
        {
            if (pp_state->in_multiline_comment)
            {
                const wchar_t *comment_end_marker = wcsstr(scan_ptr, L"*/");
                if (comment_end_marker)
                {
                    scan_ptr = comment_end_marker + 2;
                    pp_state->in_multiline_comment = false;
                }
                else
                {
                    scan_ptr += wcslen(scan_ptr);
                }
            }
            else
            {
                if (iswspace(*scan_ptr))
                {
                    scan_ptr++;
                }
                else if (wcsncmp(scan_ptr, L"/*", 2) == 0)
                {
                    pp_state->in_multiline_comment = true;
                    const wchar_t *comment_end_marker = wcsstr(scan_ptr + 2, L"*/");
                    if (comment_end_marker)
                    {
                        scan_ptr = comment_end_marker + 2;
                        pp_state->in_multiline_comment = false;
                    }
                    else
                    {
                        scan_ptr += wcslen(scan_ptr);
                    }
                }
                else if (wcsncmp(scan_ptr, L"//", 2) == 0)
                {
                    scan_ptr += wcslen(scan_ptr);
                }
                else
                {
                    effective_directive_start = (wchar_t *)scan_ptr;
                    break;
                }
            }
        }

        if (effective_directive_start && effective_directive_start[0] == L'#')
        {
            // fwprintf(stderr, L"DEBUG PS: Directive found. Line for directive: [%ls]\n", effective_directive_start);
            bool local_is_conditional_directive_ps;
            // Note: abs_path is NULL for string processing. handle_preprocessor_directive must cope.
            // For #include from a string source, it should likely be an error or disallowed by handle_preprocessor_directive.
            if (!handle_preprocessor_directive(pp_state, effective_directive_start + 1, NULL /* No abs_path for string */, &output_buffer, error_message, &local_is_conditional_directive_ps))
            {
                success = false;
            }
            if (has_newline_char && !append_to_dynamic_buffer(&output_buffer, L"\n"))
            {
                success = false;
                if (!*error_message && success)
                    *error_message = _wcsdup(L"Failed to append newline after directive (string).");
            }
        }
        else if (!pp_state->skipping_lines)
        {
            // fwprintf(stderr, L"DEBUG PS: Code line. Processing for macros: [%ls]\n", current_physical_line_buffer);
            if (!process_code_line_for_macros(pp_state, current_physical_line_buffer, wcslen(current_physical_line_buffer), &output_buffer, error_message))
            {
                success = false;
            }
        }
        else
        {
            // fwprintf(stderr, L"DEBUG PS: Skipping line %zu: [%ls]\n", pp_state->current_line_number, current_physical_line_buffer);
            if (has_newline_char && !append_to_dynamic_buffer(&output_buffer, L"\n"))
            {
                success = false;
                if (!*error_message && success)
                    *error_message = _wcsdup(L"Failed to append newline for skipped line (string).");
            }
        }

        free(current_physical_line_buffer);

        if (!success)
            break;

        if (has_newline_char)
        {
            line_start_in_source_string = next_line_start_in_source_string + 1;
            pp_state->current_line_number++;
        }
        else
        {
            break;
        }
    }

    // After processing all lines of a string, if still in_multiline_comment, it's an error
    if (success && pp_state->in_multiline_comment)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        error_loc.line = pp_state->current_line_number; // Use the last line number processed
        *error_message = format_preprocessor_error_at_location(&error_loc, L"تعليق متعدد الأسطر غير منتهٍ في نهاية السلسلة المعالجة.");
        success = false;
    }
    if (!success)
    {
        free_dynamic_buffer(&output_buffer);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    pp_state->current_file_path = prev_file_path;
    pp_state->current_line_number = prev_line_number;

    return output_buffer.buffer;
}
