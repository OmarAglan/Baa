#ifndef BAA_PREPROCESSOR_INTERNAL_H
#define BAA_PREPROCESSOR_INTERNAL_H

#include "baa/preprocessor/preprocessor.h" // Include public header (for BaaMacro, baa_preprocess signature)
#include "baa/utils/utils.h"               // For BaaBool, etc. if needed, or maybe include specific headers
#include <wchar.h>
#include <stdbool.h>
#include <stddef.h>
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

// Structure to store a single error/warning entry
typedef struct
{
    wchar_t *message;          // The formatted error/warning message
    PpSourceLocation location; // The original source location of the error/warning
    // bool is_warning;      // Could add this later to distinguish warnings
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

    // For accumulating multiple errors/diagnostics
    PreprocessorDiagnostic *diagnostics;
    size_t diagnostic_count;
    size_t diagnostic_capacity;
    bool had_error_this_pass; // True if any error (not warning) was reported
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
// New function to add a diagnostic (error or warning) to the preprocessor state
void add_preprocessor_diagnostic(BaaPreprocessor *pp_state, const PpSourceLocation *loc, bool is_error, const wchar_t *format, va_list args_list);
void free_diagnostics_list(BaaPreprocessor *pp_state);

// File Stack
bool push_file_stack(BaaPreprocessor *pp, const char *abs_path);
void pop_file_stack(BaaPreprocessor *pp);
void free_file_stack(BaaPreprocessor *pp);

// Location Stack
bool push_location(BaaPreprocessor *pp, const PpSourceLocation *location);
void pop_location(BaaPreprocessor *pp);
PpSourceLocation get_current_original_location(const BaaPreprocessor *pp); // Gets location from top of stack
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
void report_unterminated_conditional(BaaPreprocessor * st, const PpSourceLocation *loc);

#endif // BAA_PREPROCESSOR_INTERNAL_H
