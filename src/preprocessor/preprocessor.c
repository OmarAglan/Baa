#include "baa/preprocessor/preprocessor.h" // Public API
#include "preprocessor_internal.h"         // Internal definitions and function declarations
#include <time.h>                          // For __التاريخ__ and __الوقت__

// Helper function to report a simple diagnostic with a single string argument
// This is a trick to get a va_list for a single string argument
// We'll call add_preprocessor_diagnostic internally
static void report_simple_diag(BaaPreprocessor * s, const PpSourceLocation *l, bool is_err, const wchar_t *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    add_preprocessor_diagnostic(s, l, is_err, fmt, args);
    va_end(args);
}

// Helper function to report an unterminated conditional block
void report_unterminated_conditional(BaaPreprocessor * st, const PpSourceLocation *loc)
{
    report_simple_diag(st, loc, true, L"%ls", L"كتلة شرطية غير منتهية في نهاية المعالجة (مفقود #نهاية_إذا).");
}


// --- Public Preprocessor Function ---

wchar_t *baa_preprocess(const BaaPpSource *source, const char **include_paths, wchar_t **error_message)
{
    if (!source || !source->source_name || !error_message)
    {
        if (error_message)
        {
            PpSourceLocation early_error_loc = {"(preprocessor_init)", 0, 0};
            // Note: For early errors before preprocessor initialization, we still use legacy format
            // since the unified error system may not be available yet
            *error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطات غير صالحة تم تمريرها إلى المعالج المسبق (المصدر أو اسم المصدر أو مؤشر رسالة الخطأ هو NULL).");
        }
        return NULL;
    }
    if (source->type == BAA_PP_SOURCE_FILE && !source->data.file_path)
    {
        if (error_message)
        {
            PpSourceLocation early_error_loc = {source->source_name, 0, 0};
            // Note: For early errors before preprocessor initialization, we still use legacy format
            *error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطات غير صالحة: نوع المصدر هو ملف ولكن مسار الملف هو NULL.");
        }
        return NULL;
    }
    if (source->type == BAA_PP_SOURCE_STRING && !source->data.source_string)
    {
        if (error_message)
        {
            PpSourceLocation early_error_loc = {source->source_name, 0, 0};
            // Note: For early errors before preprocessor initialization, we still use legacy format
            *error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطات غير صالحة: نوع المصدر هو سلسلة ولكن مؤشر السلسلة هو NULL.");
        }
        return NULL;
    }

    // Initialize error message pointer passed by caller.
    // This will be populated at the end if any diagnostics were recorded.
    if (error_message)
    {
        *error_message = NULL; // Initialize to NULL
    }
    // --- Initialize Preprocessor State ---
    BaaPreprocessor pp_state = {0}; // Zero-initialize the structure

    // Set include paths
    pp_state.include_paths = include_paths;
    pp_state.include_path_count = 0;
    if (include_paths)
    {
        while (include_paths[pp_state.include_path_count] != NULL)
        {
            pp_state.include_path_count++;
        }
    }

    // Other fields are initialized to 0/NULL by the zero-initialization:
    // pp_state.open_files_stack = NULL;
    // pp_state.open_files_count = 0;
    // pp_state.open_files_capacity = 0;
    // pp_state.macros = NULL;
    // pp_state.macro_count = 0;
    // pp_state.macro_capacity = 0;
    // pp_state.conditional_stack = NULL;
    // pp_state.conditional_stack_count = 0;
    // pp_state.conditional_stack_capacity = 0;
    // pp_state.conditional_branch_taken_stack = NULL;
    // pp_state.conditional_branch_taken_stack_count = 0;
    // pp_state.conditional_branch_taken_stack_capacity = 0;
    // pp_state.skipping_lines = false;
    // pp_state.expanding_macros_stack = NULL;
    // pp_state.expanding_macros_count = 0;
    // pp_state.expanding_macros_capacity = 0;
    // pp_state.current_file_path = NULL; // Will be set within process_file
    // pp_state.current_line_number = 0;    // Will be set within process_file
    // pp_state.current_column_number = 0;  // Will be set within process_file
    // pp_state.location_stack = NULL;      // Zero-initialized
    // pp_state.location_stack_count = 0;   // Zero-initialized
    // pp_state.location_stack_capacity = 0;// Zero-initialized
    // pp_state.diagnostics = NULL;         // Zero-initialized
    // pp_state.diagnostic_count = 0;       // Zero-initialized
    // pp_state.diagnostic_capacity = 0;    // Zero-initialized
    // pp_state.had_error_this_pass = false; // Zero-initialized (now had_fatal_error)

    // Initialize enhanced error system
    if (!init_preprocessor_error_system(&pp_state)) {
        if (error_message) {
            PpSourceLocation early_error_loc = {source->source_name, 0, 0};
            // Note: Error system init failed, so we must use legacy format
            *error_message = format_preprocessor_error_at_location(&early_error_loc,
                L"فشل في تهيئة نظام الأخطاء المحسن للمعالج المسبق.");
        }
        return NULL;
    }

    // --- Push initial location (needed early for potential errors during macro init) ---
    PpSourceLocation initial_loc = {
        .file_path = source->source_name, // Use the provided source name
        .line = 0,                        // Line 0 for "compiler-defined" aspect before processing
        .column = 0};

    // --- Define __التاريخ__ and __الوقت__ macros ---
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    wchar_t date_str[100];
    wchar_t time_str[100];
    wchar_t quoted_date_str[104]; // For quotes and null terminator
    wchar_t quoted_time_str[104]; // For quotes and null terminator

    // Format date e.g., "May 06 2024" (English month names are common in __DATE__)
    // C standard format is "Mmm dd yyyy"
    // For Arabic, a direct equivalent might be complex due to month names.
    // Let's use a standard format that's easily parsable.
    // Using wcsftime for locale-independent month names is tricky.
    // Sticking to a fixed English month name format for __DATE__ as per C standard.
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    swprintf(date_str, sizeof(date_str) / sizeof(wchar_t), L"\"%hs %02d %04d\"", months[t->tm_mon], t->tm_mday, t->tm_year + 1900);

    // Format time "HH:MM:SS"
    swprintf(time_str, sizeof(time_str) / sizeof(wchar_t), L"\"%02d:%02d:%02d\"", t->tm_hour, t->tm_min, t->tm_sec);

    // Add macros to preprocessor state
    // Note: add_macro expects non-quoted body for string literals, it adds quotes if needed.
    // However, the C standard for __DATE__ and __TIME__ is that they expand *to* string literals.
    // So, the values themselves should be the quoted strings.
    // Add is_variadic = false for these object-like macros
    if (!add_macro(&pp_state, L"__التاريخ__", date_str, false, false, 0, NULL))
    {
        // Error handling if add_macro fails
        // For internal setup errors like this, we might still use the old direct error_message
        // or call add_preprocessor_diagnostic and then process it.
        // Let's assume add_macro will use add_preprocessor_diagnostic internally if it can.
        // For now, if add_macro returns false, we set the main error message.
        PP_REPORT_ERROR(&pp_state, &initial_loc, PP_ERROR_MACRO_EXPANSION_FAILED, "macro_definition",
            L"فشل في تعريف الماكرو المدمج __التاريخ__.");
        if (error_message)
            *error_message = generate_error_summary(&pp_state);

        // Cleanup any partially initialized state if necessary before returning
        return NULL;
    }
    if (!add_macro(&pp_state, L"__الوقت__", time_str, false, false, 0, NULL))
    {
        // Error handling if add_macro fails
        PP_REPORT_ERROR(&pp_state, &initial_loc, PP_ERROR_MACRO_EXPANSION_FAILED, "macro_definition",
            L"فشل في تعريف الماكرو المدمج __الوقت__.");
        if (error_message)
            *error_message = generate_error_summary(&pp_state);
        // Cleanup...
        return NULL;
    }
    // Define __الدالة__ as a placeholder string literal
    // The actual function name replacement would happen in later compiler stages.
    if (!add_macro(&pp_state, L"__الدالة__", L"\"__BAA_FUNCTION_PLACEHOLDER__\"", false, false, 0, NULL))
    {
        PP_REPORT_ERROR(&pp_state, &initial_loc, PP_ERROR_MACRO_EXPANSION_FAILED, "macro_definition",
            L"فشل في تعريف الماكرو المدمج __الدالة__.");
        if (error_message)
            *error_message = generate_error_summary(&pp_state);
        // Cleanup other predefined macros if necessary
        free_macros(&pp_state); // Will free __التاريخ__ and __الوقت__ if they were added
        return NULL;
    }

    // Define __إصدار_المعيار_باء__ as 10150L (for version 0.1.15, as per language.md example)
    // This expands to an integer constant.
    if (!add_macro(&pp_state, L"__إصدار_المعيار_باء__", L"10150L", false, false, 0, NULL))
    {
        PP_REPORT_ERROR(&pp_state, &initial_loc, PP_ERROR_MACRO_EXPANSION_FAILED, "macro_definition",
            L"فشل في تعريف الماكرو المدمج __إصدار_المعيار_باء__.");
        if (error_message)
            *error_message = generate_error_summary(&pp_state);
        // Cleanup other predefined macros
        free_macros(&pp_state); // Will free previous ones
        return NULL;
    }
    // --- End Define __التاريخ__ and __الوقت__ ---

    // --- End Initialize ---

    // --- Process based on source type ---
    wchar_t *final_output = NULL;
    if (source->type == BAA_PP_SOURCE_FILE)
    {
        // --- Push actual starting location for file processing ---
        PpSourceLocation file_start_loc = {
            .file_path = source->data.file_path, // Use the actual file path here
            .line = 1,
            .column = 1};
        if (!push_location(&pp_state, &file_start_loc))
        {
            // This is a critical setup error, probably still use direct error_message
            PP_REPORT_FATAL(&pp_state, &file_start_loc, PP_ERROR_OUT_OF_MEMORY, "system",
                L"فشل في دفع الموقع الأولي للملف (نفاد الذاكرة؟).");
            if (error_message)
            {
                *error_message = generate_error_summary(&pp_state);
            }
            // Cleanup initialized state before returning`
            free_macros(&pp_state);
            return NULL;
        }
        // --- End Push initial location ---

        // Start recursive processing by calling the core function for files
        final_output = process_file(&pp_state, source->data.file_path, error_message);
    }
    // The error_message passed to process_file will be handled by add_preprocessor_diagnostic internally.
    // So, the error_message parameter for baa_preprocess itself will be populated at the end.

    else if (source->type == BAA_PP_SOURCE_STRING)
    {
        // --- Push starting location for string processing ---
        PpSourceLocation string_start_loc = {
            .file_path = source->source_name, // Use the provided name (e.g., "<string>")
            .line = 1,
            .column = 1};
        if (!push_location(&pp_state, &string_start_loc))
        {
            PP_REPORT_FATAL(&pp_state, &string_start_loc, PP_ERROR_OUT_OF_MEMORY, "system",
                L"فشل في دفع الموقع الأولي للسلسلة (نفاد الذاكرة؟).");
            if (error_message)
            {
                *error_message = generate_error_summary(&pp_state);
            }
            free_macros(&pp_state);
            return NULL;
        }
        // --- End Push initial location ---

        // Directly process the string content using the new function
        // Pass a temporary local error_message_holder to process_string,
        // as errors from process_string will be added to pp_state.diagnostics
        wchar_t *temp_err_holder = NULL;
        final_output = process_string(&pp_state, source->data.source_string, &temp_err_holder);
        if (temp_err_holder)
            free(temp_err_holder); // Discard, we use pp_state.diagnostics

        // Pop the location stack after processing the string
        pop_location(&pp_state);
    }

    // --- Cleanup ---
    // Free all dynamically allocated resources held by the state struct
    free_file_stack(&pp_state);
    free_macros(&pp_state);
    free_conditional_stack(&pp_state);
    free_macro_expansion_stack(&pp_state);
    // Location stack is freed *after* potential final error reporting below
    // Note: pp_state.current_file_path is managed within process_file and its callers

    // Check for unterminated conditional block after processing is complete
    // This check needs to happen *after* cleanup of stacks but *before* returning potentially bad output
    if (pp_state.conditional_stack_count > 0 && !pp_state.had_fatal_error) // Only if no fatal error already reported
    {
        // If an error already occurred, keep that primary error message
        // Otherwise, report the unterminated block error.
        PpSourceLocation error_loc = get_current_original_location(&pp_state);
        // Use a va_list for add_preprocessor_diagnostic
        // This is a bit clunky for a single string, but keeps add_preprocessor_diagnostic consistent
        report_unterminated_conditional(&pp_state, &error_loc);
    }

    // If errors occurred, populate the output error_message parameter
    if ((pp_state.had_fatal_error || pp_state.error_count > 0) && error_message)
    {
        *error_message = generate_error_summary(&pp_state); // Use enhanced error summary
        if (final_output)
            free(final_output); // Free potentially partial output if errors

        final_output = NULL;
    }
    cleanup_preprocessor_error_system(&pp_state); // Clean up the enhanced error system
    free_location_stack(&pp_state);   // Now free the location stack

    return final_output; // Return processed source (NULL if errors and we chose to nullify)
}
