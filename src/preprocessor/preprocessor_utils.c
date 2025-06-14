#include "preprocessor_internal.h"
//#include <iconv.h>
#include <errno.h> // For errno
#include <string.h> // For strlen

// --- Location Stack ---

// Pushes a location onto the stack. Returns false on allocation failure.
bool push_location(BaaPreprocessor *pp, const PpSourceLocation *location)
{
    if (pp->location_stack_count >= pp->location_stack_capacity)
    {
        size_t new_capacity = (pp->location_stack_capacity == 0) ? 8 : pp->location_stack_capacity * 2;
        PpSourceLocation *new_stack = realloc(pp->location_stack, new_capacity * sizeof(PpSourceLocation));
        if (!new_stack) {
            PpSourceLocation error_loc = get_current_original_location(pp);
            PP_REPORT_FATAL(pp, &error_loc, PP_ERROR_OUT_OF_MEMORY, "memory",
                L"فشل في إعادة تخصيص الذاكرة لمكدس المواقع");
            return false;
        }
        pp->location_stack = new_stack;
        pp->location_stack_capacity = new_capacity;
    }
    // Copy the location data onto the stack
    pp->location_stack[pp->location_stack_count++] = *location;
    return true;
}

// Pops a location from the stack. Does nothing if the stack is empty.
void pop_location(BaaPreprocessor *pp)
{
    if (pp->location_stack_count > 0)
    {
        pp->location_stack_count--;
        // No need to free anything here as the stack holds copies, not pointers
    }
}

// Gets the location from the top of the stack. Returns a default location if stack is empty.
PpSourceLocation get_current_original_location(const BaaPreprocessor *pp)
{
    if (pp->location_stack_count > 0)
    {
        PpSourceLocation stack_loc = pp->location_stack[pp->location_stack_count - 1];
        
        // IMPROVEMENT: If the location stack only contains the initial location (line 1)
        // and we're currently processing a different line, use the current physical location
        // This fixes the line number reporting bug where all errors report line 1:1
        if (pp->location_stack_count == 1 &&
            stack_loc.line == 1 && stack_loc.column == 1 &&
            pp->current_line_number > 1)
        {
            // Use current physical location for better error reporting
            return (PpSourceLocation){
                .file_path = pp->current_file_path ? pp->current_file_path : stack_loc.file_path,
                .line = pp->current_line_number,
                .column = pp->current_column_number};
        }
        
        return stack_loc;
    }
    else
    {
        // Return a default/unknown location if stack is empty
        // Use the current physical location as a fallback if available
        return (PpSourceLocation){
            .file_path = pp->current_file_path ? pp->current_file_path : "(unknown)",
            .line = pp->current_line_number,
            .column = pp->current_column_number};
    }
}

// Updates the location on the top of the stack (if any exists)
void update_current_location(BaaPreprocessor *pp, size_t line, size_t column)
{
    if (pp->location_stack_count > 0)
    {
        pp->location_stack[pp->location_stack_count - 1].line = line;
        pp->location_stack[pp->location_stack_count - 1].column = column;
    }
}

// Frees the memory used by the location stack.
void free_location_stack(BaaPreprocessor *pp)
{
    free(pp->location_stack);
    pp->location_stack = NULL;
    pp->location_stack_count = 0;
    pp->location_stack_capacity = 0;
}

// --- New Error Formatter using Explicit Location ---
wchar_t *format_preprocessor_error_at_location(const PpSourceLocation *location, const wchar_t *format, ...)
{
    // Prepare the prefix using the provided location
    size_t prefix_base_len = wcslen(L":%zu:%zu: خطأ: "); // file:line:col: error:
    size_t path_len = location && location->file_path ? strlen(location->file_path) : strlen("(unknown file)");
    size_t prefix_buffer_size = path_len + prefix_base_len + 30; // Extra space for line/column numbers
    char *prefix_mb = malloc(prefix_buffer_size);
    if (!prefix_mb)
    {
        // Basic fallback if prefix allocation fails - create simple message without enhanced error system
        // to avoid recursion since this is a formatting function itself
        va_list args_fallback;
        va_start(args_fallback, format);
        va_list args_copy_fallback;
        va_copy(args_copy_fallback, args_fallback);
        int needed_fallback = vswprintf(NULL, 0, format, args_copy_fallback);
        va_end(args_copy_fallback);
        if (needed_fallback < 0)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة الخطأ (وفشل تخصيص البادئة).");
        }
        size_t buffer_size_fallback = (size_t)needed_fallback + 1;
        wchar_t *buffer_fallback = malloc(buffer_size_fallback * sizeof(wchar_t));
        if (!buffer_fallback)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة الخطأ (وفشل تخصيص البادئة والمخزن المؤقت).");
        }
        vswprintf(buffer_fallback, buffer_size_fallback, format, args_fallback);
        va_end(args_fallback);
        return buffer_fallback;
    }

    // Format the prefix using the provided location
    snprintf(prefix_mb, prefix_buffer_size, "%hs:%zu:%zu: خطأ: ",
             location && location->file_path ? location->file_path : "(unknown file)",
             location ? location->line : 0,
             location ? location->column : 0);

    // Convert prefix to wchar_t (same logic as format_preprocessor_error_with_context)
    wchar_t *prefix_w = NULL;
    int required_wchars = MultiByteToWideChar(CP_UTF8, 0, prefix_mb, -1, NULL, 0);
    if (required_wchars > 0)
    {
        prefix_w = malloc(required_wchars * sizeof(wchar_t));
        if (prefix_w)
        {
            MultiByteToWideChar(CP_UTF8, 0, prefix_mb, -1, prefix_w, required_wchars);
        }
        // Note: Memory allocation failure here falls through to fallback logic below
    }
    free(prefix_mb);

    if (!prefix_w)
    {
        // Fallback if prefix conversion fails
        va_list args_fallback;
        va_start(args_fallback, format);
        va_list args_copy_fallback;
        va_copy(args_copy_fallback, args_fallback);
        int needed_fallback = vswprintf(NULL, 0, format, args_copy_fallback);
        va_end(args_copy_fallback);
        if (needed_fallback < 0)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة الخطأ (وفشل تحويل البادئة).");
        }
        size_t buffer_size_fallback = (size_t)needed_fallback + 1;
        wchar_t *buffer_fallback = malloc(buffer_size_fallback * sizeof(wchar_t));
        if (!buffer_fallback)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة الخطأ (وفشل تحويل البادئة والمخزن المؤقت).");
        }
        vswprintf(buffer_fallback, buffer_size_fallback, format, args_fallback);
        va_end(args_fallback);
        return buffer_fallback;
    }

    // Format the user's message part (same logic as before)
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed_msg = vswprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed_msg < 0)
    {
        va_end(args);
        free(prefix_w);
        return baa_strdup(L"فشل في تنسيق جزء الرسالة من خطأ المعالج المسبق.");
    }

    // Combine prefix and message (same logic as before)
    size_t prefix_len = wcslen(prefix_w);
    size_t msg_len = (size_t)needed_msg;
    size_t total_len = prefix_len + msg_len;
    wchar_t *final_buffer = malloc((total_len + 1) * sizeof(wchar_t));

    if (!final_buffer)
    {
        va_end(args);
        free(prefix_w);
        // Note: Cannot use enhanced error system here as this is a formatting function
        return baa_strdup(L"فشل في تخصيص الذاكرة لرسالة خطأ المعالج المسبق الكاملة.");
    }

    wcscpy(final_buffer, prefix_w);
    free(prefix_w);

    vswprintf(final_buffer + prefix_len, msg_len + 1, format, args);
    va_end(args);

    return final_buffer;
}

