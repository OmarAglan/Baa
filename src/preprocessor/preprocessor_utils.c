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
        if (!new_stack)
            return false; // Allocation failure
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
        return pp->location_stack[pp->location_stack_count - 1];
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

// Frees the memory used by the location stack.
void free_location_stack(BaaPreprocessor *pp)
{
    free(pp->location_stack);
    pp->location_stack = NULL;
    pp->location_stack_count = 0;
    pp->location_stack_capacity = 0;
}

// --- Legacy formatting functions (restored for compatibility) ---

// Legacy error formatter - formats error message with location information
wchar_t *format_preprocessor_error_at_location(const PpSourceLocation *location, const wchar_t *format, ...)
{
    if (!format) {
        return NULL;
    }
    
    // Handle NULL location gracefully
    const char *file = location ? location->file_path : "(unknown)";
    size_t line = location ? location->line : 0;
    size_t column = location ? location->column : 0;
    
    // Use a large buffer for the variadic formatting
    const size_t temp_buffer_size = 2048;
    wchar_t temp_buffer[2048];
    
    va_list args;
    va_start(args, format);
    int result = vswprintf(temp_buffer, temp_buffer_size, format, args);
    va_end(args);
    
    if (result < 0) {
        // Formatting failed, return a basic error message
        const wchar_t *fallback = L"[ERROR] Formatting failed";
        wchar_t *error_msg = malloc((wcslen(fallback) + 1) * sizeof(wchar_t));
        if (error_msg) {
            wcscpy(error_msg, fallback);
        }
        return error_msg;
    }
    
    // Calculate the size needed for the final formatted message
    const size_t location_prefix_max = 512; // Space for "[ERROR] file:line:column: "
    size_t total_size = location_prefix_max + wcslen(temp_buffer) + 1;
    
    wchar_t *final_message = malloc(total_size * sizeof(wchar_t));
    if (!final_message) {
        return NULL;
    }
    
    // Format the complete message with location information
    if (line > 0 && column > 0) {
        swprintf(final_message, total_size, L"[ERROR] %hs:%zu:%zu: %ls",
                 file, line, column, temp_buffer);
    } else if (line > 0) {
        swprintf(final_message, total_size, L"[ERROR] %hs:%zu: %ls",
                 file, line, temp_buffer);
    } else {
        swprintf(final_message, total_size, L"[ERROR] %hs: %ls",
                 file, temp_buffer);
    }
    
    return final_message;
}

// Legacy warning formatter - formats warning message with location information
wchar_t *format_preprocessor_warning_at_location(const PpSourceLocation *location, const wchar_t *format, ...)
{
    if (!format) {
        return NULL;
    }
    
    // Handle NULL location gracefully
    const char *file = location ? location->file_path : "(unknown)";
    size_t line = location ? location->line : 0;
    size_t column = location ? location->column : 0;
    
    // Use a large buffer for the variadic formatting
    const size_t temp_buffer_size = 2048;
    wchar_t temp_buffer[2048];
    
    va_list args;
    va_start(args, format);
    int result = vswprintf(temp_buffer, temp_buffer_size, format, args);
    va_end(args);
    
    if (result < 0) {
        // Formatting failed, return a basic warning message
        const wchar_t *fallback = L"[WARNING] Formatting failed";
        wchar_t *warning_msg = malloc((wcslen(fallback) + 1) * sizeof(wchar_t));
        if (warning_msg) {
            wcscpy(warning_msg, fallback);
        }
        return warning_msg;
    }
    
    // Calculate the size needed for the final formatted message
    const size_t location_prefix_max = 512; // Space for "[WARNING] file:line:column: "
    size_t total_size = location_prefix_max + wcslen(temp_buffer) + 1;
    
    wchar_t *final_message = malloc(total_size * sizeof(wchar_t));
    if (!final_message) {
        return NULL;
    }
    
    // Format the complete message with location information
    if (line > 0 && column > 0) {
        swprintf(final_message, total_size, L"[WARNING] %hs:%zu:%zu: %ls",
                 file, line, column, temp_buffer);
    } else if (line > 0) {
        swprintf(final_message, total_size, L"[WARNING] %hs:%zu: %ls",
                 file, line, temp_buffer);
    } else {
        swprintf(final_message, total_size, L"[WARNING] %hs: %ls",
                 file, temp_buffer);
    }
    
    return final_message;
}

