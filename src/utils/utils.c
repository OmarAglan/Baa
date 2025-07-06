#include "baa/utils/utils.h"
#include "baa/utils/errors.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <errno.h>

// Error handling state
static BaaError current_error = BAA_SUCCESS;
static wchar_t error_message[1024] = {0};

void baa_set_error(BaaError error, const wchar_t *message)
{
    current_error = error;
    if (message)
    {
        wcsncpy_s(error_message, sizeof(error_message) / sizeof(wchar_t), message, sizeof(error_message) / sizeof(wchar_t) - 1);
        error_message[sizeof(error_message) / sizeof(wchar_t) - 1] = L'\0';
    }
    else
    {
        error_message[0] = L'\0';
    }
}

const wchar_t *baa_get_error_message(void)
{
    return error_message;
}

BaaError baa_get_error(void)
{
    return current_error;
}

void *baa_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
    {
        baa_set_error(BAA_ERROR_MEMORY, L"فشل في تخصيص الذاكرة");
    }
    return ptr;
}

void *baa_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr)
    {
        baa_set_error(BAA_ERROR_MEMORY, L"فشل في إعادة تخصيص الذاكرة");
    }
    return new_ptr;
}

void baa_free(void *ptr)
{
    free(ptr);
}

wchar_t *baa_strdup(const wchar_t *str)
{
    if (!str)
    {
        return NULL;
    }

    size_t len = wcslen(str) + 1;
    wchar_t *new_str = baa_malloc(len * sizeof(wchar_t));

    if (new_str)
    {
        wcscpy_s(new_str, len, str);
    }

    return new_str;
}

wchar_t *baa_strndup(const wchar_t *str, size_t n)
{
    if (!str)
    {
        return NULL;
    }

    size_t len = wcsnlen(str, n);
    wchar_t *new_str = baa_malloc((len + 1) * sizeof(wchar_t));

    if (new_str)
    {
        wcsncpy_s(new_str, len + 1, str, len);
        new_str[len] = L'\0';
    }

    return new_str;
}

char *baa_strdup_char(const char *str)
{
    if (!str)
    {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    char *new_str = baa_malloc(len); // Uses your baa_malloc
    if (new_str)
    {
#ifdef _WIN32
        strcpy_s(new_str, len, str);
#else
        strcpy(new_str, str);
#endif
    }
    return new_str;
}

int baa_strcmp(const wchar_t *s1, const wchar_t *s2)
{
    if (!s1 || !s2)
    {
        return s1 ? 1 : (s2 ? -1 : 0);
    }
    return wcscmp(s1, s2);
}

/**
 * Read the contents of a file into a wide string
 *
 * @param filename The path to the file to read
 * @return A newly allocated string with the file contents, or NULL on error
 */
wchar_t *baa_read_file(const wchar_t *filename)
{
    FILE *file = NULL;

#ifdef _WIN32
    errno_t err = _wfopen_s(&file, filename, L"rb");
    if (err != 0)
    {
        file = NULL;
    }
#else
    // Convert wchar_t* to char* for non-Windows systems
    size_t len = wcslen(filename);
    char *narrow_filename = (char *)malloc(len * 4 + 1); // UTF-8 can use up to 4 bytes per character
    if (!narrow_filename)
    {
        baa_set_error(BAA_ERROR_MEMORY, L"فشل في تخصيص الذاكرة");
        return NULL;
    }

    wcstombs(narrow_filename, filename, len * 4);
    narrow_filename[len * 4] = '\0';

    file = fopen(narrow_filename, "rb");
    free(narrow_filename);
#endif

    if (!file)
    {
        baa_set_error(BAA_ERROR_FILE_OPEN, L"فشل في فتح الملف");
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(file);
        baa_set_error(BAA_ERROR_FILE_EMPTY, L"الملف فارغ أو غير صالح");
        return NULL;
    }

    // Allocate buffer for file content
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer)
    {
        fclose(file);
        baa_set_error(BAA_ERROR_MEMORY, L"فشل في تخصيص الذاكرة");
        return NULL;
    }

    // Read file content
    size_t read_size = fread(buffer, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size)
    {
        free(buffer);
        baa_set_error(BAA_ERROR_FILE_READ, L"فشل في قراءة الملف");
        return NULL;
    }

    buffer[file_size] = '\0';

    // Convert to wide string
    size_t wide_len;
    errno_t conv_err = mbstowcs_s(&wide_len, NULL, 0, buffer, 0);
    if (conv_err != 0)
    {
        free(buffer);
        baa_set_error(BAA_ERROR_ENCODING, L"فشل في تحويل الترميز");
        return NULL;
    }

    wchar_t *wide_buffer = (wchar_t *)malloc(wide_len * sizeof(wchar_t));
    if (!wide_buffer)
    {
        free(buffer);
        baa_set_error(BAA_ERROR_MEMORY, L"فشل في تخصيص الذاكرة");
        return NULL;
    }

    size_t converted;
    conv_err = mbstowcs_s(&converted, wide_buffer, wide_len, buffer, wide_len - 1);
    if (conv_err != 0)
    {
        free(buffer);
        free(wide_buffer);
        baa_set_error(BAA_ERROR_ENCODING, L"فشل في تحويل الترميز");
        return NULL;
    }
    free(buffer);

    return wide_buffer;
}