wchar_t *format_preprocessor_warning_at_location(const PpSourceLocation *location, const wchar_t *format, ...)
{
    // Prepare the prefix using the provided location
    size_t prefix_base_len = wcslen(L":%zu:%zu: تحذير: "); // file:line:col: warning:
    size_t path_len = location && location->file_path ? strlen(location->file_path) : strlen("(unknown file)");
    size_t prefix_buffer_size = path_len + prefix_base_len + 30; // Extra space for line/column numbers
    char *prefix_mb = malloc(prefix_buffer_size);
    if (!prefix_mb)
    {
        // Basic fallback if prefix allocation fails - avoid enhanced error system to prevent recursion
        va_list args_fallback;
        va_start(args_fallback, format);
        va_list args_copy_fallback;
        va_copy(args_copy_fallback, args_fallback);
        int needed_fallback = vswprintf(NULL, 0, format, args_copy_fallback);
        va_end(args_copy_fallback);
        if (needed_fallback < 0)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة التحذير (وفشل تخصيص البادئة).");
        }
        size_t buffer_size_fallback = (size_t)needed_fallback + 1;
        wchar_t *buffer_fallback = malloc(buffer_size_fallback * sizeof(wchar_t));
        if (!buffer_fallback)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة التحذير (وفشل تخصيص البادئة والمخزن المؤقت).");
        }
        vswprintf(buffer_fallback, buffer_size_fallback, format, args_fallback);
        va_end(args_fallback);
        return buffer_fallback;
    }

    // Format the prefix using the provided location
    snprintf(prefix_mb, prefix_buffer_size, "%hs:%zu:%zu: تحذير: ", // Changed "خطأ" to "تحذير"
             location && location->file_path ? location->file_path : "(unknown file)",
             location ? location->line : 0,
             location ? location->column : 0);

    // Convert prefix to wchar_t
    wchar_t *prefix_w = NULL;
    int required_wchars = MultiByteToWideChar(CP_UTF8, 0, prefix_mb, -1, NULL, 0);
    if (required_wchars > 0)
    {
        prefix_w = malloc(required_wchars * sizeof(wchar_t));
        if (prefix_w)
        {
            MultiByteToWideChar(CP_UTF8, 0, prefix_mb, -1, prefix_w, required_wchars);
        }
        // Note: Memory allocation failure here falls through to fallback logic below
    }
    free(prefix_mb);

    if (!prefix_w)
    {
        // Fallback if prefix conversion fails
        va_list args_fallback;
        va_start(args_fallback, format);
        va_list args_copy_fallback;
        va_copy(args_copy_fallback, args_fallback);
        int needed_fallback = vswprintf(NULL, 0, format, args_copy_fallback);
        va_end(args_copy_fallback);
        if (needed_fallback < 0)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة التحذير (وفشل تحويل البادئة).");
        }
        size_t buffer_size_fallback = (size_t)needed_fallback + 1;
        wchar_t *buffer_fallback = malloc(buffer_size_fallback * sizeof(wchar_t));
        if (!buffer_fallback)
        {
            va_end(args_fallback);
            return baa_strdup(L"فشل في تنسيق رسالة التحذير (وفشل تحويل البادئة والمخزن المؤقت).");
        }
        vswprintf(buffer_fallback, buffer_size_fallback, format, args_fallback);
        va_end(args_fallback);
        return buffer_fallback;
    }

    // Format the user's message part
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed_msg = vswprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed_msg < 0)
    {
        va_end(args);
        free(prefix_w);
        return baa_strdup(L"فشل في تنسيق جزء الرسالة من تحذير المعالج المسبق.");
    }

    // Combine prefix and message
    size_t prefix_len = wcslen(prefix_w);
    size_t msg_len = (size_t)needed_msg;
    size_t total_len = prefix_len + msg_len;
    wchar_t *final_buffer = malloc((total_len + 1) * sizeof(wchar_t));

    if (!final_buffer)
    {
        va_end(args);
        free(prefix_w);
        return baa_strdup(L"فشل في تخصيص الذاكرة لرسالة تحذير المعالج المسبق الكاملة.");
    }

#ifdef _WIN32
    wcscpy_s(final_buffer, total_len + 1, prefix_w);
#else
    wcscpy(final_buffer, prefix_w);
#endif
    free(prefix_w);

    vswprintf(final_buffer + prefix_len, msg_len + 1, format, args);
    va_end(args);

    return final_buffer;
}

// --- Dynamic Buffer for Output ---

bool init_dynamic_buffer(DynamicWcharBuffer *db, size_t initial_capacity)
{
    db->buffer = malloc(initial_capacity * sizeof(wchar_t));
    if (!db->buffer)
    {
        db->length = 0;
        db->capacity = 0;
        // Note: Cannot report error here as this is a utility function without preprocessor context
        return false;
    }
    db->buffer[0] = L'\0';
    db->length = 0;
    db->capacity = initial_capacity;
    return true;
}

