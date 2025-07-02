#ifndef BAA_PREPROCESSOR_INTERNAL_H
#define BAA_PREPROCESSOR_INTERNAL_H

#include "baa/preprocessor/preprocessor.h" // Include public header (for BaaMacro, baa_preprocess signature)
#include "baa/utils/utils.h"               // For BaaBool, etc. if needed, or maybe include specific headers
#include <wchar.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h> // For expression evaluator

// Platform specific includes needed by utils/core
#ifdef _WIN32
#include <windows.h>
#include <direct.h> // _getcwd
#define PATH_SEPARATOR '\\'
#define MAX_PATH_LEN MAX_PATH
#else
#include <unistd.h> // getcwd
#include <limits.h> // PATH_MAX
#include <libgen.h> // dirname
#define PATH_SEPARATOR '/'
#define MAX_PATH_LEN PATH_MAX
#endif

// --- Struct Definitions ---

// Structure to hold source location information (file, line, column)
typedef struct
{
    const char *file_path; // Use char* consistent with internal preprocessor paths
    size_t line;
    size_t column;
} PpSourceLocation;

// Enhanced severity levels for diagnostic classification
typedef enum
{
    PP_DIAG_FATAL,   // System errors - halt immediately (memory, I/O)
    PP_DIAG_ERROR,   // Syntax/semantic errors - continue with recovery
    PP_DIAG_WARNING, // Warnings - continue processing normally
    PP_DIAG_NOTE     // Informational - continue processing normally
} PpDiagnosticSeverity;

// Error code ranges for categorization
#define PP_ERROR_DIRECTIVE_BASE 1000
#define PP_ERROR_MACRO_BASE 2000
#define PP_ERROR_EXPRESSION_BASE 3000
#define PP_ERROR_FILE_BASE 4000
#define PP_ERROR_MEMORY_BASE 5000
#define PP_ERROR_SYNTAX_BASE 6000

// Specific error codes
// Directive errors (1000-1999)
#define PP_ERROR_UNKNOWN_DIRECTIVE (PP_ERROR_DIRECTIVE_BASE + 1)
#define PP_ERROR_MISSING_ENDIF (PP_ERROR_DIRECTIVE_BASE + 2)
#define PP_ERROR_INVALID_INCLUDE (PP_ERROR_DIRECTIVE_BASE + 3)
#define PP_ERROR_MISSING_MACRO_NAME (PP_ERROR_DIRECTIVE_BASE + 4)
#define PP_ERROR_INVALID_MACRO_PARAM (PP_ERROR_DIRECTIVE_BASE + 5)
#define PP_ERROR_UNTERMINATED_CONDITION (PP_ERROR_DIRECTIVE_BASE + 6)
#define PP_ERROR_INVALID_DIRECTIVE_SYNTAX (PP_ERROR_DIRECTIVE_BASE + 7)
#define PP_ERROR_UNEXPECTED_DIRECTIVE (PP_ERROR_DIRECTIVE_BASE + 8)
#define PP_ERROR_DIRECTIVE_NOT_ALLOWED (PP_ERROR_DIRECTIVE_BASE + 9)
#define PP_ERROR_MALFORMED_DIRECTIVE (PP_ERROR_DIRECTIVE_BASE + 10)

// Macro errors (2000-2999)
#define PP_ERROR_MACRO_REDEFINITION (PP_ERROR_MACRO_BASE + 1)
#define PP_ERROR_MACRO_ARG_MISMATCH (PP_ERROR_MACRO_BASE + 2)
#define PP_ERROR_MACRO_EXPANSION_FAILED (PP_ERROR_MACRO_BASE + 3)
#define PP_ERROR_INVALID_CONCATENATION (PP_ERROR_MACRO_BASE + 4)
#define PP_ERROR_STRINGIFICATION_ERROR (PP_ERROR_MACRO_BASE + 5)
#define PP_ERROR_MACRO_TOO_COMPLEX (PP_ERROR_MACRO_BASE + 6)
#define PP_ERROR_RECURSIVE_MACRO (PP_ERROR_MACRO_BASE + 7)
#define PP_ERROR_INVALID_MACRO_NAME (PP_ERROR_MACRO_BASE + 8)

