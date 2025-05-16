# Baa Preprocessor Documentation (توثيق المعالج المسبق)

## Overview (نظرة عامة)

The Baa preprocessor is a crucial initial stage in the compilation pipeline. It runs **before** the lexer and is responsible for handling preprocessor directives within the Baa source code. Its primary function is to process the source text, resolving includes and expanding macros, to produce a single, unified translation unit (as a wide character string) that is then passed to the lexer.

It primarily processes source files, automatically detecting UTF-8 (with or without BOM) and UTF-16LE encodings, defaulting to UTF-8 if no BOM is found. See the "File Encoding Support" section for more details.

## Features (الميزات)

### Directive Handling (معالجة التوجيهات)

- **`#تضمين` (Include):**
  - Processes file inclusion directives.
  - Supports standard include paths using angle brackets (`#تضمين <ملف.ب>`). Searches paths provided during initialization.
  - Supports relative include paths using double quotes (`#تضمين "ملف.ب"`). Paths are resolved relative to the directory of the file containing the directive.
- **`#تعريف` (Define):**
  - **Object-like Macros:** Defines simple text replacement macros.
    - Example: `#تعريف MAX_SIZE 1024`
  - **Function-like Macros:** Defines macros that take arguments.
    - Example: `#تعريف ADD(a, b) a + b`
    - Parses parameter names within the parentheses.
    - Performs substitution of parameter names in the macro body with the provided arguments during invocation.
    - **Rescanning:** After parameter substitution and processing of `#` and `##` operators, the resulting token sequence is rescanned for further macro names to be replaced. This process repeats until no more expansions can be performed on the line.
    - Argument parsing correctly handles nested parentheses and basic string/character literals.
    - **Stringification (`#`):** Supports the `#` operator before a parameter name (e.g., `#param`) to convert the corresponding argument into a string literal (quoted and escaped).
    - **Token Pasting (`##`):** Supports the `##` operator for concatenating tokens during macro expansion. It pastes the token preceding `##` with the token following `##`. Handles edge cases like empty arguments.
- **`#الغاء_تعريف` (Undefine):**
  - Removes a previously defined macro (both object-like and function-like).
  - Example: `#الغاء_تعريف MAX_SIZE`
- **Conditional Compilation:**
  - Supports conditional blocks:
    - `#إذا expression` (if): Evaluates a constant integer `expression`. Undefined identifiers in the expression evaluate to `0`. Defined object-like macros containing simple integer literals are also evaluated correctly.
    - `#إذا_عرف MACRO` (ifdef): Checks if `MACRO` is defined.
    - `#إذا_لم_يعرف MACRO` (ifndef): Checks if `MACRO` is *not* defined.
    - `#وإلا_إذا expression` (elif): Evaluates a constant integer `expression` if the preceding `#if`/`#elif` was false and no prior branch in the block was taken.
    - `#إلا` (else): Executes if the preceding `#if`/`#elif` was false and no prior branch in the block was taken.
    - `#نهاية_إذا` (endif): Ends the conditional block.
  - Lines within blocks whose conditions are false are skipped.
  - **Expression Evaluation:** Supports standard integer arithmetic (`+`, `-`, `*`, `/`, `%`), comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), logical (`&&`, `||`, `!`), bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`), parentheses `()`, and the `defined(MACRO)` operator within `#إذا` and `#وإلا_إذا` directives, respecting operator precedence. Undefined identifiers evaluate to `0`. Defined object-like macros containing simple integer literals are also evaluated correctly.

### Macro Recursion Detection (كشف استدعاء الماكرو الذاتي)

- Detects and prevents infinite loops caused by direct or indirect recursive macro expansions using an internal expansion stack.

### Circular Include Detection (كشف التضمين الدائري)

- Tracks the stack of currently open files using their absolute paths.
- Prevents infinite loops by detecting and reporting circular `#تضمين` directives.

### File Encoding Support (دعم ترميز الملفات)