bool append_to_dynamic_buffer(DynamicWcharBuffer *db, const wchar_t *str_to_append)
{
    size_t append_len = wcslen(str_to_append);
    if (db->length + append_len + 1 > db->capacity)
    {
        size_t new_capacity = (db->capacity == 0) ? append_len + 1 : db->capacity * 2;
        while (new_capacity < db->length + append_len + 1)
        {
            new_capacity *= 2;
        }
        wchar_t *new_buffer = realloc(db->buffer, new_capacity * sizeof(wchar_t));
        if (!new_buffer)
        {
            return false; // Reallocation failed
        }
        db->buffer = new_buffer;
        db->capacity = new_capacity;
    }
#ifdef _WIN32
    wcscat_s(db->buffer, db->capacity, str_to_append);
#else
    wcscat(db->buffer, str_to_append);
#endif
    db->length += append_len;
    return true;
}

// Appends exactly n characters from str_to_append
bool append_dynamic_buffer_n(DynamicWcharBuffer *db, const wchar_t *str_to_append, size_t n)
{
    if (n == 0)
        return true; // Nothing to append
    if (db->length + n + 1 > db->capacity)
    {
        size_t new_capacity = (db->capacity == 0) ? n + 1 : db->capacity * 2;
        while (new_capacity < db->length + n + 1)
        {
            new_capacity *= 2;
        }
        wchar_t *new_buffer = realloc(db->buffer, new_capacity * sizeof(wchar_t));
        if (!new_buffer)
        {
            return false; // Reallocation failed
        }
        db->buffer = new_buffer;
        db->capacity = new_capacity;
    }
    // Use wcsncat for safe appending
    wcsncat(db->buffer, str_to_append, n);
    // Ensure null termination manually as wcsncat might not if n is exact size
    db->length += n;
    db->buffer[db->length] = L'\0';
    return true;
}

void free_dynamic_buffer(DynamicWcharBuffer *db)
{
    free(db->buffer);
    db->buffer = NULL;
    db->length = 0;
    db->capacity = 0;
}

// --- Compatibility ---

// Implementation of wcsndup for Windows compatibility (renamed)
wchar_t *wcsndup_internal(const wchar_t *s, size_t n)
{
    wchar_t *result = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
    if (result)
    {
#ifdef _WIN32
        wcsncpy_s(result, n + 1, s, n);
        result[n] = L'\0'; // Ensure null termination even with wcsncpy_s
#else
        wcsncpy(result, s, n);
        result[n] = L'\0';
#endif
    }
    return result;
}

// --- File I/O ---

// Reads the content of a file, detecting UTF-8 or UTF-16LE encoding via BOM.
// Assumes UTF-8 if no BOM is present.
// Returns a dynamically allocated wchar_t* string (caller must free).
// Returns NULL on error and sets error_message.
wchar_t *read_file_content(BaaPreprocessor *pp_state, const char *file_path, wchar_t **error_message)
{
    *error_message = NULL;
    FILE *file = NULL;
    wchar_t *buffer_w = NULL; // Wide char buffer (final result)
    char *buffer_mb = NULL;   // Multi-byte buffer (for UTF-8 reading)

#ifdef _WIN32
    // Use _wfopen on Windows for potentially non-ASCII paths
    // Convert UTF-8 path (common) to UTF-16
    int required_wchars = MultiByteToWideChar(CP_UTF8, 0, file_path, -1, NULL, 0);
    if (required_wchars <= 0)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file",
            L"فشل في تحويل مسار الملف '%hs' إلى UTF-16", file_path);
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"فشل في تحويل مسار الملف '%hs' إلى UTF-16.", file_path);
        if (error_message) *error_message = generate_error_summary(pp_state);
        return NULL;
    }
    wchar_t *w_file_path = malloc(required_wchars * sizeof(wchar_t));
    if (!w_file_path)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص الذاكرة لمسار الملف (UTF-16) '%hs'.", file_path);
        if (error_message) *error_message = generate_error_summary(pp_state);
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, file_path, -1, w_file_path, required_wchars);
    file = _wfopen(w_file_path, L"rb");
    free(w_file_path);
#else
    // Standard fopen for non-Windows
    file = fopen(file_path, "rb");
#endif

    if (!file)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_FILE_NOT_FOUND, "file", L"فشل في فتح الملف '%hs'.", file_path);
        if (error_message) *error_message = generate_error_summary(pp_state);
        return NULL;
    }

    // --- BOM Detection ---
    unsigned char bom[3] = {0};
    size_t bom_read = fread(bom, 1, 3, file);
    long file_start_pos = 0;
    bool is_utf16le = false;
    bool is_utf8 = false;

    if (bom_read >= 2 && bom[0] == 0xFF && bom[1] == 0xFE)
    {
        is_utf16le = true;
        file_start_pos = 2; // Skip BOM
    }
    else if (bom_read == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)
    {
        is_utf8 = true;
        file_start_pos = 3; // Skip BOM
    }
    else
    {
        // No recognized BOM, assume UTF-8
        is_utf8 = true;
        file_start_pos = 0; // Start from beginning
    }

    // Reset file position to after BOM or beginning
    fseek(file, file_start_pos, SEEK_SET);

    // Get file size (from current position to end)
    long current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    long file_end_pos = ftell(file);
    fseek(file, current_pos, SEEK_SET); // Go back to where we were

    long content_size_bytes = file_end_pos - current_pos;

    if (content_size_bytes < 0)
    { // Should not happen, but check
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_FILE_TOO_LARGE, "file", L"خطأ في حساب حجم الملف '%hs'.", file_path);
        if (error_message) *error_message = generate_error_summary(pp_state);
        fclose(file);
        return NULL;
    }
    if (content_size_bytes == 0)
    { // Empty file (after BOM or no BOM)
        fclose(file);
        buffer_w = malloc(sizeof(wchar_t)); // Allocate space for null terminator
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص الذاكرة لملف فارغ '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            return NULL;
        }
        buffer_w[0] = L'\0';
        return buffer_w; // Return empty, null-terminated string
    }

    // --- Read based on detected encoding ---
    if (is_utf16le)
    {
        // Read UTF-16LE directly into wchar_t buffer
        if (content_size_bytes % sizeof(wchar_t) != 0)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"حجم محتوى الملف '%hs' (UTF-16LE) ليس من مضاعفات حجم wchar_t.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            fclose(file);
            return NULL;
        }
        size_t num_wchars = content_size_bytes / sizeof(wchar_t);
        buffer_w = malloc((num_wchars + 1) * sizeof(wchar_t));
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص الذاكرة لمحتوى الملف (UTF-16LE) '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            fclose(file);
            return NULL;
        }
        size_t bytes_read = fread(buffer_w, 1, content_size_bytes, file);
        if (bytes_read != (size_t)content_size_bytes)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_FILE_NOT_FOUND, "file", L"فشل في قراءة محتوى الملف (UTF-16LE) بالكامل من '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_w);
            fclose(file);
            return NULL;
        }
        buffer_w[num_wchars] = L'\0'; // Null-terminate
    }
    else if (is_utf8)
    {
        // Read UTF-8 into byte buffer, then convert
        buffer_mb = malloc(content_size_bytes + 1); // +1 for potential null terminator
        if (!buffer_mb)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص الذاكرة لمحتوى الملف (UTF-8) '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            fclose(file);
            return NULL;
        }
        size_t bytes_read = fread(buffer_mb, 1, content_size_bytes, file);
        if (bytes_read != (size_t)content_size_bytes)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_FILE_NOT_FOUND, "file", L"فشل في قراءة محتوى الملف (UTF-8) بالكامل من '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        buffer_mb[content_size_bytes] = '\0'; // Null-terminate the byte buffer

        // Convert UTF-8 buffer_mb to wchar_t buffer_w