// Expression errors (3000-3999)
#define PP_ERROR_DIVISION_BY_ZERO (PP_ERROR_EXPRESSION_BASE + 1)
#define PP_ERROR_UNDEFINED_IDENTIFIER (PP_ERROR_EXPRESSION_BASE + 2)
#define PP_ERROR_INVALID_OPERATOR (PP_ERROR_EXPRESSION_BASE + 3)
#define PP_ERROR_EXPRESSION_TOO_COMPLEX (PP_ERROR_EXPRESSION_BASE + 4)
#define PP_ERROR_UNBALANCED_PARENTHESES (PP_ERROR_EXPRESSION_BASE + 5)
#define PP_ERROR_INVALID_NUMBER_FORMAT (PP_ERROR_EXPRESSION_BASE + 6)
#define PP_ERROR_EXPRESSION_OVERFLOW (PP_ERROR_EXPRESSION_BASE + 7)

// File errors (4000-4999)
#define PP_ERROR_FILE_NOT_FOUND (PP_ERROR_FILE_BASE + 1)
#define PP_ERROR_CIRCULAR_INCLUDE (PP_ERROR_FILE_BASE + 2)
#define PP_ERROR_ENCODING_ERROR (PP_ERROR_FILE_BASE + 3)
#define PP_ERROR_PERMISSION_DENIED (PP_ERROR_FILE_BASE + 4)
#define PP_ERROR_FILE_TOO_LARGE (PP_ERROR_FILE_BASE + 5)
#define PP_ERROR_INVALID_FILE_PATH (PP_ERROR_FILE_BASE + 6)

// Memory errors (5000-5999)
#define PP_ERROR_OUT_OF_MEMORY (PP_ERROR_MEMORY_BASE + 1)
#define PP_ERROR_BUFFER_OVERFLOW (PP_ERROR_MEMORY_BASE + 2)
#define PP_ERROR_ALLOCATION_FAILED (PP_ERROR_MEMORY_BASE + 3)

// Syntax errors (6000-6999)
#define PP_ERROR_UNEXPECTED_TOKEN (PP_ERROR_SYNTAX_BASE + 1)
#define PP_ERROR_MISSING_TOKEN (PP_ERROR_SYNTAX_BASE + 2)
#define PP_ERROR_INVALID_CHARACTER (PP_ERROR_SYNTAX_BASE + 3)
#define PP_ERROR_UNTERMINATED_STRING (PP_ERROR_SYNTAX_BASE + 4)
#define PP_ERROR_UNTERMINATED_COMMENT (PP_ERROR_SYNTAX_BASE + 5)

// Error limit configuration
typedef struct
{
    size_t max_errors;      // Maximum errors before stopping (default: 100)
    size_t max_warnings;    // Maximum warnings before stopping (default: 1000)
    size_t max_notes;       // Maximum notes (default: SIZE_MAX - unlimited)
    bool stop_on_fatal;     // Halt on fatal errors (default: true)
    size_t cascading_limit; // Max consecutive errors in same context (default: 10)
} PpErrorLimits;

// Recovery state tracking
typedef struct
{
    size_t consecutive_errors;    // Track cascading failures
    size_t errors_this_line;      // Prevent infinite loops on single line
    size_t directive_errors;      // Track directive-specific issues
    size_t expression_errors;     // Track expression evaluation failures
    bool in_recovery_mode;        // Flag indicating recovery state
    const char *recovery_context; // Current recovery context
} PpRecoveryState;

// Recovery action types
typedef enum
{
    PP_RECOVERY_CONTINUE,         // Continue processing normally
    PP_RECOVERY_SKIP_LINE,        // Skip to next line
    PP_RECOVERY_SKIP_DIRECTIVE,   // Skip current directive
    PP_RECOVERY_SYNC_CONDITIONAL, // Synchronize conditional stack
    PP_RECOVERY_HALT              // Stop processing
} PpRecoveryAction;

// Enhanced structure to store a single diagnostic entry
typedef struct
{
    wchar_t *message;              // Formatted diagnostic message
    PpSourceLocation location;     // Original source location
    PpDiagnosticSeverity severity; // Diagnostic severity level
    uint32_t error_code;           // Unique error identifier (for i18n)
    const char *category;          // Error category ("directive", "macro", etc.)
    wchar_t *suggestion;           // Optional fix suggestion (may be NULL)
} PreprocessorDiagnostic;