- Reads source files and automatically detects the encoding based on the Byte Order Mark (BOM):
  - **UTF-16LE:** Recognizes the `FF FE` BOM.
  - **UTF-8:** Recognizes the `EF BB BF` BOM.
- If no BOM is present, the file is assumed to be **UTF-8**.
- Files encoded in UTF-16BE are currently **not** supported.
- Internally, the preprocessor works with wide characters (`wchar_t`). UTF-8 input is converted to this internal representation.
- Outputs the processed source code as a UTF-16 `wchar_t*` string, suitable for the lexer.

### Predefined Macros (الماكروهات المدمجة المعرفة مسبقًا)

The Baa preprocessor defines the following macros automatically. These names are inspired by standard C predefined macros but are in Arabic.

- **`__الملف__` (`__FILE__`):** Expands to a string literal representing the name of the current input file being processed.
- **`__السطر__` (`__LINE__`):** Expands to an integer constant representing the current line number in the input file.
- **`__التاريخ__` (`__DATE__`):** Expands to a string literal representing the date of preprocessing, in the format `"Mmm dd yyyy"` (e.g., `"May 06 2025"`). The month names are in English as per common C standard behavior.
- **`__الوقت__` (`__TIME__`):** Expands to a string literal representing the time of preprocessing, in the format `"HH:MM:SS"` (e.g., `"19:32:14"`).
- **`__الدالة__` (`__func__` equivalent):** Expands to the string literal `L"__BAA_FUNCTION_PLACEHOLDER__"`. The preprocessor provides this placeholder; actual function name substitution would occur in later compiler stages.
- **`__إصدار_المعيار_باء__` (Baa Standard Version):** Expands to the long integer constant `10010L` (representing Baa version 0.1.10 as an example).

### Error Handling (معالجة الأخطاء)