// Implementation moved from lexer.c
long baa_file_size(FILE *file)
{
    if (!file)
    {
        return 0;
    }
    fpos_t original;
    if (fgetpos(file, &original) != 0)
    {
        // Consider using baa_set_error here or a more robust error handling
        // For now, matches original print and return.
        printf("fgetpos() فشلت دالة: %i \n", errno);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    long out = ftell(file);
    if (fsetpos(file, &original) != 0)
    {
        printf("fsetpos() فشلت دالة: %i \n", errno);
    }
    return out;
}

// Implementation moved from lexer.c
// This version specifically handles UTF-16LE with BOM, suitable for Baa source files.
wchar_t *baa_file_content(const wchar_t *path)
{
    FILE *file;
#ifdef _WIN32
    // Use _wfopen_s on Windows for potentially non-ASCII paths
    errno_t err = _wfopen_s(&file, path, L"rb"); // Removed ", ccs=UTF-16LE" as BOM check is manual
    if (err != 0)
    {
        // baa_set_error(BAA_ERROR_FILE_OPEN, L"فشل في فتح الملف"); // Consider using baa_set_error
        printf("لا يمكن فتح الملف (wfopen_s)\n");
        return NULL;
    }
#else
    // Convert wchar_t* to char* for non-Windows systems
    size_t len = wcslen(path);
    char *narrow_path = (char *)malloc(len * 4 + 1); // Max UTF-8 bytes
    if (!narrow_path)
    {
        // baa_set_error(BAA_ERROR_MEMORY, L"فشل في تخصيص الذاكرة لمسار الملف");
        printf("فشل تخصيص الذاكرة لمسار الملف\n");
        return NULL;
    }
    if (wcstombs(narrow_path, path, len * 4) == (size_t)-1)
    {
        free(narrow_path);
        // baa_set_error(BAA_ERROR_ENCODING, L"فشل في تحويل مسار الملف إلى متعدد البايتات");
        printf("فشل تحويل مسار الملف\n");
        return NULL;
    }
    narrow_path[len * 4] = '\0'; // Ensure null termination if wcstombs doesn't fill buffer

    file = fopen(narrow_path, "rb");
    free(narrow_path);
    if (!file)
    {
        // baa_set_error(BAA_ERROR_FILE_OPEN, L"فشل في فتح الملف");
        printf("لا يمكن فتح الملف (fopen)\n");
        return NULL;
    }
#endif

    // Skip BOM if present (UTF-16LE BOM is FF FE)
    unsigned char bom_check[2];
    if (fread(bom_check, 1, 2, file) == 2)
    {
        if (bom_check[0] != 0xFF || bom_check[1] != 0xFE)
        {
            fseek(file, 0, SEEK_SET); // Not a UTF-16LE BOM, rewind
        }
        // If it is a BOM, we've consumed it.
    }
    else
    {
        fseek(file, 0, SEEK_SET); // Couldn't read 2 bytes, rewind.
    }

    long file_size_bytes = baa_file_size(file); // Use the utility function
    if (file_size_bytes <= 0 && ftell(file) == 0)
    {   // Check if file is truly empty or baa_file_size failed
        // If ftell is also 0 after BOM check and rewind, it might be an empty file or just BOM
        fseek(file, 0, SEEK_END);
        file_size_bytes = ftell(file); // Re-check size from current position
        fseek(file, 0, SEEK_SET);      // Rewind to start after BOM check

        // Re-read and check BOM after ensuring position
        if (fread(bom_check, 1, 2, file) == 2)
        {
            if (bom_check[0] != 0xFF || bom_check[1] != 0xFE)
            {
                fseek(file, 0, SEEK_SET);
            }
        }
        else
        {
            fseek(file, 0, SEEK_SET);
        }
        file_size_bytes = baa_file_size(file); // Final check
    }

    // Correct calculation for content size after BOM handling
    long current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    long end_pos = ftell(file);
    fseek(file, current_pos, SEEK_SET); // Restore position after BOM check

    long content_size_bytes_actual = end_pos - current_pos;

    if (content_size_bytes_actual < 0)
        content_size_bytes_actual = 0;

    if (content_size_bytes_actual == 0)
    { // File is empty or only BOM
        fclose(file);
        wchar_t *contents = baa_malloc(sizeof(wchar_t));
        if (!contents)
            return NULL;
        contents[0] = L'\0';
        return contents;
    }

    // Ensure size is multiple of wchar_t for direct wchar_t reading
    if (content_size_bytes_actual % sizeof(wchar_t) != 0)
    {
        fclose(file);
        // baa_set_error(BAA_ERROR_FILE_FORMAT, L"حجم الملف غير متوافق مع UTF-16LE");
        printf("حجم الملف غير متوافق مع UTF-16LE\n");
        return NULL;
    }

    size_t char_count = (content_size_bytes_actual / sizeof(wchar_t));
    wchar_t *contents = baa_malloc((char_count + 1) * sizeof(wchar_t));

    if (!contents)
    {
        fclose(file);
        // baa_set_error(BAA_ERROR_MEMORY, L"فشل تخصيص الذاكرة لمحتوى الملف");
        return NULL;
    }

    size_t chars_read = fread(contents, sizeof(wchar_t), char_count, file);
    contents[chars_read] = L'\0'; // Null-terminate based on actual chars read

    fclose(file);

    if (chars_read != char_count)
    {
        // Potentially partial read, though fread itself doesn't distinguish short read from error easily
        // baa_set_error(BAA_ERROR_FILE_READ, L"فشل في قراءة محتوى الملف بالكامل");
        // For now, return what was read, null-terminated.
        // Caller might need to check ferror(file) before fclose if this is critical.
    }

    return contents;
}