/**
 * @brief Main preprocessor state structure
 *
 * This structure holds all the state needed for preprocessing a Baa source file.
 * It includes macro definitions, conditional compilation state, file inclusion
 * tracking, error collection, and recovery state. The structure is designed to
 * support comprehensive error recovery and multi-error reporting.
 *
 * The public header only has a forward declaration - this full definition is
 * internal to the preprocessor implementation.
 */
struct BaaPreprocessor
{
    // Include path management
    const char **include_paths;       ///< Array of standard include directories
    size_t include_path_count;        ///< Number of include paths

    // File inclusion stack (for circular include detection)
    char **open_files_stack;          ///< Stack of currently open files (absolute paths)
    size_t open_files_count;          ///< Number of files currently open
    size_t open_files_capacity;       ///< Capacity of open files stack

    // Macro management
    BaaMacro *macros;                 ///< Array of defined macros
    size_t macro_count;               ///< Number of defined macros
    size_t macro_capacity;            ///< Capacity of macros array

    // Conditional compilation state
    bool *conditional_stack;          ///< Stack tracking #إذا/#نهاية_إذا nesting
    size_t conditional_stack_count;   ///< Depth of conditional nesting
    size_t conditional_stack_capacity;///< Capacity of conditional stack
    bool *conditional_branch_taken_stack; ///< Stack tracking which branches were taken
    size_t conditional_branch_taken_stack_count;   ///< Depth of branch tracking
    size_t conditional_branch_taken_stack_capacity;///< Capacity of branch stack
    bool skipping_lines;              ///< True when inside false conditional block

    // Macro expansion recursion detection
    const BaaMacro **expanding_macros_stack; ///< Stack of currently expanding macros
    size_t expanding_macros_count;    ///< Number of macros currently expanding
    size_t expanding_macros_capacity; ///< Capacity of expansion stack

    // Location tracking
    const char *current_file_path;    ///< Path of currently processing file
    size_t current_line_number;       ///< Current line number (1-based)
    size_t current_column_number;     ///< Current column number (1-based)
    PpSourceLocation *location_stack; ///< Stack to track original source locations
    size_t location_stack_count;      ///< Depth of location stack
    size_t location_stack_capacity;   ///< Capacity of location stack

    // Enhanced error management system
    PreprocessorDiagnostic *diagnostics; ///< Array of collected diagnostics
    size_t diagnostic_count;          ///< Number of collected diagnostics
    size_t diagnostic_capacity;       ///< Capacity of diagnostics array

    // Error counting by severity level
    size_t fatal_count;               ///< Number of fatal errors
    size_t error_count;               ///< Number of errors
    size_t warning_count;             ///< Number of warnings
    size_t note_count;                ///< Number of notes

    // Configuration and recovery state
    PpErrorLimits error_limits;       ///< Configurable error limits
    PpRecoveryState recovery_state;   ///< Current recovery state
    bool had_fatal_error;             ///< True if any fatal error occurred
};

// Dynamic Buffer for Output
typedef struct
{
    wchar_t *buffer;
    size_t length;
    size_t capacity;
} DynamicWcharBuffer;

// Token types for the expression evaluator
typedef enum
{
    PP_EXPR_TOKEN_EOF,
    PP_EXPR_TOKEN_ERROR,
    PP_EXPR_TOKEN_INT_LITERAL,
    PP_EXPR_TOKEN_IDENTIFIER,
    PP_EXPR_TOKEN_DEFINED,
    PP_EXPR_TOKEN_LPAREN,
    PP_EXPR_TOKEN_RPAREN,
    PP_EXPR_TOKEN_PLUS,
    PP_EXPR_TOKEN_MINUS,
    PP_EXPR_TOKEN_STAR,
    PP_EXPR_TOKEN_SLASH,
    PP_EXPR_TOKEN_PERCENT,
    PP_EXPR_TOKEN_EQEQ,
    PP_EXPR_TOKEN_BANGEQ,
    PP_EXPR_TOKEN_LT,
    PP_EXPR_TOKEN_GT,
    PP_EXPR_TOKEN_LTEQ,
    PP_EXPR_TOKEN_GTEQ,
    PP_EXPR_TOKEN_AMPAMP,
    PP_EXPR_TOKEN_PIPEPIPE,
    // Bitwise operators
    PP_EXPR_TOKEN_AMPERSAND, // &
    PP_EXPR_TOKEN_PIPE,      // |
    PP_EXPR_TOKEN_CARET,     // ^
    PP_EXPR_TOKEN_TILDE,     // ~ (Unary)
    PP_EXPR_TOKEN_LSHIFT,    // <<
    PP_EXPR_TOKEN_RSHIFT,    // >>
    // Logical NOT
    PP_EXPR_TOKEN_BANG,
    // Ternary operator
    PP_EXPR_TOKEN_QUESTION,  // ?
    PP_EXPR_TOKEN_COLON,     // :
} PpExprTokenType;