// --- Dynamic Buffer for Output ---

bool init_dynamic_buffer(DynamicWcharBuffer *db, size_t initial_capacity)
{
    db->buffer = malloc(initial_capacity * sizeof(wchar_t));
    if (!db->buffer)
    {
        db->length = 0;
        db->capacity = 0;
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
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحويل مسار الملف '%hs' إلى UTF-16.", file_path);
        return NULL;
    }
    wchar_t *w_file_path = malloc(required_wchars * sizeof(wchar_t));
    if (!w_file_path)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لمسار الملف (UTF-16) '%hs'.", file_path);
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
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في فتح الملف '%hs'.", file_path);
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
        *error_message = format_preprocessor_error_at_location(&error_loc, L"خطأ في حساب حجم الملف '%hs'.", file_path);
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
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لملف فارغ '%hs'.", file_path);
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
            *error_message = format_preprocessor_error_at_location(&error_loc, L"حجم محتوى الملف '%hs' (UTF-16LE) ليس من مضاعفات حجم wchar_t.", file_path);
            fclose(file);
            return NULL;
        }
        size_t num_wchars = content_size_bytes / sizeof(wchar_t);
        buffer_w = malloc((num_wchars + 1) * sizeof(wchar_t));
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لمحتوى الملف (UTF-16LE) '%hs'.", file_path);
            fclose(file);
            return NULL;
        }
        size_t bytes_read = fread(buffer_w, 1, content_size_bytes, file);
        if (bytes_read != (size_t)content_size_bytes)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في قراءة محتوى الملف (UTF-16LE) بالكامل من '%hs'.", file_path);
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
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة لمحتوى الملف (UTF-8) '%hs'.", file_path);
            fclose(file);
            return NULL;
        }
        size_t bytes_read = fread(buffer_mb, 1, content_size_bytes, file);
        if (bytes_read != (size_t)content_size_bytes)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في قراءة محتوى الملف (UTF-8) بالكامل من '%hs'.", file_path);
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
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في حساب حجم التحويل من UTF-8 للملف '%hs'.", file_path);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        buffer_w = malloc(required_wchars * sizeof(wchar_t));
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'.", file_path);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        int converted_wchars = MultiByteToWideChar(CP_UTF8, 0, buffer_mb, -1, buffer_w, required_wchars);
        if (converted_wchars <= 0)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t.", file_path);
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
            *error_message = format_preprocessor_error_at_location(&error_loc, L"تسلسل بايت UTF-8 غير صالح في الملف '%hs' أو فشل في تحديد حجم التحويل.", file_path);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        size_t num_wchars = required_wchars_check;
        buffer_w = malloc((num_wchars + 1) * sizeof(wchar_t));
        if (!buffer_w)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة للمخزن المؤقت wchar_t للملف '%hs'.", file_path);
            free(buffer_mb);
            fclose(file);
            return NULL;
        }
        size_t converted_wchars = mbstowcs(buffer_w, buffer_mb, num_wchars + 1);
        if (converted_wchars == (size_t)-1)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تحويل محتوى الملف '%hs' من UTF-8 إلى wchar_t (تسلسل غير صالح؟).", file_path);
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
        *error_message = format_preprocessor_error_at_location(&error_loc, L"خطأ داخلي: ترميز ملف غير معروف تم اكتشافه لـ '%hs'.", file_path);
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
        return NULL;

#ifdef _WIN32
    if (_fullpath(abs_path_buf, file_path, MAX_PATH_LEN) == NULL)
    {
        free(abs_path_buf);
        return NULL;
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
        return NULL;
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
        return NULL;

#ifdef _WIN32
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    errno_t err = _splitpath_s(path_copy, drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);
    free(path_copy);
    if (err != 0)
        return NULL;
    size_t drive_len = strlen(drive);
    size_t dir_len = strlen(dir);
    char *dir_part = malloc((drive_len + dir_len + 1) * sizeof(char));
    if (!dir_part)
        return NULL;
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
        // pp_state->diagnostic_count++;
        // pp_state->had_error_this_pass = true;
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
        pp_state->had_error_this_pass = true;
    }
}

