// preprocessor_core.c
#include "preprocessor_internal.h"
#include <wctype.h>
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
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_INVALID_FILE_PATH, "file",
                       L"فشل في الحصول على المسار المطلق لملف التضمين '%hs'.", file_path);
        // Restore context before returning on error (though it might be the same)
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // Check for #براغما مرة_واحدة before processing
    if (is_pragma_once_file(pp_state, abs_path))
    {
        // File is marked with #براغما مرة_واحدة and already processed, skip it
        free(abs_path);
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        
        // Return empty content (successfully processed, but no output)
        wchar_t *empty_result = malloc(sizeof(wchar_t));
        if (empty_result)
        {
            empty_result[0] = L'\0';
        }
        return empty_result;
    }

    // Set current context for this file
    pp_state->current_file_path = abs_path; // Use the allocated absolute path
    pp_state->current_line_number = 1;      // Start at line 1
    pp_state->current_column_number = 1;    // Start at column 1

    // 1. Circular Include Check
    if (!push_file_stack(pp_state, abs_path))
    {
        // Use the location stack to report the error at the include site
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_CIRCULAR_INCLUDE, "file",
                       L"تم اكتشاف تضمين دائري: الملف '%hs' مضمن بالفعل.", abs_path);
        free(abs_path);
        pp_state->current_file_path = prev_file_path; // Restore before returning
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // 2. Read File Content (handles UTF-8 and UTF-16LE with BOM detection)
    wchar_t *file_content = read_file_content(pp_state, abs_path, error_message);
    if (!file_content)
    {
        // error_message should be set by read_file_content using current physical context
        pop_file_stack(pp_state); // Pop before returning error
        free(abs_path);
        pp_state->current_file_path = prev_file_path; // Restore before returning
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    DynamicWcharBuffer output_buffer;
    // Estimate initial capacity: file length + some overhead
    if (!init_dynamic_buffer(&output_buffer, wcslen(file_content) + 1024))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state); // Get location before error
        PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory",
                       L"فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت.");
        free(file_content);
        pop_file_stack(pp_state);
        free(abs_path);
        pp_state->current_file_path = prev_file_path; // Restore before returning
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // 3. Process Lines and Directives
    wchar_t *line_start = file_content;
    wchar_t *line_end;
    bool success = true;

    while (*line_start != L'\0' && success)
    {
        line_end = wcschr(line_start, L'\n');
        size_t line_len;
        if (line_end != NULL)
        {
            line_len = line_end - line_start;
            // Handle \r\n endings
            if (line_len > 0 && line_start[line_len - 1] == L'\r')
            {
                line_len--;
            }
        }
        else
        {
            // Last line without newline
            line_len = wcslen(line_start);
        }

        // Create a temporary buffer for the current line (use internal wcsndup)
        wchar_t *current_line = wcsndup_internal(line_start, line_len);
        if (!current_line)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory",
                           L"فشل في تخصيص الذاكرة لسطر.");
            success = false;
            break;
        }

        // fwprintf(stderr, L"DEBUG: Processing line %zu: [%ls]\n", pp_state->current_line_number, current_line); // DEBUG PRINT

        // Reset physical column for the start of the line
        pp_state->current_column_number = 1;

        // --- Skip Comments FIRST ---
        wchar_t *effective_line_start = current_line;
        while (iswspace(*effective_line_start))
        { // Skip leading whitespace on the line
            effective_line_start++;
            pp_state->current_column_number++;
        }

        if (wcsncmp(effective_line_start, L"//", 2) == 0)
        {
            // Single-line comment, skip the rest of the line processing
            free(current_line); // Free the duplicated line
            // Advance to next line in the main loop
            if (line_end != NULL)
            {
                line_start = line_end + 1;
                pp_state->current_line_number++;
                // Update the location stack with the current line number for accurate error reporting
                update_current_location(pp_state, pp_state->current_line_number, 1);
            }
            else
            {
                break; // End of file
            }
            continue; // Go to next iteration of the while loop
        }
        // TODO: Add handling for multi-line comments /* */ which might span lines.
        // This requires more state tracking across loop iterations.
        // For now, focusing on the single-line comment issue.

        // Check for directives (use effective_line_start which has whitespace skipped)
        if (effective_line_start[0] == L'#')
        {
            // Call the new function to handle all directive logic.
            // This function is responsible for its own output (e.g., from #include via output_buffer)
            // and for updating the success flag.
            // fwprintf(stderr, L"DEBUG PF: Calling handle_preprocessor_directive for line %zu: [%ls]\n", pp_state->current_line_number, effective_line_start);
            bool local_is_conditional_directive_pf; // For process_file
            if (!handle_preprocessor_directive(pp_state, effective_line_start + 1, abs_path, &output_buffer, error_message, &local_is_conditional_directive_pf))
            {
                success = false;
            }
        }
        else if (!pp_state->skipping_lines)
        {
            // Not a directive and not skipping: process line for macro substitution.
            // The process_code_line_for_macros function appends the processed line (without newline) to output_buffer.
            // fwprintf(stderr, L"DEBUG PF: Calling process_code_line_for_macros for line %zu: [%ls]\n", pp_state->current_line_number, current_line);
            if (!process_code_line_for_macros(pp_state, current_line, line_len, &output_buffer, error_message))
            {
                success = false;
            }
            else
            {
                // Append newline after successfully processing and appending the code line
                if (!append_to_dynamic_buffer(&output_buffer, L"\n"))
                {
                    success = false;
                    if (!*error_message) {
                        PpSourceLocation current_loc = get_current_original_location(pp_state);
                        PP_REPORT_FATAL(pp_state, &current_loc, PP_ERROR_BUFFER_OVERFLOW, "memory",
                                       L"فشل في إلحاق السطر بمخزن الإخراج المؤقت.");
                    }
                }
            }
        }
        // If skipping_lines is true and it's not a directive handled above, the line is effectively skipped (no output generated here).
        // Comment lines (//) are handled before this block and 'continue' the loop.

        free(current_line); // Free the duplicated line

        if (!success)
            break; // Exit loop on error

        if (line_end != NULL)
        {
            line_start = line_end + 1;       // Move to the next line
            pp_state->current_line_number++; // Increment line number
            // Update the location stack with the current line number for accurate error reporting
            update_current_location(pp_state, pp_state->current_line_number, 1);
            // Column reset happens at the start of the next line processing
        }
        else
        {
            break; // End of file content
        }
    } // End while processing lines

    // Error handling for append failures or other errors during loop
    if (!success)
    {
        free_dynamic_buffer(&output_buffer); // Free partially built output
        free(file_content);
        pop_file_stack(pp_state);
        free(abs_path);
        pp_state->current_file_path = prev_file_path; // Restore before returning
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    free(file_content); // Original content buffer no longer needed

    // 4. Clean up stack and restore context for this file
    pop_file_stack(pp_state);
    free(abs_path); // Free the absolute path string we allocated
    pp_state->current_file_path = prev_file_path;
    pp_state->current_line_number = prev_line_number;

    // Return the final concatenated buffer (ownership transferred)
    return output_buffer.buffer;
}