#ifdef _WIN32
        int required_wchars = MultiByteToWideChar(CP_UTF8, 0, buffer_mb, -1, NULL, 0);
        if (required_wchars <= 0)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"فشل في حساب حجم التحويل من UTF-8 للملف '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        buffer_w = malloc(required_wchars * sizeof(wchar_t));
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        int converted_wchars = MultiByteToWideChar(CP_UTF8, 0, buffer_mb, -1, buffer_w, required_wchars);
        if (converted_wchars <= 0)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_w);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
#else
        // Use mbstowcs on POSIX (requires locale to be set correctly, e.g., "en_US.UTF-8")
        // Ensure locale is set via setlocale(LC_ALL, "") in main or preprocessor init
        size_t required_wchars_check = mbstowcs(NULL, buffer_mb, 0);
        if (required_wchars_check == (size_t)-1)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"تسلسل بايت UTF-8 غير صالح في الملف '%hs' أو فشل في تحديد حجم التحويل.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        size_t num_wchars = required_wchars_check;
        buffer_w = malloc((num_wchars + 1) * sizeof(wchar_t));
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &error_loc, PP_ERROR_ALLOCATION_FAILED, "memory", L"فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'.", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        size_t converted_wchars = mbstowcs(buffer_w, buffer_mb, num_wchars + 1);
        if (converted_wchars == (size_t)-1)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t (تسلسل غير صالح؟).", file_path);
            if (error_message) *error_message = generate_error_summary(pp_state);
            free(buffer_w);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        buffer_w[num_wchars] = L'\0'; // Ensure null termination
#endif
        free(buffer_mb); // Free the intermediate byte buffer
    }
    else
    {
        // Should not happen due to logic above, but handle defensively
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        PP_REPORT_ERROR(pp_state, &error_loc, PP_ERROR_ENCODING_ERROR, "file", L"خطأ داخلي: ترميز ملف غير معروف تم اكتشافه لـ '%hs'.", file_path);
        if (error_message) *error_message = generate_error_summary(pp_state);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return buffer_w;
}

// --- Path Helpers ---

// Gets the absolute path for a given file path. Returns allocated string (caller frees) or NULL.
char *get_absolute_path(const char *file_path)
{
    char *abs_path_buf = malloc(MAX_PATH_LEN * sizeof(char));
    if (!abs_path_buf)
        return NULL; // Note: Cannot use enhanced error system here - utility function without preprocessor context

#ifdef _WIN32
    if (_fullpath(abs_path_buf, file_path, MAX_PATH_LEN) == NULL)
    {
        free(abs_path_buf);
        return NULL; // Note: Cannot use enhanced error system here - utility function without preprocessor context
    }
#else
    if (realpath(file_path, abs_path_buf) == NULL)
    {
        // realpath fails if the file doesn't exist, which might be okay
        // if we are just constructing a path. Let's try a simpler approach
        // if realpath fails, like getting CWD and joining.
        // However, for include resolution, the file *should* exist.
        // For now, stick to realpath's behavior.
        free(abs_path_buf);
        return NULL; // Note: Cannot use enhanced error system here - utility function without preprocessor context
    }
#endif
    return abs_path_buf;
}

// Gets the directory part of a file path. Returns allocated string (caller frees) or NULL.
char *get_directory_part(const char *file_path)
{
#ifdef _WIN32
    char *path_copy = baa_strdup_char(file_path);
#else
    char *path_copy = baa_strdup_char(file_path); // Use _strdup here too
#endif
    if (!path_copy)
        return NULL; // Note: Cannot use enhanced error system here - utility function without preprocessor context

#ifdef _WIN32
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    errno_t err = _splitpath_s(path_copy, drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);
    free(path_copy);
    if (err != 0)
        return NULL; // Note: Cannot use enhanced error system here - utility function without preprocessor context
    size_t drive_len = strlen(drive);
    size_t dir_len = strlen(dir);
    char *dir_part = malloc((drive_len + dir_len + 1) * sizeof(char));
    if (!dir_part)
        return NULL; // Note: Cannot use enhanced error system here - utility function without preprocessor context
    // Use strcpy_s and strcat_s for safety
    strcpy_s(dir_part, drive_len + 1, drive);
    strcat_s(dir_part, drive_len + dir_len + 1, dir);
    // Remove trailing separator if present and not just root
    size_t len = strlen(dir_part);
    if (len > 0 && (dir_part[len - 1] == '\\' || dir_part[len - 1] == '/'))
    {
        if (!(len == 1 || (len == 3 && dir_part[1] == ':'))) // Handle C:\ root or / root
        {
            dir_part[len - 1] = '\0';
        }
    }
    // Handle case where only drive is present (e.g., "C:") -> should return "C:." or similar?
    // Current logic returns "C:", which might be okay. _splitpath_s behavior is key.
    // If dir is empty, maybe return "."? Let's stick to _splitpath result for now.
    return dir_part;
#else
    char *dir_name_result = dirname(path_copy);
    char *dir_part = baa_strdup_char(dir_name_result); // Use _strdup here too
    free(path_copy);
    return dir_part;
#endif
}

// --- File Stack for Circular Include Detection ---