// Enhanced diagnostic function with severity levels and recovery suggestions
void add_preprocessor_diagnostic_with_severity(BaaPreprocessor *pp_state, const PpSourceLocation *loc,
                                               PpDiagnosticSeverity severity, const wchar_t *recovery_suggestion,
                                               const wchar_t *format, va_list args_list)
{
    if (!pp_state || !loc || !format)
        return;

    // Create a copy of args_list as vswprintf might be called multiple times
    va_list args_copy;
    va_copy(args_copy, args_list);

    // Determine required size for the formatted message from format and args_list
    int needed_chars = vswprintf(NULL, 0, format, args_copy);
    va_end(args_copy); // Clean up the copied va_list

    if (needed_chars < 0)
    {
        // Error in formatting, skip adding the diagnostic
        return;
    }

    size_t message_len = (size_t)needed_chars;
    wchar_t *formatted_message_body = malloc((message_len + 1) * sizeof(wchar_t));
    if (!formatted_message_body)
        return; // Out of memory

    // Format the actual message body
    vswprintf(formatted_message_body, message_len + 1, format, args_list);

    // Create severity-specific formatted message
    wchar_t *full_diagnostic_message = NULL;
    const wchar_t *severity_prefix = NULL;
    
    switch (severity) {
        case PP_DIAGNOSTIC_INFO:
            severity_prefix = L"معلومات";
            break;
        case PP_DIAGNOSTIC_WARNING:
            severity_prefix = L"تحذير";
            break;
        case PP_DIAGNOSTIC_ERROR:
            severity_prefix = L"خطأ";
            break;
        case PP_DIAGNOSTIC_FATAL:
            severity_prefix = L"خطأ فادح";
            break;
    }

    // Format with severity prefix
    size_t prefix_base_len = wcslen(L":%zu:%zu: : ") + wcslen(severity_prefix);
    size_t path_len = loc && loc->file_path ? strlen(loc->file_path) : strlen("(unknown file)");
    size_t prefix_buffer_size = path_len + prefix_base_len + 30;
    char *prefix_mb = malloc(prefix_buffer_size);
    if (!prefix_mb)
    {
        free(formatted_message_body);
        return;
    }

    snprintf(prefix_mb, prefix_buffer_size, "%hs:%zu:%zu: %ls: ",
             loc && loc->file_path ? loc->file_path : "(unknown file)",
             loc ? loc->line : 0,
             loc ? loc->column : 0,
             severity_prefix);

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
    }
    free(prefix_mb);

    if (!prefix_w)
    {
        free(formatted_message_body);
        return;
    }

    // Combine prefix and message
    size_t prefix_len = wcslen(prefix_w);
    size_t msg_len = wcslen(formatted_message_body);
    size_t total_len = prefix_len + msg_len;
    full_diagnostic_message = malloc((total_len + 1) * sizeof(wchar_t));

    if (!full_diagnostic_message)
    {
        free(formatted_message_body);
        free(prefix_w);
        return;
    }

    wcscpy(full_diagnostic_message, prefix_w);
    wcscat(full_diagnostic_message, formatted_message_body);
    free(prefix_w);
    free(formatted_message_body);

    // Copy recovery suggestion if provided
    wchar_t *suggestion_copy = NULL;
    if (recovery_suggestion)
    {
        suggestion_copy = baa_strdup(recovery_suggestion);
    }

    // Expand diagnostics array if needed
    if (pp_state->diagnostic_count >= pp_state->diagnostic_capacity)
    {
        size_t new_capacity = (pp_state->diagnostic_capacity == 0) ? 8 : pp_state->diagnostic_capacity * 2;
        PreprocessorDiagnostic *new_diagnostics = realloc(pp_state->diagnostics, new_capacity * sizeof(PreprocessorDiagnostic));
        if (!new_diagnostics)
        {
            free(full_diagnostic_message);
            free(suggestion_copy);
            return; // Out of memory
        }
        pp_state->diagnostics = new_diagnostics;
        pp_state->diagnostic_capacity = new_capacity;
    }

    // Store the diagnostic
    pp_state->diagnostics[pp_state->diagnostic_count].message = full_diagnostic_message;
    pp_state->diagnostics[pp_state->diagnostic_count].location = *loc;
    pp_state->diagnostics[pp_state->diagnostic_count].severity = severity;
    pp_state->diagnostics[pp_state->diagnostic_count].recovery_suggestion = suggestion_copy;
    pp_state->diagnostic_count++;

    // Update counters and flags
    switch (severity) {
        case PP_DIAGNOSTIC_INFO:
            break;
        case PP_DIAGNOSTIC_WARNING:
            pp_state->warning_count++;
            break;
        case PP_DIAGNOSTIC_ERROR:
            pp_state->error_count++;
            pp_state->had_error_this_pass = true;
            break;
        case PP_DIAGNOSTIC_FATAL:
            pp_state->fatal_error_count++;
            pp_state->had_error_this_pass = true;
            break;
    }
}

