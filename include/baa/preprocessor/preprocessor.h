#ifndef BAA_PREPROCESSOR_H
#define BAA_PREPROCESSOR_H

#include <wchar.h>
#include <stdbool.h>
#include <stddef.h> // For size_t

// Structure to hold a macro definition
typedef struct
{
    wchar_t *name;         // Macro name
    wchar_t *body;         // Macro body (replacement text)
    bool is_function_like; // True if defined with (...)
    size_t param_count;    // Number of named parameters
    wchar_t **param_names; // Array of named parameter names (NULL if not function-like)
    bool is_variadic;      // True if the function-like macro is variadic (ends with وسائط_إضافية)
} BaaMacro;

// Forward declaration for the preprocessor state structure (Opaque Pointer)
// The actual definition is internal to the preprocessor library.
typedef struct BaaPreprocessor BaaPreprocessor;

// --- Types for Input Source Abstraction ---

// Enum defining the type of input source for the preprocessor
typedef enum {
    BAA_PP_SOURCE_FILE,   // Input is a file path
    BAA_PP_SOURCE_STRING, // Input is a wide character string in memory
    // BAA_PP_SOURCE_STDIN // Future: Input from standard input
} BaaPpSourceType;

// Structure to represent the input source for the preprocessor
typedef struct {
    BaaPpSourceType type;       // The type of the source
    const char* source_name;    // Name for error messages (e.g., filename or "<string>")
    union {
        const char* file_path;          // Path to the source file (if type is FILE)
        const wchar_t* source_string;   // Pointer to the source string (if type is STRING)
    } data;
} BaaPpSource;


// --- Function Declarations ---

/**
 * @brief Processes Baa source code from a specified source, handling preprocessor directives.
 *
 * This function takes a description of the source (file path or string), processes directives
 * like #تضمين (include), expands macros, and returns a dynamically allocated wide character
 * string containing the fully processed source code.
 *
 * The caller is responsible for freeing the returned string using free().
 *
 * @param source A pointer to a BaaPpSource struct describing the input source.
 * @param include_paths Null-terminated array of strings representing standard include directories for `#تضمين <...>`. Can be NULL.
 * @param error_message Pointer to a wchar_t* that will be set to an allocated error message on failure (caller must free).
 * @return A dynamically allocated wchar_t* containing the processed source code, or NULL on failure.
 */
wchar_t* baa_preprocess(const BaaPpSource* source, const char** include_paths, wchar_t** error_message);

#endif // BAA_PREPROCESSOR_H