- Reports errors via an output parameter (`wchar_t** error_message`) in the main `baa_preprocess` function.
- Detects and reports errors such as:
  - File not found (for main file or includes).
  - Invalid file encoding (not UTF-16LE or BOM mismatch).
  - Circular includes.
  - Invalid directive syntax (e.g., malformed `#تضمين` or `#تعريف`).
  - Macro errors (recursion, argument count mismatch, invalid ## usage).
  - Conditional expression evaluation errors (syntax errors, division by zero).
  - Memory allocation failures.
- Error messages are provided in Arabic and consistently include the original file path, line number, and column number where the error originated, even through includes and macro expansions (e.g., `original_file:line:col: خطأ: ...`). This is achieved using an internal location stack and a unified error formatting function.

## Preprocessor Structure (بنية المعالج المسبق)

The preprocessor implementation has been refactored into several internal source files located in `src/preprocessor/`:

- `preprocessor.c`: Main entry point (`baa_preprocess`).
- `preprocessor_core.c`: Core recursive file/string processing logic (`process_file`, `process_string`). Delegates line-by-line processing.
- `preprocessor_directives.c`: Handles all preprocessor directive logic (e.g., `#تعريف`, `#إذا`, `#تضمين`). Called by `preprocessor_core.c` for lines starting with `#`.
- `preprocessor_line_processing.c`: Handles macro substitution and processing for non-directive code lines. Called by `preprocessor_core.c`.
- `preprocessor_utils.c`: Utility functions (file I/O, path handling, error formatting, dynamic buffers, file stack).
- `preprocessor_macros.c`: Macro definition management (`add_macro`, `find_macro`, `undefine_macro`).
- `preprocessor_expansion.c`: Macro expansion logic (substitution, argument parsing, `#`, `##`, recursion stack).
- `preprocessor_conditionals.c`: Conditional compilation stack management (`#if`, `#ifdef`, etc.).
- `preprocessor_expr_eval.c`: Conditional expression evaluation logic.

An internal header, `src/preprocessor/preprocessor_internal.h`, defines shared structures and function prototypes used across these files.

### BaaPreprocessor (Public Interface)

The public header (`include/baa/preprocessor/preprocessor.h`) uses an **opaque pointer** pattern. Users interact with `BaaPreprocessor*` but the internal structure is hidden.

```c
// Forward declaration in public header
typedef struct BaaPreprocessor BaaPreprocessor;
```

The full definition of `struct BaaPreprocessor` resides in `src/preprocessor/preprocessor_internal.h` and contains fields for managing include paths, macro definitions, conditional state, file stacks, expansion stacks, and error context.

### BaaMacro

Represents a single macro definition:

```c
typedef struct {
    wchar_t* name;          // Macro name
    wchar_t* body;          // Macro body (replacement text)
    bool is_function_like;  // True if defined with (...)
    size_t param_count;     // Number of parameters
    wchar_t** param_names;  // Array of parameter names (NULL if not function-like)
} BaaMacro;
```

## Usage (الاستخدام)

The preprocessor is typically invoked by the main compiler driver before the lexer stage.

```c
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h> // For wchar_t

// ... inside compiler logic ...

wchar_t* error_msg = NULL;
const char* std_includes[] = {"/usr/local/include/baa", NULL}; // Example include paths
wchar_t* processed_source = NULL;

// --- Example: Preprocessing a File ---
const char* main_file_path = "path/to/your/program.ب";
BaaPpSource file_source = {
    .type = BAA_PP_SOURCE_FILE,
    .source_name = main_file_path, // Use filename for error messages
    .data.file_path = main_file_path
};
processed_source = baa_preprocess(&file_source, std_includes, &error_msg);

if (!processed_source) {
    // Handle file preprocessing error
    if (error_msg) {
        fwprintf(stderr, L"Preprocessor Error (File: %hs): %ls\n", main_file_path, error_msg);
        free(error_msg);
        error_msg = NULL; // Reset error message
    }
    // Abort or handle error...
} else {
    // Use processed_source from file...
    // baa_init_lexer(..., processed_source, ...);
    free(processed_source); // Free the result when done
    processed_source = NULL;
}


// --- Example: Preprocessing a String ---
const wchar_t* source_code_string = L"#تعريف GREETING L\"مرحبا\"\nمتغير تحية = GREETING L\" يا عالم!\";";
const char* string_source_name = "<input_string>"; // Name for error messages
BaaPpSource string_source = {
    .type = BAA_PP_SOURCE_STRING,
    .source_name = string_source_name,
    .data.source_string = source_code_string
};
// Note: Includes are generally not useful when preprocessing a string directly
processed_source = baa_preprocess(&string_source, NULL, &error_msg);

if (!processed_source) {
    // Handle string preprocessing error
        fwprintf(stderr, L"Preprocessor Error (String: %hs): %ls\n", string_source_name, error_msg);
        free(error_msg);
        error_msg = NULL;
    }
    // Abort or handle error...
} else {
    // Use processed_source from string...
    // baa_init_lexer(..., processed_source, ...);
    free(processed_source); // Free the result when done
    processed_source = NULL;
}

```

## Future Enhancements (تحسينات مستقبلية)

- **Input Abstraction:** The preprocessor currently supports input from files and strings via `BaaPpSource`. Future work could extend this to include standard input (`stdin`).
- **Error Recovery:** Improve error recovery mechanisms within directives and expressions.
- **Macro Expansion in Conditional Expressions**: Enhance the expression evaluator for `#إذا` and `#وإلا_إذا` to support full macro expansion (including function-like macros and rescanning of results) before evaluation.
- **Multi-line Comment Handling**: Implement robust handling for multi-line comments (`/* ... */`) across all preprocessor stages.
- **Macro Redefinition Warnings/Errors**: Implement checks for macro redefinitions.
- **Implement Remaining C99 Preprocessor Features**: Such as `#error` (`#خطأ`), `#warning` (`#تحذير`), `#line` (`#سطر`), `_Pragma` (`أمر_براغما`), and `#pragma` (`#براغما`). (`__الدالة__` placeholder and `__إصدار_المعيار_باء__` are now implemented).