// Convenience wrapper functions
void add_fatal_diagnostic(BaaPreprocessor *pp_state, const PpSourceLocation *loc, const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    add_preprocessor_diagnostic_with_severity(pp_state, loc, PP_DIAGNOSTIC_FATAL, NULL, format, args);
    va_end(args);
}

void add_error_with_suggestion(BaaPreprocessor *pp_state, const PpSourceLocation *loc, const wchar_t *recovery_suggestion, const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    add_preprocessor_diagnostic_with_severity(pp_state, loc, PP_DIAGNOSTIC_ERROR, recovery_suggestion, format, args);
    va_end(args);
}

void add_warning_with_suggestion(BaaPreprocessor *pp_state, const PpSourceLocation *loc, const wchar_t *recovery_suggestion, const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    add_preprocessor_diagnostic_with_severity(pp_state, loc, PP_DIAGNOSTIC_WARNING, recovery_suggestion, format, args);
    va_end(args);
}

void add_info_diagnostic(BaaPreprocessor *pp_state, const PpSourceLocation *loc, const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    add_preprocessor_diagnostic_with_severity(pp_state, loc, PP_DIAGNOSTIC_INFO, NULL, format, args);
    va_end(args);
}

void free_diagnostics_list(BaaPreprocessor *pp_state)
{
    if (pp_state && pp_state->diagnostics)
    {
        for (size_t i = 0; i < pp_state->diagnostic_count; ++i)
        {
            free(pp_state->diagnostics[i].message);
            free(pp_state->diagnostics[i].recovery_suggestion); // Free recovery suggestions
        }
        free(pp_state->diagnostics);
        pp_state->diagnostics = NULL;
        pp_state->diagnostic_count = 0;
        pp_state->diagnostic_capacity = 0;
        
        // Reset enhanced counters
        pp_state->fatal_error_count = 0;
        pp_state->error_count = 0;
        pp_state->warning_count = 0;
    }
}

// --- Error Recovery and Synchronization Utilities ---

// Skips to the next line, used for error recovery
void skip_to_next_line(BaaPreprocessor *pp_state, const wchar_t **current_pos)
{
    if (!pp_state || !current_pos || !*current_pos)
        return;
    
    // Skip to end of current line
    while (**current_pos != L'\0' && **current_pos != L'\n')
    {
        (*current_pos)++;
    }
    
    // Skip the newline character if present
    if (**current_pos == L'\n')
    {
        (*current_pos)++;
        pp_state->current_line_number++;
        pp_state->current_column_number = 1;
    }
}

// Finds the next preprocessor directive for synchronization
const wchar_t *find_next_directive(const wchar_t *content)
{
    if (!content)
        return NULL;
    
    const wchar_t *pos = content;
    
    while (*pos != L'\0')
    {
        // Skip to beginning of line
        while (*pos != L'\0' && *pos != L'\n')
            pos++;
        
        if (*pos == L'\n')
            pos++; // Skip newline
        
        // Skip leading whitespace on new line
        while (*pos == L' ' || *pos == L'\t')
            pos++;
        
        // Check if this line starts with '#'
        if (*pos == L'#')
        {
            return pos;
        }
    }
    
    return NULL; // No more directives found
}