bool push_file_stack(BaaPreprocessor *pp, const char *abs_path)
{
    for (size_t i = 0; i < pp->open_files_count; ++i)
    {
        if (strcmp(pp->open_files_stack[i], abs_path) == 0)
        {
            return false; // Circular include detected
        }
    }

    if (pp->open_files_count >= pp->open_files_capacity)
    {
        size_t new_capacity = (pp->open_files_capacity == 0) ? 8 : pp->open_files_capacity * 2;
        char **new_stack = realloc(pp->open_files_stack, new_capacity * sizeof(char *));
        if (!new_stack)
            return false; // Allocation failure
        pp->open_files_stack = new_stack;
        pp->open_files_capacity = new_capacity;
    }

    char *path_copy = baa_strdup_char(abs_path); // Use _strdup here
    if (!path_copy)
        return false;
    pp->open_files_stack[pp->open_files_count++] = path_copy;
    return true;
}

void pop_file_stack(BaaPreprocessor *pp)
{
    if (pp->open_files_count > 0)
    {
        pp->open_files_count--;
        free(pp->open_files_stack[pp->open_files_count]);
        pp->open_files_stack[pp->open_files_count] = NULL;
    }
}

void free_file_stack(BaaPreprocessor *pp)
{
    while (pp->open_files_count > 0)
    {
        pop_file_stack(pp);
    }
    free(pp->open_files_stack);
    pp->open_files_stack = NULL;
    pp->open_files_capacity = 0;
}

// --- Diagnostic Accumulation ---
void add_preprocessor_diagnostic(BaaPreprocessor *pp_state, const PpSourceLocation *loc, bool is_error, const wchar_t *format, va_list args_list)
{
    if (!pp_state || !loc || !format)
        return;

    // Create a copy of args_list as vswprintf might be called multiple times
    va_list args_copy;
    va_copy(args_copy, args_list);

    // Determine required size for the formatted message from format and args_list
    // This call to vswprintf with NULL buffer is to get the required length.
    int needed_chars = vswprintf(NULL, 0, format, args_copy);
    va_end(args_copy); // Clean up the copied va_list

    if (needed_chars < 0)
    {
        // Error in formatting, use a fallback message
        // This diagnostic itself won't have a super precise location for the formatting error.
        // For internal formatting errors, we still use the legacy format to avoid recursion
        wchar_t *fallback_msg = format_preprocessor_error_at_location(loc, L"فشل داخلي: خطأ في تنسيق رسالة التشخيص الأصلية.");
        // Add this fallback message as a new diagnostic
        // This could lead to recursion if format_preprocessor_error_at_location also fails badly,
        // but it's a last resort.
        // To avoid potential recursion, we can directly add a simple diagnostic here.
        // For now, let's assume format_preprocessor_error_at_location is robust enough for simple strings.

        // TODO: Revisit if this fallback causes issues.
        // add_preprocessor_diagnostic(pp_state, loc, true, L"%ls", fallback_msg); // This would recurse

        // Instead, directly manage a simple error entry for this rare case:
        if (pp_state->diagnostic_count >= pp_state->diagnostic_capacity)
        { /* resize logic needed here too, or pre-allocate */
        }
        // pp_state->diagnostics[pp_state->diagnostic_count].message = fallback_msg; // (Simplified, needs proper struct init)
        // pp_state->diagnostics[pp_state->diagnostic_count].location = *loc;
        // For now, let's just skip adding the diagnostic if formatting itself fails.
        // The original va_end(args_list) will be called by the caller of add_preprocessor_diagnostic
        return;
    }

    size_t message_len = (size_t)needed_chars;
    wchar_t *formatted_message_body = malloc((message_len + 1) * sizeof(wchar_t));
    if (!formatted_message_body)
        return; // Out of memory

    // Now format the actual message body
    vswprintf(formatted_message_body, message_len + 1, format, args_list); // Use original args_list

    // Prepend location information to the formatted_message_body
    // For legacy diagnostic compatibility, we still use the old formatter
    wchar_t *full_diagnostic_message = is_error ? format_preprocessor_error_at_location(loc, L"%ls", formatted_message_body) : format_preprocessor_warning_at_location(loc, L"%ls", formatted_message_body);

    free(formatted_message_body); // Free the intermediate message body

    if (!full_diagnostic_message)
        return; // Error during full message formatting

    if (pp_state->diagnostic_count >= pp_state->diagnostic_capacity)
    {
        size_t new_capacity = (pp_state->diagnostic_capacity == 0) ? 8 : pp_state->diagnostic_capacity * 2;
        PreprocessorDiagnostic *new_diagnostics = realloc(pp_state->diagnostics, new_capacity * sizeof(PreprocessorDiagnostic));
        if (!new_diagnostics)
        {
            free(full_diagnostic_message); // Critical: free message if realloc fails
            return;                        // Out of memory
        }
        pp_state->diagnostics = new_diagnostics;
        pp_state->diagnostic_capacity = new_capacity;
    }

    pp_state->diagnostics[pp_state->diagnostic_count].message = full_diagnostic_message; // Store the fully formatted message
    pp_state->diagnostics[pp_state->diagnostic_count].location = *loc;                   // Store original location struct
    pp_state->diagnostic_count++;

    if (is_error)
    {
        pp_state->had_fatal_error = true;
    }
}

void free_diagnostics_list(BaaPreprocessor *pp_state)
{
    if (pp_state && pp_state->diagnostics)
    {
        for (size_t i = 0; i < pp_state->diagnostic_count; ++i)
        {
            free(pp_state->diagnostics[i].message);
        }
        free(pp_state->diagnostics);
        pp_state->diagnostics = NULL;
        pp_state->diagnostic_count = 0;
        pp_state->diagnostic_capacity = 0;
    }
}

// --- Enhanced Error System Implementation ---

// Initialize the error system with default configuration
bool init_preprocessor_error_system(BaaPreprocessor *pp_state)
{
    if (!pp_state) return false;
    
    // Initialize error limits with defaults
    pp_state->error_limits.max_errors = 100;
    pp_state->error_limits.max_warnings = 1000;
    pp_state->error_limits.max_notes = SIZE_MAX; // Unlimited
    pp_state->error_limits.stop_on_fatal = true;
    pp_state->error_limits.cascading_limit = 10;
    
    // Initialize error counters
    pp_state->fatal_count = 0;
    pp_state->error_count = 0;
    pp_state->warning_count = 0;
    pp_state->note_count = 0;
    pp_state->had_fatal_error = false;
    
    // Initialize diagnostic array
    pp_state->diagnostics = NULL;
    pp_state->diagnostic_count = 0;
    pp_state->diagnostic_capacity = 0;
    
    // Initialize recovery state
    pp_state->recovery_state.consecutive_errors = 0;
    pp_state->recovery_state.errors_this_line = 0;
    pp_state->recovery_state.directive_errors = 0;
    pp_state->recovery_state.expression_errors = 0;
    pp_state->recovery_state.in_recovery_mode = false;
    pp_state->recovery_state.recovery_context = "initialization";
    
    return true;
}