// Token structure for expression evaluator
typedef struct
{
    PpExprTokenType type;
    wchar_t *text;
    long value;
} PpExprToken;

// Tokenizer state for expression evaluator
typedef struct
{
    const wchar_t *current;
    const wchar_t *expression_string_start; // Pointer to the beginning of the full expression string
    size_t expr_string_column_offset;       // Starting column of the expression string on the original line.
    const wchar_t *start;
    BaaPreprocessor *pp_state; // Now points to the struct, not the typedef
    const char *abs_path;
    wchar_t **error_message;
    size_t current_token_start_column; // 1-based column *within the expression string* where the current token started
} PpExprTokenizer;

// --- Function Declarations (Internal Interface) ---

// === Utility Functions (preprocessor_utils.c) ===

/**
 * @brief Initialize a dynamic wide character buffer
 * @param db Buffer to initialize
 * @param initial_capacity Initial capacity in wide characters
 * @return true on success, false on memory allocation failure
 */
bool init_dynamic_buffer(DynamicWcharBuffer *db, size_t initial_capacity);

/**
 * @brief Append a wide character string to dynamic buffer
 * @param db Buffer to append to
 * @param str_to_append String to append (must be null-terminated)
 * @return true on success, false on memory allocation failure
 */
bool append_to_dynamic_buffer(DynamicWcharBuffer *db, const wchar_t *str_to_append);

/**
 * @brief Append first n characters of a wide character string to dynamic buffer
 * @param db Buffer to append to
 * @param str_to_append String to append from
 * @param n Number of characters to append
 * @return true on success, false on memory allocation failure
 */
bool append_dynamic_buffer_n(DynamicWcharBuffer *db, const wchar_t *str_to_append, size_t n);

/**
 * @brief Free resources used by dynamic buffer
 * @param db Buffer to free
 */
void free_dynamic_buffer(DynamicWcharBuffer *db);

/**
 * @brief Duplicate first n characters of a wide character string
 * @param s Source string
 * @param n Number of characters to duplicate
 * @return Allocated copy of first n characters, or NULL on failure
 */
wchar_t *wcsndup_internal(const wchar_t *s, size_t n);

/**
 * @brief Read file content with automatic encoding detection
 *
 * Detects UTF-8 (default) or UTF-16LE (with BOM) encoding and converts
 * to UTF-16LE wide character string for internal processing.
 *
 * @param pp_state Preprocessor state for error reporting
 * @param file_path Path to file to read
 * @param error_message Output parameter for error message on failure
 * @return Allocated wide character string, or NULL on failure
 */
wchar_t *read_file_content(BaaPreprocessor *pp_state, const char *file_path, wchar_t **error_message);

/**
 * @brief Get absolute path from relative or absolute path
 * @param file_path Input path
 * @return Allocated absolute path, or NULL on failure
 */
char *get_absolute_path(const char *file_path);

/**
 * @brief Extract directory part from file path
 * @param file_path Input file path
 * @return Allocated directory path, or NULL on failure
 */
char *get_directory_part(const char *file_path);

/**
 * @brief Format error message with location information
 * @param location Source location for error
 * @param format Printf-style format string
 * @return Allocated formatted error message
 */
wchar_t *format_preprocessor_error_at_location(const PpSourceLocation *location, const wchar_t *format, ...);

/**
 * @brief Format warning message with location information
 * @param location Source location for warning
 * @param format Printf-style format string
 * @return Allocated formatted warning message
 */