// Checks if a directive is a conditional directive for recovery
bool is_conditional_directive_keyword(const wchar_t *directive_line)
{
    if (!directive_line || *directive_line != L'#')
        return false;
    
    const wchar_t *keyword = directive_line + 1; // Skip '#'
    
    // Skip whitespace after '#'
    while (*keyword == L' ' || *keyword == L'\t')
        keyword++;
    
    // Check for conditional directive keywords
    const wchar_t *conditionals[] = {
        L"إذا",        // #if
        L"إذا_عرف",    // #ifdef
        L"إذا_لم_يعرف", // #ifndef
        L"وإلا_إذا",    // #elif
        L"إلا",        // #else
        L"نهاية_إذا",   // #endif
        NULL
    };
    
    for (int i = 0; conditionals[i]; i++)
    {
        size_t len = wcslen(conditionals[i]);
        if (wcsncmp(keyword, conditionals[i], len) == 0)
        {
            // Check that it's followed by whitespace or end of string
            wchar_t next_char = keyword[len];
            if (next_char == L'\0' || iswspace(next_char))
                return true;
        }
    }
    
    return false;
}

// Attempts to recover from a malformed directive by skipping to a safe point
bool attempt_directive_recovery(BaaPreprocessor *pp_state, const wchar_t **current_pos)
{
    if (!pp_state || !current_pos || !*current_pos)
        return false;
    
    PpSourceLocation error_loc = get_current_original_location(pp_state);
    
    // Report recovery attempt - use direct formatting instead of va_list
    wchar_t *recovery_msg = format_preprocessor_warning_at_location(&error_loc, 
        L"محاولة استرداد من خطأ التوجيه - البحث عن نقطة آمنة للمتابعة");
    if (recovery_msg) {
        fwprintf(stderr, L"%ls\n", recovery_msg);
        free(recovery_msg);
    }
    
    // Find next directive
    const wchar_t *next_directive = find_next_directive(*current_pos);
    
    if (next_directive)
    {
        // Count skipped lines for accurate line tracking
        const wchar_t *line_counter = *current_pos;
        while (line_counter < next_directive)
        {
            if (*line_counter == L'\n')
            {
                pp_state->current_line_number++;
            }
            line_counter++;
        }
        
        *current_pos = next_directive;
        pp_state->current_column_number = 1;
        
        // Report successful recovery
        PpSourceLocation recovery_loc = get_current_original_location(pp_state);
        wchar_t *success_msg = format_preprocessor_warning_at_location(&recovery_loc,
            L"تم الاسترداد بنجاح - استئناف المعالجة من السطر %zu", pp_state->current_line_number);
        if (success_msg) {
            fwprintf(stderr, L"%ls\n", success_msg);
            free(success_msg);
        }
        
        return true;
    }
    
    // If no next directive found, skip to end
    skip_to_next_line(pp_state, current_pos);
    return false;
}

// Validates and attempts to recover conditional directive stack
bool validate_and_recover_conditional_stack(BaaPreprocessor *pp_state)
{
    if (!pp_state)
        return false;
    
    // Check for unmatched conditionals
    if (pp_state->conditional_stack_count > 0)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        
        // Report each unmatched conditional using enhanced diagnostics
        add_error_with_suggestion(pp_state, &error_loc,
            L"أضف #نهاية_إذا لكل توجيه شرطي غير مكتمل",
            L"توجيه شرطي غير مطابق - مفقود #نهاية_إذا (العدد: %zu)", pp_state->conditional_stack_count);
        // Note: Enhanced diagnostics system handles error reporting
        
        // Clear the stack to prevent further issues
        pp_state->conditional_stack_count = 0;
        pp_state->conditional_branch_taken_stack_count = 0;
        pp_state->skipping_lines = false;
        
        return true; // Recovery attempted
    }
    
    return false; // No recovery needed
}

// Checks if the current error count exceeds a threshold for aborting
bool should_abort_processing(BaaPreprocessor *pp_state, size_t max_errors)
{
    if (!pp_state)
        return true;
    
    size_t error_count = 0;
    for (size_t i = 0; i < pp_state->diagnostic_count; i++)
    {
        // Count errors (assuming errors have specific formatting that can be detected)
        // For now, we'll assume all diagnostics are errors until we add warning support
        // TODO: Add is_warning field to PreprocessorDiagnostic
        error_count++;
    }
    
    if (error_count >= max_errors)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_fatal_diagnostic(pp_state, &error_loc,
            L"تم تجاوز الحد الأقصى للأخطاء (%zu) - إيقاف المعالجة", max_errors);
        // Note: Enhanced diagnostics system handles error reporting
        return true;
    }
    
    return false;
}