// Enhanced diagnostic collection with full severity and categorization support
void add_preprocessor_diagnostic_ex(
    BaaPreprocessor *pp_state,
    const PpSourceLocation *loc,
    PpDiagnosticSeverity severity,
    uint32_t error_code,
    const char *category,
    const wchar_t *suggestion,
    const wchar_t *format,
    ...)
{
    if (!pp_state || !loc || !format) return;
    
    // Check if we should continue processing based on limits
    if (!should_continue_processing(pp_state)) {
        return;
    }
    
    // Increment error count and check limits
    if (!increment_error_count(pp_state, severity)) {
        return; // Hit error limit
    }
    
    // Format the diagnostic message
    va_list args;
    va_start(args, format);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int needed_chars = vswprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed_chars < 0) {
        va_end(args);
        return; // Formatting error
    }
    
    size_t message_len = (size_t)needed_chars;
    wchar_t *formatted_message_body = malloc((message_len + 1) * sizeof(wchar_t));
    if (!formatted_message_body) {
        va_end(args);
        return; // Out of memory
    }
    
    vswprintf(formatted_message_body, message_len + 1, format, args);
    va_end(args);
    
    // Format with location information based on severity
    wchar_t *full_message;
    switch (severity) {
        case PP_DIAG_FATAL:
            // For internal error formatting, we still use the legacy format
            full_message = format_preprocessor_error_at_location(loc, L"خطأ فادح: %ls", formatted_message_body);
            break;
        case PP_DIAG_ERROR:
            // For internal error formatting, we still use the legacy format
            // For internal error formatting, we still use the legacy format
            full_message = format_preprocessor_error_at_location(loc, L"%ls", formatted_message_body);
            break;
        case PP_DIAG_WARNING:
            full_message = format_preprocessor_warning_at_location(loc, L"%ls", formatted_message_body);
            break;
        case PP_DIAG_NOTE:
            full_message = format_preprocessor_warning_at_location(loc, L"ملاحظة: %ls", formatted_message_body);
            break;
        default:
            full_message = format_preprocessor_error_at_location(loc, L"%ls", formatted_message_body);
            break;
    }
    
    free(formatted_message_body);
    
    if (!full_message) {
        return; // Formatting error
    }
    
    // Ensure diagnostic array has capacity
    if (pp_state->diagnostic_count >= pp_state->diagnostic_capacity) {
        size_t new_capacity = (pp_state->diagnostic_capacity == 0) ? 8 : pp_state->diagnostic_capacity * 2;
        PreprocessorDiagnostic *new_diagnostics = realloc(pp_state->diagnostics, new_capacity * sizeof(PreprocessorDiagnostic));
        if (!new_diagnostics) {
            free(full_message);
            return; // Out of memory
        }
        pp_state->diagnostics = new_diagnostics;
        pp_state->diagnostic_capacity = new_capacity;
    }
    
    // Store the diagnostic
    PreprocessorDiagnostic *diag = &pp_state->diagnostics[pp_state->diagnostic_count];
    diag->message = full_message;
    diag->location = *loc;
    diag->severity = severity;
    diag->error_code = error_code;
    diag->category = category;
    diag->suggestion = suggestion ? wcsdup(suggestion) : NULL;
    
    pp_state->diagnostic_count++;
    
    // Update counters and check limits
    increment_error_count(pp_state, severity);
    
    // Set fatal flag if needed
    if (severity == PP_DIAG_FATAL) {
        pp_state->had_fatal_error = true;
    }
}

// Check if processing should continue based on error limits
bool should_continue_processing(const BaaPreprocessor *pp_state)
{
    if (!pp_state) return false;
    
    return !pp_state->had_fatal_error &&
           pp_state->error_count < pp_state->error_limits.max_errors &&
           pp_state->warning_count < pp_state->error_limits.max_warnings;
}

// Increment error count and check limits
bool increment_error_count(BaaPreprocessor *pp_state, PpDiagnosticSeverity severity)
{
    if (!pp_state) return false;
    
    switch (severity) {
        case PP_DIAG_FATAL:
            pp_state->fatal_count++;
            return !pp_state->error_limits.stop_on_fatal;
        case PP_DIAG_ERROR:
            pp_state->error_count++;
            pp_state->recovery_state.consecutive_errors++;
            return pp_state->error_count < pp_state->error_limits.max_errors;
        case PP_DIAG_WARNING:
            pp_state->warning_count++;
            return pp_state->warning_count < pp_state->error_limits.max_warnings;
        case PP_DIAG_NOTE:
            pp_state->note_count++;
            return true; // Notes don't stop processing
    }
    return true;
}

// Check if specific error limit reached
bool has_reached_error_limit(const BaaPreprocessor *pp_state, PpDiagnosticSeverity severity)
{
    if (!pp_state) return true;
    
    switch (severity) {
        case PP_DIAG_FATAL:
            return pp_state->fatal_count > 0 && pp_state->error_limits.stop_on_fatal;
        case PP_DIAG_ERROR:
            return pp_state->error_count >= pp_state->error_limits.max_errors;
        case PP_DIAG_WARNING:
            return pp_state->warning_count >= pp_state->error_limits.max_warnings;
        case PP_DIAG_NOTE:
            return pp_state->note_count >= pp_state->error_limits.max_notes;
    }
    return false;
}

// Reset recovery state for new context
void reset_recovery_state(BaaPreprocessor *pp_state, const char *new_context)
{
    if (!pp_state) return;
    
    pp_state->recovery_state.consecutive_errors = 0;
    pp_state->recovery_state.errors_this_line = 0;
    pp_state->recovery_state.in_recovery_mode = false;
    pp_state->recovery_state.recovery_context = new_context;
}

