#include "baa/preprocessor/preprocessor.h" // Public API
#include "preprocessor_internal.h"         // Internal definitions and function declarations
#include <time.h>                          // For __التاريخ__ and __الوقت__

// --- Public Preprocessor Function ---

wchar_t *baa_preprocess(const BaaPpSource *source, const char **include_paths, wchar_t **error_message)
{
    if (!source || !source->source_name || !error_message)
    {
        if (error_message)
        {
            PpSourceLocation early_error_loc = {"(preprocessor_init)", 0, 0};
            *error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطات غير صالحة تم تمريرها إلى المعالج المسبق (المصدر أو اسم المصدر أو مؤشر رسالة الخطأ هو NULL).");
        }
        return NULL;
    }
    if (source->type == BAA_PP_SOURCE_FILE && !source->data.file_path)
    {
        if (error_message)
        {
            PpSourceLocation early_error_loc = {source->source_name, 0, 0};
            *error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطات غير صالحة: نوع المصدر هو ملف ولكن مسار الملف هو NULL.");
        }
        return NULL;
    }
    if (source->type == BAA_PP_SOURCE_STRING && !source->data.source_string)
    {
        if (error_message)
        {
            PpSourceLocation early_error_loc = {source->source_name, 0, 0};
            *error_message = format_preprocessor_error_at_location(&early_error_loc, L"وسيطات غير صالحة: نوع المصدر هو سلسلة ولكن مؤشر السلسلة هو NULL.");
        }
        return NULL;
    }

    *error_message = NULL; // Initialize error message to NULL

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
    pp_state.in_multiline_comment = false; // Initialize new state
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
        *error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تعريف الماكرو المدمج __التاريخ__.");
        // Cleanup any partially initialized state if necessary before returning
        return NULL;
    }
    if (!add_macro(&pp_state, L"__الوقت__", time_str, false, false, 0, NULL))
    {
        // Error handling if add_macro fails
        *error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تعريف الماكرو المدمج __الوقت__.");
        // Cleanup...
        return NULL;
    }
    // Define __الدالة__ as a placeholder string literal
    // The actual function name replacement would happen in later compiler stages.
    if (!add_macro(&pp_state, L"__الدالة__", L"\"__BAA_FUNCTION_PLACEHOLDER__\"", false, false, 0, NULL))
    {
        *error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تعريف الماكرو المدمج __الدالة__.");
        // Cleanup other predefined macros if necessary
        free_macros(&pp_state); // Will free __التاريخ__ and __الوقت__ if they were added
        return NULL;
    }

    // Define __إصدار_المعيار_باء__ as 10010L (for version 0.1.10, as per language.md example)
    // This expands to an integer constant.
    if (!add_macro(&pp_state, L"__إصدار_المعيار_باء__", L"10010L", false, false, 0, NULL))
    {
        *error_message = format_preprocessor_error_at_location(&initial_loc, L"فشل في تعريف الماكرو المدمج __إصدار_المعيار_باء__.");
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
            *error_message = format_preprocessor_error_at_location(&file_start_loc, L"فشل في دفع الموقع الأولي للملف (نفاد الذاكرة؟).");
            // Cleanup initialized state before returning
            free_macros(&pp_state);
            return NULL;
        }
        // --- End Push initial location ---

        // Start recursive processing by calling the core function for files
        final_output = process_file(&pp_state, source->data.file_path, error_message);
    }
    else if (source->type == BAA_PP_SOURCE_STRING)
    {
        // --- Push starting location for string processing ---
        PpSourceLocation string_start_loc = {
            .file_path = source->source_name, // Use the provided name (e.g., "<string>")
            .line = 1,
            .column = 1};
        if (!push_location(&pp_state, &string_start_loc))
        {
            *error_message = format_preprocessor_error_at_location(&string_start_loc, L"فشل في دفع الموقع الأولي للسلسلة (نفاد الذاكرة؟).");
            free_macros(&pp_state);
            return NULL;
        }
        // --- End Push initial location ---

        // Directly process the string content using the new function
        final_output = process_string(&pp_state, source->data.source_string, error_message);
        // Pop the location stack after processing the string
        pop_location(&pp_state);
    }

    // --- Cleanup ---
    // Free all dynamically allocated resources held by the state struct
    free_file_stack(&pp_state);
    free_macros(&pp_state);
    free_conditional_stack(&pp_state);
    free_macro_expansion_stack(&pp_state);
    free_location_stack(&pp_state); // Free the location stack
    // Note: pp_state.current_file_path is managed within process_file and its callers

    // Check for unterminated conditional block after processing is complete
    // This check needs to happen *after* cleanup of stacks but *before* returning potentially bad output
    if (pp_state.conditional_stack_count > 0 && final_output && !*error_message)
    {
        // If an error already occurred, keep that primary error message
        // Otherwise, report the unterminated block error.
        free(*error_message); // Free previous non-error message if any
        PpSourceLocation error_loc = get_current_original_location(&pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"كتلة شرطية غير منتهية في نهاية المعالجة (مفقود #نهاية_إذا).");
        free(final_output); // Free the potentially partially generated output
        final_output = NULL;
    }

    // Return the fully processed string (or NULL if an error occurred)
    return final_output;
}