// Attempts to recover from include errors by trying alternative paths
bool attempt_include_recovery(BaaPreprocessor *pp_state, const wchar_t *failed_path, 
                             wchar_t **recovered_content)
{
    if (!pp_state || !failed_path || !recovered_content)
        return false;
    
    *recovered_content = NULL;
    
    // Convert wide path to multibyte for file operations
    char *failed_path_mb = NULL;
    int required_bytes = WideCharToMultiByte(CP_UTF8, 0, failed_path, -1, NULL, 0, NULL, NULL);
    if (required_bytes > 0)
    {
        failed_path_mb = malloc(required_bytes);
        if (failed_path_mb)
        {
            WideCharToMultiByte(CP_UTF8, 0, failed_path, -1, failed_path_mb, required_bytes, NULL, NULL);
        }
    }
    
    if (!failed_path_mb)
        return false;
    
    // Try alternative extensions
    const char *alt_extensions[] = {".baa", ".h", ".hpp", ".txt", NULL};
    
    for (int i = 0; alt_extensions[i]; i++)
    {
        char alt_path[MAX_PATH_LEN];
        snprintf(alt_path, MAX_PATH_LEN, "%s%s", failed_path_mb, alt_extensions[i]);
        
        wchar_t *temp_error = NULL;
        wchar_t *content = read_file_content(pp_state, alt_path, &temp_error);
        
        if (content)
        {
            // Successfully found alternative
            free(failed_path_mb);
            if (temp_error) free(temp_error);
            
            PpSourceLocation recovery_loc = get_current_original_location(pp_state);
            wchar_t *alt_found_msg = format_preprocessor_warning_at_location(&recovery_loc,
                L"تم العثور على ملف بديل: %hs", alt_path);
            if (alt_found_msg) {
                fwprintf(stderr, L"%ls\n", alt_found_msg);
                free(alt_found_msg);
            }
            
            *recovered_content = content;
            return true;
        }
        
        if (temp_error) free(temp_error);
    }
    
    free(failed_path_mb);
    return false;
}

// Clears error state to allow continued processing
void clear_error_state(BaaPreprocessor *pp_state)
{
    if (pp_state)
    {
        pp_state->had_error_this_pass = false;
    }
}

// Checks if we can continue processing after an error
bool can_continue_after_error(BaaPreprocessor *pp_state, const wchar_t *current_context)
{
    if (!pp_state)
        return false;
    
    // Don't continue if we have too many errors
    if (should_abort_processing(pp_state, 50)) // Max 50 errors
        return false;
    
    // Don't continue if we're in a critical section (like macro expansion)
    if (pp_state->expanding_macros_count > 10) // Prevent runaway recursion
        return false;
    
    // Check for infinite loops in conditional processing
    if (pp_state->conditional_stack_count > 100) // Reasonable nesting limit
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_error_with_suggestion(pp_state, &error_loc,
            L"قلل من مستوى التداخل أو أعد تنظيم التوجيهات الشرطية",
            L"تم تجاوز حد تداخل التوجيهات الشرطية (العدد: %zu)", pp_state->conditional_stack_count);
        // Note: Enhanced diagnostics system handles error reporting
        return false;
    }
    
    return true;
}