wchar_t *format_preprocessor_warning_at_location(const PpSourceLocation *location, const wchar_t *format, ...);

/**
 * @brief Legacy function to add a diagnostic to preprocessor state
 * @param pp_state Preprocessor state
 * @param loc Source location
 * @param is_error true for error, false for warning
 * @param format Printf-style format string
 * @param args_list Variable argument list
 */
void add_preprocessor_diagnostic(BaaPreprocessor *pp_state, const PpSourceLocation *loc, bool is_error, const wchar_t *format, va_list args_list);

// === Enhanced Diagnostic System ===

/**
 * @brief Add a diagnostic message with full context and categorization
 *
 * This is the main function for reporting errors, warnings, and other diagnostics
 * in the enhanced error system. It supports error codes, categories, suggestions,
 * and automatic error counting and limiting.
 *
 * @param pp_state Preprocessor state
 * @param loc Source location of the diagnostic
 * @param severity Severity level (fatal, error, warning, note)
 * @param error_code Numeric error code for categorization
 * @param category Error category string (e.g., "directive", "macro", "expression")
 * @param suggestion Optional suggestion text for fixing the issue
 * @param format Printf-style format string for the diagnostic message
 */
void add_preprocessor_diagnostic_ex(
    BaaPreprocessor *pp_state,
    const PpSourceLocation *loc,
    PpDiagnosticSeverity severity,
    uint32_t error_code,
    const char *category,
    const wchar_t *suggestion,
    const wchar_t *format,
    ...
);

