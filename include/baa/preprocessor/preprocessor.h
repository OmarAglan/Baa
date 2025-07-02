#ifndef BAA_PREPROCESSOR_H
#define BAA_PREPROCESSOR_H

#include <wchar.h>
#include <stdbool.h>
#include <stddef.h> // For size_t

/**
 * @brief Structure to hold a macro definition
 *
 * This structure represents a preprocessor macro, which can be either object-like
 * (simple text replacement) or function-like (with parameters). Function-like
 * macros can also be variadic, accepting a variable number of arguments.
 */
typedef struct
{
    wchar_t *name;         ///< Macro name (wide character string)
    wchar_t *body;         ///< Macro body/replacement text (wide character string)
    bool is_function_like; ///< True if macro is function-like (defined with parentheses)
    size_t param_count;    ///< Number of named parameters (0 for object-like macros)
    wchar_t **param_names; ///< Array of parameter names (NULL if not function-like)
    bool is_variadic;      ///< True if macro accepts variadic arguments (وسائط_إضافية)
} BaaMacro;

/**
 * @brief Forward declaration for the preprocessor state structure (Opaque Pointer)
 *
 * The actual definition is internal to the preprocessor library. This structure
 * maintains all preprocessor state including macros, conditional compilation stack,
 * error collection, file inclusion stack, and recovery state.
 */
typedef struct BaaPreprocessor BaaPreprocessor;

// --- Types for Input Source Abstraction ---

/**
 * @brief Enum defining the type of input source for the preprocessor
 */
typedef enum {
    BAA_PP_SOURCE_FILE,   ///< Input is a file path (UTF-8 or UTF-16LE with BOM)
    BAA_PP_SOURCE_STRING, ///< Input is a wide character string in memory (UTF-16LE)
    // BAA_PP_SOURCE_STDIN // Future: Input from standard input
} BaaPpSourceType;

/**
 * @brief Structure to represent the input source for the preprocessor
 *
 * This structure abstracts the input source, allowing the preprocessor to work
 * with either files or in-memory strings. The source_name is used for error
 * reporting and should be descriptive (e.g., filename or "<string>").
 */
typedef struct {
    BaaPpSourceType type;       ///< The type of the source (file or string)
    const char* source_name;    ///< Name for error messages (e.g., filename or "<string>")
    union {
        const char* file_path;          ///< Path to the source file (if type is BAA_PP_SOURCE_FILE)
        const wchar_t* source_string;   ///< Pointer to the source string (if type is BAA_PP_SOURCE_STRING)
    } data;
} BaaPpSource;


// --- Function Declarations ---

/**
 * @brief Processes Baa source code from a specified source, handling preprocessor directives.
 *
 * This function is the main entry point for the Baa preprocessor. It takes a description
 * of the source (file path or string), processes all preprocessor directives, expands
 * macros, handles conditional compilation, and returns a dynamically allocated wide
 * character string containing the fully processed source code ready for lexical analysis.
 *
 * The preprocessor handles the following features:
 * - File inclusion (#تضمين) with circular include detection
 * - Macro definitions (#تعريف) including object-like, function-like, and variadic macros
 * - Macro expansion with rescanning and recursion detection
 * - Conditional compilation (#إذا, #إذا_عرف, #وإلا_إذا, #إلا, #نهاية_إذا)
 * - Expression evaluation with arithmetic, logical, and bitwise operators
 * - Predefined macros (__الملف__, __السطر__, __التاريخ__, __الوقت__, __الدالة__, __إصدار_المعيار_باء__)
 * - Error and warning directives (#خطأ, #تحذير)
 * - Comprehensive error recovery and multi-error reporting
 *
 * Input files are automatically detected as UTF-8 (default) or UTF-16LE (with BOM).
 * Output is always UTF-16LE wide character string.
 *
 * The caller is responsible for freeing both the returned string and any error message
 * using free().
 *
 * @param source A pointer to a BaaPpSource struct describing the input source. Must not be NULL.
 * @param include_paths Null-terminated array of strings representing standard include
 *                      directories for `#تضمين <...>`. Can be NULL if no standard paths needed.
 * @param error_message Pointer to a wchar_t* that will be set to an allocated error message
 *                      on failure (caller must free). Must not be NULL.
 * @return A dynamically allocated wchar_t* containing the processed source code in UTF-16LE
 *         encoding, or NULL on failure. The caller must free this using free().
 *
 * @note This function is not thread-safe. Each preprocessing operation should use a
 *       separate call with its own parameters.
 *
 * @see BaaPpSource, BaaPpSourceType
 */
wchar_t* baa_preprocess(const BaaPpSource* source, const char** include_paths, wchar_t** error_message);

#endif // BAA_PREPROCESSOR_H