// Enhanced error continuation check with progressive limits
bool can_continue_after_error_enhanced(BaaPreprocessor *pp_state, const wchar_t *current_context)
{
    if (!pp_state)
        return false;
    
    // Progressive error limits based on nesting depth
    size_t max_errors = 50;
    if (pp_state->nesting_depth > 30) {
        max_errors = 10; // Stricter limit for deep nesting
    } else if (pp_state->nesting_depth > 20) {
        max_errors = 20;
    } else if (pp_state->nesting_depth > 10) {
        max_errors = 35;
    }
    
    // Check for fatal errors - stop immediately
    if (pp_state->fatal_error_count > 0)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_info_diagnostic(pp_state, &error_loc, L"توقف المعالجة بسبب خطأ فادح");
        return false;
    }
    
    // Check total error count against progressive limit
    if (pp_state->error_count >= max_errors)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_error_with_suggestion(pp_state, &error_loc, 
            L"راجع الأخطاء السابقة وقم بإصلاحها قبل المتابعة",
            L"تم تجاوز الحد الأقصى للأخطاء (%zu) في عمق التداخل %zu", 
            max_errors, pp_state->nesting_depth);
        return false;
    }
    
    // Check nesting depth limits
    if (pp_state->nesting_depth > pp_state->max_nesting_depth)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_fatal_diagnostic(pp_state, &error_loc,
            L"تم تجاوز الحد الأقصى لعمق تداخل الملفات (%zu)", pp_state->max_nesting_depth);
        return false;
    }
    
    // Check for runaway macro expansion
    if (pp_state->expanding_macros_count > 20)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_fatal_diagnostic(pp_state, &error_loc,
            L"تم تجاوز حد توسيع الماكرو - احتمال تكرار لا نهائي");
        return false;
    }
    
    // Check conditional nesting
    if (pp_state->conditional_stack_count > 50)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        add_error_with_suggestion(pp_state, &error_loc,
            L"تحقق من أن كل #إذا له #نهاية_إذا مطابق",
            L"عمق التداخل للتوجيهات الشرطية مرتفع جداً (%zu)", pp_state->conditional_stack_count);
        return false;
    }
    
    return true;
}

// Enhanced directive recovery with context-aware suggestions
bool attempt_directive_recovery_enhanced(BaaPreprocessor *pp_state, const wchar_t **current_pos, const wchar_t *directive_type)
{
    if (!pp_state || !current_pos || !*current_pos)
        return false;
    
    PpSourceLocation error_loc = get_current_original_location(pp_state);
    
    // Provide context-specific recovery suggestions
    const wchar_t *recovery_suggestion = NULL;
    if (directive_type)
    {
        if (wcscmp(directive_type, L"إذا") == 0 || wcscmp(directive_type, L"إذا_عرف") == 0 || wcscmp(directive_type, L"إذا_لم_يعرف") == 0)
        {
            recovery_suggestion = L"تحقق من صحة التعبير الشرطي وتأكد من إنهاء الكتلة بـ #نهاية_إذا";
        }
        else if (wcscmp(directive_type, L"تضمين") == 0)
        {
            recovery_suggestion = L"تحقق من وجود الملف وصحة المسار، استخدم علامات الاقتباس المناسبة";
        }
        else if (wcscmp(directive_type, L"عرف") == 0)
        {
            recovery_suggestion = L"تحقق من صحة اسم الماكرو والتعريف";
        }
        else
        {
            recovery_suggestion = L"راجع صيغة التوجيه أو استخدم توجيهاً صحيحاً";
        }
    }
    
    // Report recovery attempt with suggestion
    add_warning_with_suggestion(pp_state, &error_loc, recovery_suggestion,
        L"محاولة استرداد من خطأ التوجيه '%ls' - البحث عن نقطة آمنة للمتابعة", 
        directive_type ? directive_type : L"غير معروف");
    
    // Find next directive
    const wchar_t *next_directive = find_next_directive(*current_pos);
    
    if (next_directive)
    {
        // Count skipped lines for accurate line tracking
        const wchar_t *line_counter = *current_pos;
        size_t skipped_lines = 0;
        while (line_counter < next_directive)
        {
            if (*line_counter == L'\n')
            {
                pp_state->current_line_number++;
                skipped_lines++;
            }
            line_counter++;
        }
        
        *current_pos = next_directive;
        pp_state->current_column_number = 1;
        
        // Report successful recovery
        PpSourceLocation recovery_loc = get_current_original_location(pp_state);
        add_info_diagnostic(pp_state, &recovery_loc,
            L"تم الاسترداد بنجاح - تم تخطي %zu سطر، استئناف المعالجة من السطر %zu", 
            skipped_lines, pp_state->current_line_number);
        
        return true;
    }
    
    // If no next directive found, skip to end
    skip_to_next_line(pp_state, current_pos);
    add_warning_with_suggestion(pp_state, &error_loc,
        L"تحقق من بقية الملف للتأكد من عدم وجود توجيهات معلقة",
        L"لم يتم العثور على توجيه صالح للاسترداد");
    return false;
}