// Convenience macros for common diagnostic types
#define PP_REPORT_FATAL(pp, loc, code, cat, fmt, ...) \
    add_preprocessor_diagnostic_ex(pp, loc, PP_DIAG_FATAL, code, cat, NULL, fmt, ##__VA_ARGS__)

#define PP_REPORT_ERROR(pp, loc, code, cat, fmt, ...) \
    add_preprocessor_diagnostic_ex(pp, loc, PP_DIAG_ERROR, code, cat, NULL, fmt, ##__VA_ARGS__)

#define PP_REPORT_WARNING(pp, loc, code, cat, fmt, ...) \
    add_preprocessor_diagnostic_ex(pp, loc, PP_DIAG_WARNING, code, cat, NULL, fmt, ##__VA_ARGS__)

#define PP_REPORT_NOTE(pp, loc, code, cat, fmt, ...) \
    add_preprocessor_diagnostic_ex(pp, loc, PP_DIAG_NOTE, code, cat, NULL, fmt, ##__VA_ARGS__)

/**
 * @brief Determine appropriate recovery action for an error
 *
 * Analyzes the error context and determines the best recovery strategy
 * to continue processing while maintaining accuracy.
 *
 * @param pp_state Preprocessor state
 * @param severity Severity of the error
 * @param category Error category
 */
PpRecoveryAction determine_recovery_action(
    BaaPreprocessor *pp_state,
    PpDiagnosticSeverity severity,
    const char *category,
    const PpSourceLocation *location
);

bool execute_recovery_action(
    BaaPreprocessor *pp_state,
    PpRecoveryAction action,
    const wchar_t **current_position
);

// Error limit management
bool should_continue_processing(const BaaPreprocessor *pp_state);
bool increment_error_count(BaaPreprocessor *pp_state, PpDiagnosticSeverity severity);
bool has_reached_error_limit(const BaaPreprocessor *pp_state, PpDiagnosticSeverity severity);
void reset_recovery_state(BaaPreprocessor *pp_state, const char *new_context);

// Synchronization functions
bool sync_to_next_directive(BaaPreprocessor *pp_state, const wchar_t **line_ptr);
bool sync_to_next_line(BaaPreprocessor *pp_state, const wchar_t **line_ptr);
bool sync_expression_parsing(BaaPreprocessor *pp_state, const wchar_t **expr_ptr, wchar_t terminator);
bool recover_conditional_stack(BaaPreprocessor *pp_state);

// Enhanced error system initialization and cleanup
bool init_preprocessor_error_system(BaaPreprocessor *pp_state);
void cleanup_preprocessor_error_system(BaaPreprocessor *pp_state);
wchar_t* generate_error_summary(const BaaPreprocessor *pp_state);

void free_diagnostics_list(BaaPreprocessor *pp_state);

// File Stack
bool push_file_stack(BaaPreprocessor *pp, const char *abs_path);
void pop_file_stack(BaaPreprocessor *pp);
void free_file_stack(BaaPreprocessor *pp);

// Location Stack
bool push_location(BaaPreprocessor *pp, const PpSourceLocation *location);
void pop_location(BaaPreprocessor *pp);
PpSourceLocation get_current_original_location(const BaaPreprocessor *pp);     // Gets location from top of stack
void update_current_location(BaaPreprocessor *pp, size_t line, size_t column); // Updates location on top of stack
void free_location_stack(BaaPreprocessor *pp);

// From preprocessor_macros.c
bool add_macro(BaaPreprocessor *pp_state, const wchar_t *name, const wchar_t *body, bool is_function_like, bool is_variadic, size_t param_count, wchar_t **param_names);
const BaaMacro *find_macro(const BaaPreprocessor *pp_state, const wchar_t *name);
bool undefine_macro(BaaPreprocessor *pp_state, const wchar_t *name);
void free_macros(BaaPreprocessor *pp);

// From preprocessor_expansion.c
bool push_macro_expansion(BaaPreprocessor *pp_state, const BaaMacro *macro);
void pop_macro_expansion(BaaPreprocessor *pp_state);
bool is_macro_expanding(const BaaPreprocessor *pp_state, const BaaMacro *macro);
void free_macro_expansion_stack(BaaPreprocessor *pp_state);
wchar_t **parse_macro_arguments(BaaPreprocessor *pp_state, const wchar_t **invocation_ptr_ref, const BaaMacro *macro, size_t *actual_arg_count, wchar_t **error_message);
bool substitute_macro_body(BaaPreprocessor *pp_state, DynamicWcharBuffer *output_buffer, const BaaMacro *macro, wchar_t **arguments, size_t arg_count, wchar_t **error_message);
bool stringify_argument(BaaPreprocessor *pp_state, DynamicWcharBuffer *output_buffer, const wchar_t *argument, wchar_t **error_message);

// From preprocessor_conditionals.c
bool push_conditional(BaaPreprocessor *pp_state, bool condition_met);
bool pop_conditional(BaaPreprocessor *pp_state);
void update_skipping_state(BaaPreprocessor *pp_state);
void free_conditional_stack(BaaPreprocessor *pp);

// From preprocessor_expr_eval.c
bool evaluate_preprocessor_expression(BaaPreprocessor *pp_state, const wchar_t *expression, bool *value, wchar_t **error_message, const char *abs_path);

// From preprocessor_directives.c
bool handle_preprocessor_directive(BaaPreprocessor *pp_state, wchar_t *directive_start, const char *abs_path, DynamicWcharBuffer *output_buffer, wchar_t **error_message, bool *is_conditional_directive);

// From preprocessor_line_processing.c
// This function is now also called by the expression evaluator for full macro expansion of the expression string.
bool process_code_line_for_macros(BaaPreprocessor *pp_state, const wchar_t *current_line, size_t line_len, DynamicWcharBuffer *output_buffer, wchar_t **error_message);
bool scan_and_substitute_macros_one_pass(
    BaaPreprocessor *pp_state,
    const wchar_t *input_line_content,
    size_t original_line_number_for_errors,
    DynamicWcharBuffer *one_pass_buffer,
    bool *overall_success,
    wchar_t **error_message);
// New function for expression expansion without 'معرف' special handling
bool scan_and_expand_macros_for_expressions(
    BaaPreprocessor *pp_state,
    const wchar_t *input_line_content,
    size_t original_line_number_for_errors,
    DynamicWcharBuffer *one_pass_buffer,
    bool *overall_success,
    wchar_t **error_message);

// From preprocessor_core.c
wchar_t *process_file(BaaPreprocessor *pp_state, const char *file_path, wchar_t **error_message);
wchar_t *process_string(BaaPreprocessor *pp_state, const wchar_t *source_string, wchar_t **error_message); // New function for string input

// From preprocessor.c (internal helper)
void report_unterminated_conditional(BaaPreprocessor *st, const PpSourceLocation *loc);

#endif // BAA_PREPROCESSOR_INTERNAL_H