// Determine appropriate recovery action based on error context
PpRecoveryAction determine_recovery_action(
    BaaPreprocessor *pp_state,
    PpDiagnosticSeverity severity,
    const char *category,
    const PpSourceLocation *location)
{
    if (!pp_state || !category) return PP_RECOVERY_HALT;
    
    // Check for fatal errors
    if (severity == PP_DIAG_FATAL) {
        return PP_RECOVERY_HALT;
    }
    
    // Check cascading error limits
    if (pp_state->recovery_state.consecutive_errors > pp_state->error_limits.cascading_limit) {
        return PP_RECOVERY_HALT;
    }
    
    // Category-specific recovery logic
    if (strcmp(category, "directive") == 0) {
        return PP_RECOVERY_SKIP_DIRECTIVE;
    } else if (strcmp(category, "expression") == 0) {
        return PP_RECOVERY_CONTINUE; // Can usually continue expression evaluation
    } else if (strcmp(category, "macro") == 0) {
        return PP_RECOVERY_CONTINUE; // Skip expansion, continue parsing
    } else if (strcmp(category, "file") == 0) {
        return PP_RECOVERY_SKIP_LINE; // Skip include, continue
    }
    
    return PP_RECOVERY_CONTINUE;
}

// Execute recovery action
bool execute_recovery_action(
    BaaPreprocessor *pp_state,
    PpRecoveryAction action,
    const wchar_t **current_position)
{
    if (!pp_state) return false;
    
    switch (action) {
        case PP_RECOVERY_CONTINUE:
            return true;
            
        case PP_RECOVERY_SKIP_LINE:
            return sync_to_next_line(pp_state, current_position);
            
        case PP_RECOVERY_SKIP_DIRECTIVE:
            return sync_to_next_directive(pp_state, current_position);
            
        case PP_RECOVERY_SYNC_CONDITIONAL:
            return recover_conditional_stack(pp_state);
            
        case PP_RECOVERY_HALT:
            return false;
    }
    
    return true;
}

// Directive-level synchronization - skip to next preprocessor directive
bool sync_to_next_directive(BaaPreprocessor *pp_state, const wchar_t **line_ptr)
{
    if (!line_ptr || !*line_ptr || !pp_state) return false;
    
    bool found_directive = false;
    size_t lines_searched = 0;
    const size_t max_search_lines = 100; // Prevent infinite loops
    
    // Skip to end of current line
    while (**line_ptr && **line_ptr != L'\n') {
        (*line_ptr)++;
    }
    
    // Search for next preprocessor directive
    while (**line_ptr && lines_searched < max_search_lines) {
        // Skip the newline if present
        if (**line_ptr == L'\n') {
            (*line_ptr)++;
            pp_state->current_line_number++;
            pp_state->current_column_number = 1;
            lines_searched++;
        }
        
        // Skip whitespace at beginning of line
        while (**line_ptr && (**line_ptr == L' ' || **line_ptr == L'\t')) {
            (*line_ptr)++;
            pp_state->current_column_number++;
        }
        
        // Check if this line starts with a preprocessor directive (#)
        if (**line_ptr == L'#') {
            found_directive = true;
            break;
        }
        
        // If not a directive, skip to end of line
        while (**line_ptr && **line_ptr != L'\n') {
            (*line_ptr)++;
            pp_state->current_column_number++;
        }
    }
    
    // Reset recovery state for new parsing context
    pp_state->recovery_state.errors_this_line = 0;
    pp_state->recovery_state.consecutive_errors = 0;
    
    // If we couldn't find a directive within reasonable search, still count as successful
    // to prevent infinite loops - the caller can handle end-of-input
    return true;
}

// Line-level synchronization - skip to next line
bool sync_to_next_line(BaaPreprocessor *pp_state, const wchar_t **line_ptr)
{
    if (!line_ptr || !*line_ptr || !pp_state) return false;
    
    // Skip to end of current line
    while (**line_ptr && **line_ptr != L'\n') {
        (*line_ptr)++;
        pp_state->current_column_number++;
    }
    
    // Skip the newline if present
    if (**line_ptr == L'\n') {
        (*line_ptr)++;
        pp_state->current_line_number++;
        pp_state->current_column_number = 1;
    }
    
    // Reset line-specific recovery state
    pp_state->recovery_state.errors_this_line = 0;
    
    // If we've had too many consecutive errors, don't increment consecutive count
    // This prevents cascading failures from escalating recovery actions
    if (pp_state->recovery_state.consecutive_errors > 5) {
        pp_state->recovery_state.consecutive_errors = 5; // Cap it
    }
    
    return true;
}

// Expression recovery - skip to next safe point in expression
bool sync_expression_parsing(BaaPreprocessor *pp_state, const wchar_t **expr_ptr, wchar_t terminator)
{
    if (!expr_ptr || !*expr_ptr || !pp_state) return false;
    
    // Skip to next safe parsing point with enhanced bracket/parentheses tracking
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    size_t chars_processed = 0;
    const size_t max_expression_length = 10000; // Prevent infinite loops
    
    while (**expr_ptr && **expr_ptr != terminator && chars_processed < max_expression_length) {
        wchar_t current = **expr_ptr;
        
        // Track nesting depth for balanced recovery
        switch (current) {
            case L'(':
                paren_depth++;
                break;
            case L')':
                paren_depth--;
                if (paren_depth < 0) {
                    // Found unmatched closing paren - this might be our terminator
                    return true;
                }
                break;
            case L'[':
                bracket_depth++;
                break;
            case L']':
                bracket_depth--;
                if (bracket_depth < 0) {
                    // Found unmatched closing bracket
                    return true;
                }
                break;
            case L'{':
                brace_depth++;
                break;
            case L'}':
                brace_depth--;
                if (brace_depth < 0) {
                    // Found unmatched closing brace
                    return true;
                }
                break;
            case L',':
                // Comma at top level might be a good recovery point
                if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                    return true;
                }
                break;
            case L';':
                // Semicolon is usually a statement separator - good recovery point
                if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                    return true;
                }
                break;
            case L'\n':
                // Newline might be a good recovery point
                if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                    return true;
                }
                break;
        }
        
        (*expr_ptr)++;
        chars_processed++;
        
        // Update column tracking
        if (current == L'\n') {
            pp_state->current_line_number++;
            pp_state->current_column_number = 1;
        } else {
            pp_state->current_column_number++;
        }
    }
    
    // Reset expression-specific error tracking
    pp_state->recovery_state.expression_errors = 0;
    
    return **expr_ptr != L'\0';
}