// Enhanced include recovery with better suggestions and alternative searching
bool attempt_include_recovery_enhanced(BaaPreprocessor *pp_state, const wchar_t *failed_path, wchar_t **recovered_content)
{
    if (!pp_state || !failed_path || !recovered_content)
        return false;
    
    *recovered_content = NULL;
    PpSourceLocation error_loc = get_current_original_location(pp_state);
    
    // Convert wide path to multibyte for file operations
    char *failed_path_mb = NULL;
    int required_bytes = WideCharToMultiByte(CP_UTF8, 0, failed_path, -1, NULL, 0, NULL, NULL);
    if (required_bytes > 0)
    {
        failed_path_mb = malloc(required_bytes);
        if (failed_path_mb)
        {
            WideCharToMultiByte(CP_UTF8, 0, failed_path, -1, failed_path_mb, required_bytes, NULL, NULL);
        }
    }
    
    if (!failed_path_mb)
    {
        add_error_with_suggestion(pp_state, &error_loc,
            L"تحقق من صحة اسم الملف وترميز المسار",
            L"فشل في تحويل مسار الملف للبحث عن بدائل");
        return false;
    }
    
    // Try alternative extensions with specific suggestions
    const char *alt_extensions[] = {".baa", ".h", ".hpp", ".inc", ".txt", NULL};
    const wchar_t *extension_suggestions[] = {
        L"ملف بلغة باء - تحقق من صحة الاسم",
        L"ملف رأسية C - قد يحتاج تعديل للتوافق مع باء", 
        L"ملف رأسية C++ - قد يحتاج تعديل للتوافق مع باء",
        L"ملف تضمين - تحقق من المحتوى",
        L"ملف نصي - قد لا يحتوي على توجيهات صحيحة"
    };
    
    for (int i = 0; alt_extensions[i]; i++)
    {
        char alt_path[MAX_PATH_LEN];
        snprintf(alt_path, MAX_PATH_LEN, "%s%s", failed_path_mb, alt_extensions[i]);
        
        wchar_t *temp_error = NULL;
        wchar_t *content = read_file_content(pp_state, alt_path, &temp_error);
        
        if (content)
        {
            // Successfully found alternative
            free(failed_path_mb);
            if (temp_error) free(temp_error);
            
            add_warning_with_suggestion(pp_state, &error_loc, extension_suggestions[i],
                L"تم العثور على ملف بديل: %hs", alt_path);
            
            *recovered_content = content;
            return true;
        }
        
        if (temp_error) free(temp_error);
    }
    
    // Try searching in include paths with more detailed reporting
    if (pp_state->include_paths)
    {
        for (size_t i = 0; i < pp_state->include_path_count; i++)
        {
            char full_path[MAX_PATH_LEN];
            snprintf(full_path, MAX_PATH_LEN, "%s%c%s", 
                pp_state->include_paths[i], PATH_SEPARATOR, failed_path_mb);
            
            wchar_t *temp_error = NULL;
            wchar_t *content = read_file_content(pp_state, full_path, &temp_error);
            
            if (content)
            {
                free(failed_path_mb);
                if (temp_error) free(temp_error);
                
                add_warning_with_suggestion(pp_state, &error_loc,
                    L"تم العثور على الملف في مسار التضمين - تحقق من أن هذا هو المطلوب",
                    L"تم العثور على الملف في مسار التضمين: %s", pp_state->include_paths[i]);
                
                *recovered_content = content;
                return true;
            }
            
            if (temp_error) free(temp_error);
        }
    }
    
    // Final attempt - report detailed failure with comprehensive suggestions
    add_error_with_suggestion(pp_state, &error_loc,
        L"1. تحقق من وجود الملف في المسار الصحيح\n"
        L"2. تأكد من أن اسم الملف مكتوب بشكل صحيح\n"
        L"3. أضف مسار الملف إلى مسارات التضمين\n"
        L"4. تحقق من أذونات الوصول للملف",
        L"فشل في العثور على الملف '%ls' أو أي بديل مناسب", failed_path);
    
    free(failed_path_mb);
    return false;
}