// Processes a string directly, handling directives and macros.
// Returns a dynamically allocated string with processed content (caller frees), or NULL on error.
// NOTE: This function is very similar to process_file, but operates on an existing string.
// It does NOT handle #include directives originating from the string itself,
// as there's no base path to resolve relative includes from.
// Includes *within* files included by the string *might* work if process_file is called recursively,
// but the primary source string cannot include files itself.
wchar_t *process_string(BaaPreprocessor *pp_state, const wchar_t *source_string, wchar_t **error_message)
{
    *error_message = NULL;

    // Store previous context for restoration (though less critical for string input)
    const char *prev_file_path = pp_state->current_file_path;
    size_t prev_line_number = pp_state->current_line_number;

    // Set current context for this string (using the name provided in BaaPpSource)
    // The file_path field in the location stack will hold the source_name (e.g., "<string>")
    pp_state->current_file_path = get_current_original_location(pp_state).file_path;
    pp_state->current_line_number = 1;   // Start at line 1
    pp_state->current_column_number = 1; // Start at column 1

    // No file stack push/pop needed for string input.
    // No file reading needed.

    DynamicWcharBuffer output_buffer;
    // Estimate initial capacity
    if (!init_dynamic_buffer(&output_buffer, wcslen(source_string) + 1024))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory",
                       L"فشل في تخصيص الذاكرة لمخزن الإخراج المؤقت للسلسلة.");
        // Restore context before returning
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // 3. Process Lines and Directives (Logic largely copied from process_file)
    wchar_t *line_start = (wchar_t *)source_string; // Cast needed as source_string is const
    wchar_t *line_end;
    bool success = true;

    while (*line_start != L'\0' && success)
    {
        line_end = wcschr(line_start, L'\n');
        size_t line_len;
        if (line_end != NULL)
        {
            line_len = line_end - line_start;
            if (line_len > 0 && line_start[line_len - 1] == L'\r')
            {
                line_len--;
            }
        }
        else
        {
            line_len = wcslen(line_start);
        }

        wchar_t *current_line = wcsndup_internal(line_start, line_len);
        if (!current_line)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory",
                           L"فشل في تخصيص الذاكرة لسطر من السلسلة.");
            success = false;
            break;
        }

        pp_state->current_column_number = 1;

        // --- Skip Comments FIRST ---
        wchar_t *effective_line_start = current_line;
        while (iswspace(*effective_line_start))
        { // Skip leading whitespace
            effective_line_start++;
            pp_state->current_column_number++;
        }

        if (wcsncmp(effective_line_start, L"//", 2) == 0)
        {
            // Single-line comment, skip line processing
            free(current_line);
            if (line_end != NULL)
            {
                line_start = line_end + 1;
                pp_state->current_line_number++;
                // Update the location stack with the current line number for accurate error reporting
                update_current_location(pp_state, pp_state->current_line_number, 1);
            }
            else
            {
                break; // End of string
            }
            continue; // Next line
        }
        // TODO: Add multi-line comment handling for strings too

        // Check for directives (use effective_line_start)
        if (effective_line_start[0] == L'#')
        {
            // Call the new function to handle all directive logic.
            // Note: abs_path is NULL for string processing.
            // fwprintf(stderr, L"DEBUG PS: Calling handle_preprocessor_directive for line %zu: [%ls]\n", pp_state->current_line_number, effective_line_start);
            bool local_is_conditional_directive_ps; // For process_string
            if (!handle_preprocessor_directive(pp_state, effective_line_start + 1, NULL, &output_buffer, error_message, &local_is_conditional_directive_ps))
            {
                success = false;
            }
        }
        else if (!pp_state->skipping_lines)
        {
            // Not a directive and not skipping: process line for macro substitution.
            // fwprintf(stderr, L"DEBUG PS: Calling process_code_line_for_macros for line %zu: [%ls]\n", pp_state->current_line_number, current_line);
            if (!process_code_line_for_macros(pp_state, current_line, line_len, &output_buffer, error_message))
            {
                success = false;
            }
            else
            {
                // Append newline after successfully processing and appending the code line
                if (!append_to_dynamic_buffer(&output_buffer, L"\n"))
                {
                    success = false;
                    if (!*error_message) {
                        PpSourceLocation current_loc = get_current_original_location(pp_state);
                        PP_REPORT_FATAL(pp_state, &current_loc, PP_ERROR_BUFFER_OVERFLOW, "memory",
                                       L"فشل في إلحاق السطر بمخزن الإخراج المؤقت.");
                    }
                }
            }
        }
        // If skipping_lines is true and it's not a directive handled above, the line is effectively skipped.

        free(current_line);

        if (!success)
            break;

        if (line_end != NULL)
        {
            line_start = line_end + 1;
            pp_state->current_line_number++;
        }
        else
        {
            break;
        }
    }

    if (!success)
    {
        free_dynamic_buffer(&output_buffer);
        // Restore context before returning
        pp_state->current_file_path = prev_file_path;
        pp_state->current_line_number = prev_line_number;
        return NULL;
    }

    // Restore context
    pp_state->current_file_path = prev_file_path;
    pp_state->current_line_number = prev_line_number;

    return output_buffer.buffer;
}
