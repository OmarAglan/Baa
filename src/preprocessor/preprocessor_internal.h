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
#define PP_ERROR_UNKNOWN_DIRECTIVE (PP_ERROR_DIRECTIVE_BASE + 1)
#define PP_ERROR_MISSING_ENDIF (PP_ERROR_DIRECTIVE_BASE + 2)
#define PP_ERROR_INVALID_INCLUDE (PP_ERROR_DIRECTIVE_BASE + 3)
#define PP_ERROR_MISSING_MACRO_NAME (PP_ERROR_DIRECTIVE_BASE + 4)
#define PP_ERROR_MACRO_REDEFINITION (PP_ERROR_MACRO_BASE + 1)
#define PP_ERROR_MACRO_ARG_MISMATCH (PP_ERROR_MACRO_BASE + 2)
#define PP_ERROR_DIVISION_BY_ZERO (PP_ERROR_EXPRESSION_BASE + 1)
#define PP_ERROR_UNDEFINED_IDENTIFIER (PP_ERROR_EXPRESSION_BASE + 2)
#define PP_ERROR_FILE_NOT_FOUND (PP_ERROR_FILE_BASE + 1)
#define PP_ERROR_CIRCULAR_INCLUDE (PP_ERROR_FILE_BASE + 2)
#define PP_ERROR_OUT_OF_MEMORY (PP_ERROR_MEMORY_BASE + 1)

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

// Structure to hold preprocessor state
// Define the struct fully here for internal use. The public header only has a forward declaration.
struct BaaPreprocessor
{
    const char **include_paths;
    size_t include_path_count;
    char **open_files_stack;
    size_t open_files_count;
    size_t open_files_capacity;
    BaaMacro *macros;
    size_t macro_count;
    size_t macro_capacity;
    bool *conditional_stack;
    size_t conditional_stack_count;
    size_t conditional_stack_capacity;
    bool *conditional_branch_taken_stack;
    size_t conditional_branch_taken_stack_count;
    size_t conditional_branch_taken_stack_capacity;
    bool skipping_lines;
    const BaaMacro **expanding_macros_stack;
    size_t expanding_macros_count;
    size_t expanding_macros_capacity;
    const char *current_file_path;
    size_t current_line_number;
    size_t current_column_number;     // Current physical location being processed
    PpSourceLocation *location_stack; // Stack to track original source locations
    size_t location_stack_count;
    size_t location_stack_capacity;

    // Enhanced error management system
    PreprocessorDiagnostic *diagnostics;
    size_t diagnostic_count;
    size_t diagnostic_capacity;

    // Error counting by severity
    size_t fatal_count;
    size_t error_count;
    size_t warning_count;
    size_t note_count;

    // Configuration and state
    PpErrorLimits error_limits;
    PpRecoveryState recovery_state;
    bool had_fatal_error; // Replaces had_error_this_pass
}; // Note: No typedef name here

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

// From preprocessor_utils.c
bool init_dynamic_buffer(DynamicWcharBuffer *db, size_t initial_capacity);
bool append_to_dynamic_buffer(DynamicWcharBuffer *db, const wchar_t *str_to_append);
bool append_dynamic_buffer_n(DynamicWcharBuffer *db, const wchar_t *str_to_append, size_t n); // Re-typed
void free_dynamic_buffer(DynamicWcharBuffer *db);
wchar_t *wcsndup_internal(const wchar_t *s, size_t n); // Renamed to avoid potential conflicts if wcsndup exists
// Reads file content, detecting UTF-8/UTF-16LE encoding.
wchar_t *read_file_content(BaaPreprocessor *pp_state, const char *file_path, wchar_t **error_message);
char *get_absolute_path(const char *file_path);
char *get_directory_part(const char *file_path);
// Updated error formatter to potentially accept an explicit location
wchar_t *format_preprocessor_error_at_location(const PpSourceLocation *location, const wchar_t *format, ...);
wchar_t *format_preprocessor_warning_at_location(const PpSourceLocation *location, const wchar_t *format, ...);
// Legacy function to add a diagnostic (error or warning) to the preprocessor state
void add_preprocessor_diagnostic(BaaPreprocessor *pp_state, const PpSourceLocation *loc, bool is_error, const wchar_t *format, va_list args_list);

// Enhanced diagnostic collection with full severity and categorization support
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

// Error recovery management
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