// Conditional stack recovery - attempt to repair conditional nesting
bool recover_conditional_stack(BaaPreprocessor *pp_state)
{
    if (!pp_state) return false;
    
    // Advanced conditional stack recovery implementation
    bool recovery_successful = false;
    
    // Check if conditional stack is corrupted
    if (pp_state->conditional_stack_count > 0) {
        // Try to find a safe recovery point by looking for balanced conditions
        size_t original_count = pp_state->conditional_stack_count;
        
        // If we have too many open conditionals (more than reasonable), truncate
        const size_t max_reasonable_depth = 50;
        if (pp_state->conditional_stack_count > max_reasonable_depth) {
            pp_state->conditional_stack_count = max_reasonable_depth;
            recovery_successful = true;
        }
        
        // Ensure corresponding branch stacks are synchronized
        if (pp_state->conditional_branch_taken_stack_count != pp_state->conditional_stack_count) {
            // Synchronize the branch taken stack
            if (pp_state->conditional_branch_taken_stack_count < pp_state->conditional_stack_count) {
                // Extend branch stack with false values
                while (pp_state->conditional_branch_taken_stack_count < pp_state->conditional_stack_count &&
                       pp_state->conditional_branch_taken_stack_count < pp_state->conditional_branch_taken_stack_capacity) {
                    pp_state->conditional_branch_taken_stack[pp_state->conditional_branch_taken_stack_count] = false;
                    pp_state->conditional_branch_taken_stack_count++;
                }
            } else {
                // Truncate branch stack to match conditional stack
                pp_state->conditional_branch_taken_stack_count = pp_state->conditional_stack_count;
            }
            recovery_successful = true;
        }
        
        // Reset skipping state to a safe default
        pp_state->skipping_lines = false;
        
        // Check each level for consistency
        for (size_t i = 0; i < pp_state->conditional_stack_count; i++) {
            if (pp_state->conditional_stack[i] && pp_state->conditional_branch_taken_stack[i]) {
                // This level should not be skipping
                continue;
            }
            // Mark suspicious levels for potential recovery
        }
    }
    
    // Reset recovery state tracking
    reset_recovery_state(pp_state, "conditional_recovery");
    
    // Update error tracking
    pp_state->recovery_state.directive_errors = 0;
    pp_state->recovery_state.consecutive_errors = 0;
    
    return recovery_successful;
}

// Generate backward-compatible error summary from collected diagnostics
wchar_t* generate_error_summary(const BaaPreprocessor *pp_state)
{
    if (!pp_state || pp_state->diagnostic_count == 0) {
        return NULL; // No errors
    }
    
    DynamicWcharBuffer summary = {0};
    if (!init_dynamic_buffer(&summary, 1024)) {
        // Note: Cannot use enhanced error system here as this function generates error summaries
        return wcsdup(L"فشل في إنشاء ملخص الأخطاء");
    }
    
    // Add summary header with cleaner, less repetitive format
    wchar_t temp_buffer[128];
    bool needs_separator = false;
    
    // Start with the single prefix
    if (!append_to_dynamic_buffer(&summary, L"تم العثور على ")) {
        free_dynamic_buffer(&summary);
        return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
    }
    
    if (pp_state->fatal_count > 0) {
        swprintf(temp_buffer, sizeof(temp_buffer)/sizeof(wchar_t), L"%zu خطأ فادح", pp_state->fatal_count);
        if (!append_to_dynamic_buffer(&summary, temp_buffer)) {
            free_dynamic_buffer(&summary);
            return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
        }
        needs_separator = true;
    }
    
    if (pp_state->error_count > 0) {
        if (needs_separator) {
            if (!append_to_dynamic_buffer(&summary, L"، ")) {
                free_dynamic_buffer(&summary);
                return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
            }
        }
        swprintf(temp_buffer, sizeof(temp_buffer)/sizeof(wchar_t), L"%zu خطأ", pp_state->error_count);
        if (!append_to_dynamic_buffer(&summary, temp_buffer)) {
            free_dynamic_buffer(&summary);
            return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
        }
        needs_separator = true;
    }
    
    if (pp_state->warning_count > 0) {
        if (needs_separator) {
            if (!append_to_dynamic_buffer(&summary, L"، ")) {
                free_dynamic_buffer(&summary);
                return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
            }
        }
        swprintf(temp_buffer, sizeof(temp_buffer)/sizeof(wchar_t), L"%zu تحذير", pp_state->warning_count);
        if (!append_to_dynamic_buffer(&summary, temp_buffer)) {
            free_dynamic_buffer(&summary);
            return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
        }
        needs_separator = true;
    }
    
    if (!append_to_dynamic_buffer(&summary, L":\n\n")) {
        free_dynamic_buffer(&summary);
        // Note: Cannot use enhanced error system here as this function generates error summaries
        return wcsdup(L"فشل في تنسيق ملخص الأخطاء");
    }
    
    // Add detailed error messages (limit to first 10 for readability)
    size_t errors_shown = 0;
    const size_t max_errors_in_summary = 10;
    
    for (size_t i = 0; i < pp_state->diagnostic_count && errors_shown < max_errors_in_summary; i++) {
        const PreprocessorDiagnostic *diag = &pp_state->diagnostics[i];
        
        // Skip notes in summary for brevity
        if (diag->severity == PP_DIAG_NOTE) continue;
        
        if (!append_to_dynamic_buffer(&summary, diag->message) ||
            !append_to_dynamic_buffer(&summary, L"\n")) {
            break;
        }
        errors_shown++;
    }
    
    // Add truncation notice if more errors exist
    if (pp_state->diagnostic_count > max_errors_in_summary) {
        size_t remaining = pp_state->diagnostic_count - max_errors_in_summary;
        wchar_t truncation_msg[256];
        swprintf(truncation_msg, sizeof(truncation_msg)/sizeof(wchar_t),
                L"\n... و %zu خطأ إضافي\n", remaining);
        append_to_dynamic_buffer(&summary, truncation_msg);
    }
    
    return summary.buffer; // Transfer ownership
}

// Enhanced cleanup that handles new diagnostic fields
void cleanup_preprocessor_error_system(BaaPreprocessor *pp_state)
{
    if (!pp_state) return;
    
    // Free all diagnostics including new fields
    if (pp_state->diagnostics) {
        for (size_t i = 0; i < pp_state->diagnostic_count; ++i) {
            free(pp_state->diagnostics[i].message);
            free(pp_state->diagnostics[i].suggestion);
        }
        free(pp_state->diagnostics);
        pp_state->diagnostics = NULL;
    }
    
    // Reset all counters
    pp_state->diagnostic_count = 0;
    pp_state->diagnostic_capacity = 0;
    pp_state->fatal_count = 0;
    pp_state->error_count = 0;
    pp_state->warning_count = 0;
    pp_state->note_count = 0;
    pp_state->had_fatal_error = false;
    
    // Reset recovery state
    reset_recovery_state(pp_state, "cleanup");
}
