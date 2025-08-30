# Baa Preprocessor Documentation (توثيق المعالج المسبق)

## Overview (نظرة عامة)

The Baa preprocessor is a crucial initial stage in the Baa compilation pipeline. It operates on Baa source files **before** lexical analysis (tokenization). Its primary responsibilities include:

* Handling preprocessor directives (lines starting with `#`).
* Including the content of other files (`#تضمين`).
* Defining and expanding macros (`#تعريف`, including object-like, function-like, and variadic macros).
* Performing conditional compilation (`#إذا`, `#إذا_عرف`, `#وإلا_إذا`, etc.).
* Stripping comments.

The preprocessor takes a Baa source file (or string) as input, processes these directives, and produces a single, unified translation unit (as a wide character string, typically UTF-16LE) which is then passed to the Baa lexer.

**Current Version**: v0.1.31.0 (98% complete with C99-compliant preprocessor features including line control, pragma directives, and _Pragma operator)

## Features (الميزات)

### 1. File Handling and Encoding

* **Input Source Abstraction:** Can process input from files (`BAA_PP_SOURCE_FILE`) or directly from wide character strings (`BAA_PP_SOURCE_STRING`) via the `BaaPpSource` struct.
* **File Encoding Detection:** Automatically detects UTF-8 (with or without BOM) and UTF-16LE encodings for input files. Defaults to UTF-8 if no BOM is found. UTF-16BE is not currently supported.
* **Internal Representation:** Works with wide characters (`wchar_t`) internally.
* **Output:** Produces a UTF-16LE `wchar_t*` string.

### 2. Directive Handling (معالجة التوجيهات)

* **`#تضمين` (Include):**
  * `#تضمين "مسار/نسبي.ب"`: Includes files using paths relative to the current file.
  * `#تضمين <مكتبة_قياسية>`: Includes files from specified standard include paths.
  * Detects and reports errors for circular includes.
* **`#تعريف` (Define):**
  * **Object-like Macros:** e.g., `#تعريف PI 3.14159`
  * **Function-like Macros:** e.g., `#تعريف MAX(a, b) ((a) > (b) ? (a) : (b))`
    * Handles parameter substitution.
    * Supports the stringification operator (`#`): `#تعريف STR(x) #x` expands `STR(abc)` to `"abc"`.
    * Supports the token-pasting operator (`##`): `#تعريف CONCAT(x, y) x##y` expands `CONCAT(var, 1)` to `var1`.
    * **Variadic Macros (C99 Style):** Uses `وسائط_إضافية` for `...` in definition and `__وسائط_متغيرة__` for `__VA_ARGS__` in the body.
            Example: `#تعريف LOG(fmt, وسائط_إضافية) printf(fmt, __وسائط_متغيرة__)`
  * **Rescanning:** The output of macro expansions (after argument substitution, `#`, and `##`) is rescanned for further macro names to be expanded, adhering to C99 standards.
  * Detects and prevents direct self-recursive macro expansion.
  * **C99-Compliant Macro Redefinition Checking:**
    * **Identical Redefinitions:** Silent acceptance of identical macro redefinitions as per C99 standard (ISO/IEC 9899:1999 section 6.10.3).
    * **Incompatible Redefinitions:** Warning messages in Arabic when attempting to redefine a macro with different replacement text or parameter signature.
    * **Predefined Macro Protection:** Error reporting for attempts to redefine built-in macros (`__الملف__`, `__السطر__`, etc.) with rejection of redefinition.
    * **Intelligent Comparison:** Uses whitespace normalization and parameter signature matching to determine macro equivalence according to C99 rules.
* **`#الغاء_تعريف` (Undefine):** Removes a macro definition (e.g., `#الغاء_تعريف PI`).
* **Conditional Compilation:**
  * `#إذا expression`: Evaluates a constant integer expression. Undefined identifiers are treated as 0.
  * `#إذا_عرف MACRO`: True if `MACRO` is defined. (Equivalent to `#إذا معرف(MACRO)`)
  * `#إذا_لم_يعرف MACRO`: True if `MACRO` is not defined. (Equivalent to `#إذا !معرف(MACRO)`)
  * `#وإلا_إذا expression`: Else-if branch.
  * `#إلا`: Else branch.
  * `#نهاية_إذا`: Ends a conditional block.
  * **Expression Evaluation:** Supports arithmetic (`+`, `-`, `*`, `/`, `%`), comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), logical (`&&`, `||`, `!`), bitwise (`&`, `|`, `^`, `~`, `<<`, `>>`), and ternary conditional (`? :`) operators with standard C precedence. Also supports the `معرف(IDENTIFIER)` or `معرف IDENTIFIER` operator. Integer literals can be decimal, hexadecimal (`0x...`), or binary (`0b...`).
  * **Full Macro Expansion in Conditionals:** Function-like macros (with arguments) are now fully expanded within `#إذا` and `#وإلا_إذا` expressions before evaluation, including nested function-like macro calls and complex rescanning scenarios. The `معرف` operator arguments are correctly preserved without expansion.
* **`#سطر` (Line Control):**
  * `#سطر 100`: Sets the line number for the next line to 100.
  * `#سطر 50 "custom_file.baa"`: Sets both line number and filename for error reporting.
  * Affects `__السطر__` macro expansion and error location reporting.
  * Supports macro expansion in arguments.
* **`#براغما` (Pragma):**
  * `#براغما مرة_واحدة`: Prevents multiple inclusion of the same file (pragma once).
  * `#براغما unknown_directive`: Unknown pragma directives are silently ignored as per C99 standard.
  * Empty pragma directives are allowed and ignored.
* **`#خطأ "message"` (Error):** Halts preprocessing and reports the specified message as a fatal error.
* **`#تحذير "message"` (Warning):** Prints the specified message to `stderr` and preprocessing continues.

### 3. Predefined Macros (الماكروهات المدمجة)

The Baa preprocessor automatically defines the following macros:

* `__الملف__`: Expands to the current source file name (string literal). Can be overridden by `#سطر` directive.
* `__السطر__`: Expands to the current line number (integer constant). Affected by `#سطر` directive overrides.
* `__التاريخ__`: Expands to the compilation date (string literal, e.g., `"May 21 2025"`).
* `__الوقت__`: Expands to the compilation time (string literal, e.g., `"10:30:00"`).
* `__الدالة__`: Expands to a placeholder string literal `"__BAA_FUNCTION_PLACEHOLDER__"`. (Actual function name substitution occurs in later compiler stages).
* `__إصدار_المعيار_باء__`: Expands to a long integer constant representing the Baa language version (currently `10150L` for v0.1.15.0).

### 5. Preprocessor Operators (مشغلات المعالج المسبق)

* **`أمر_براغما("string")` (_Pragma Operator):**
  * Converts a string literal into a pragma directive during macro expansion.
  * Also supports the shorter form `براغما("string")`.
  * Example: `أمر_براغما("مرة_واحدة")` is equivalent to `#براغما مرة_واحدة`.
  * Supports escape sequences within the string literal (`\n`, `\t`, `\"`, `\\`, etc.).
  * Can be used within macros to generate pragma directives dynamically.

### 4. Error Handling and Reporting (معالجة الأخطاء)

* **Comprehensive Error Collection**: Reports errors and warnings with precise location information (original file, line, and column), even through include files and macro expansions.
* **Enhanced Error Recovery**: The preprocessor implements an advanced error collection system that accumulates multiple diagnostics (errors/warnings) in a single pass instead of halting on the first error. This allows for more comprehensive error reporting and better development workflow.
* **Severity Levels**: Supports multiple diagnostic severity levels:
  * **Fatal Errors** (`PP_DIAG_FATAL`): Critical system errors that halt processing immediately (e.g., memory allocation failures)
  * **Errors** (`PP_DIAG_ERROR`): Syntax and semantic errors that allow recovery and continued processing
  * **Warnings** (`PP_DIAG_WARNING`): Non-critical issues that don't prevent successful preprocessing
  * **Notes** (`PP_DIAG_NOTE`): Informational messages and debugging information
* **Error Categories**: Errors are categorized for better organization and recovery strategies:
  * **Directive errors** (1000-1999 range): Preprocessor directive parsing issues
  * **Macro errors** (2000-2999 range): Macro definition and expansion problems
  * **Expression errors** (3000-3999 range): Conditional expression evaluation failures
  * **File errors** (4000-4999 range): File I/O and include-related issues
  * **Memory errors** (5000-5999 range): Memory management failures
  * **Syntax errors** (6000-6999 range): General syntax problems
* **Configurable Error Limits**: Supports configurable thresholds to prevent error flooding:
  * Maximum errors before stopping (default: 100)
  * Maximum warnings before stopping (default: 1000)
  * Cascading error prevention to avoid repetitive error reports
* **Smart Recovery Strategies**: Context-aware error recovery that continues processing when possible:
  * **Directive-level synchronization**: Skip invalid directives and continue at next line
  * **Expression-level recovery**: Continue evaluation after encountering invalid operators or operands
  * **Conditional stack validation**: Automatic cleanup for unmatched `#إذا`/`#نهاية_إذا` pairs
  * **Line-boundary synchronization**: Ensure errors in one line don't corrupt subsequent line processing
  * **File-level recovery**: Skip failed include files and continue processing
  * **Macro expansion recovery**: Handle argument count mismatches and invalid parameter names
* **Recovery Functions**: Implemented recovery functions include:
  * `sync_to_next_directive()`: Synchronize to next preprocessor directive
  * `sync_to_next_line()`: Synchronize to next line boundary
  * `sync_expression_parsing()`: Recover from expression parsing errors
  * `recover_conditional_stack()`: Validate and repair conditional compilation stack
* **Arabic Language Support**: Error messages are primarily in Arabic with proper formatting and cultural considerations.

## Preprocessor Structure (بنية المعالج المسبق)

The preprocessor is implemented as a modular component within the `src/preprocessor/` directory, with functionalities split into several files:

* **`preprocessor.c`**: Main entry point and initialization, defines `baa_preprocess()` function
* **`preprocessor_core.c`**: Core file and string processing functions (`process_file()`, `process_string()`)
* **`preprocessor_directives.c`**: Handles all preprocessor directives (`#تضمين`, `#تعريف`, `#إذا`, etc.)
* **`preprocessor_macros.c`**: Macro definition, lookup, and management functions
* **`preprocessor_expansion.c`**: Macro expansion, argument parsing, and substitution logic
* **`preprocessor_conditionals.c`**: Conditional compilation stack management
* **`preprocessor_expr_eval.c`**: Expression evaluation for conditional directives
* **`preprocessor_line_processing.c`**: Line-by-line macro scanning and substitution
* **`preprocessor_utils.c`**: Utility functions for error handling, location tracking, and file operations
* **`preprocessor_internal.h`**: Internal header with shared definitions and function declarations

The public API is defined in `include/baa/preprocessor/preprocessor.h` and consists primarily of the `baa_preprocess()` function and supporting data structures.

## Internal API Functions (واجهة برمجة التطبيقات الداخلية)

The preprocessor provides several internal API functions for advanced error handling and state management:

### Error System Functions

* **`init_preprocessor_error_system(BaaPreprocessor *pp_state)`**: Initializes the error collection system
* **`cleanup_preprocessor_error_system(BaaPreprocessor *pp_state)`**: Cleans up error system resources
* **`generate_error_summary(const BaaPreprocessor *pp_state)`**: Generates a summary of all collected errors
* **`add_preprocessor_diagnostic_ex()`**: Adds a diagnostic message with full context information

### Location Tracking Functions

* **`push_location(BaaPreprocessor *pp, const PpSourceLocation *location)`**: Pushes a location onto the location stack
* **`pop_location(BaaPreprocessor *pp)`**: Pops a location from the location stack
* **`get_current_original_location(const BaaPreprocessor *pp)`**: Gets the current location from the stack
* **`update_current_location(BaaPreprocessor *pp, size_t line, size_t column)`**: Updates the current location

### File Stack Management

* **`push_file_stack(BaaPreprocessor *pp, const char *abs_path)`**: Pushes a file onto the include stack
* **`pop_file_stack(BaaPreprocessor *pp)`**: Pops a file from the include stack
* **`free_file_stack(BaaPreprocessor *pp)`**: Frees the file stack resources

### Recovery and Synchronization Functions

* **`determine_recovery_action()`**: Determines the appropriate recovery action for an error
* **`execute_recovery_action()`**: Executes a recovery action
* **`should_continue_processing(const BaaPreprocessor *pp_state)`**: Checks if processing should continue
* **`sync_to_next_directive(BaaPreprocessor *pp_state, const wchar_t **line_ptr)`**: Synchronizes to the next directive
* **`sync_to_next_line(BaaPreprocessor *pp_state, const wchar_t **line_ptr)`**: Synchronizes to the next line
* **`sync_expression_parsing(BaaPreprocessor *pp_state, const wchar_t **expr_ptr, wchar_t terminator)`**: Recovers from expression parsing errors

## Usage (الاستخدام)

The preprocessor is invoked via the `baa_preprocess` function:

```c
#include "baa/preprocessor/preprocessor.h" // Public API
#include <wchar.h>

// ...
const char* file_to_process = "program.ب";
wchar_t* error_msg = NULL;
const char* include_dirs[] = {"./includes", "/usr/local/baa/include", NULL};

BaaPpSource source_input;
source_input.type = BAA_PP_SOURCE_FILE;
source_input.source_name = file_to_process; // For error messages
source_input.data.file_path = file_to_process;

wchar_t* processed_code = baa_preprocess(&source_input, include_dirs, &error_msg);

if (processed_code) {
    // Use processed_code for lexing...
    // fwprintf(stdout, L"%ls", processed_code);
    free(processed_code);
} else {
    if (error_msg) {
        fwprintf(stderr, L"Preprocessor Error:\n%ls\n", error_msg);
        free(error_msg);
    } else {
        fwprintf(stderr, L"Unknown preprocessor error for %hs.\n", file_to_process);
    }
}
// ...
```

### Example: Ternary Operator in Preprocessor Conditionals

You can now use the ternary operator (`? :`) in preprocessor conditional expressions:

```baa
#تعريف USE_FAST 1
#إذا USE_FAST ? 100 : 200
    // This block is included because USE_FAST is 1 (true), so 100 is the result (nonzero)
#إلا
    // This block would be included if USE_FAST was 0 (false), so 200 would be the result (nonzero)
#نهاية_إذا

#إذا 0 ? 1 : 0
    // This block is NOT included (result is 0)
#إلا
    // This block IS included (result is nonzero)
#نهاية_إذا
```

## Error Recovery Examples

### Directive Error Recovery
```baa
#تعريف VALID_MACRO 42
#unknown_directive invalid syntax    // Error reported, directive skipped
#تعريف ANOTHER_VALID 123            // Processing continues
```

### Macro Error Recovery
```baa
#تعريف FUNC(a, b) a + b
FUNC(1)                             // Error: wrong argument count, expansion skipped
int x = VALID_MACRO;                // Processing continues with valid macros
```

### Expression Error Recovery
```baa
#إذا UNDEFINED_MACRO / 0 > 1        // Warning: undefined treated as 0, division by zero handled
    int valid_code = 42;            // Block processing continues
#نهاية_إذا
```

### File Error Recovery
```baa
#تضمين "nonexistent.h"             // Error reported, include skipped
#تعريف AFTER_ERROR 123             // Processing continues
```

## Error Message Format

Error messages follow a consistent Arabic format with location information:

```
program.ب:15:8: خطأ: تنسيق #تعريف غير صالح: اسم الماكرو مفقود
program.ب:20:12: تحذير: إعادة تعريف الماكرو 'PI'
```

The format is: `filename:line:column: severity: message`

## Examples (أمثلة)

### Line Control Directive
```baa
#سطر 100
int x = 42;        // This line is reported as line 100
int y = __السطر__; // y gets the value 101

#سطر 50 "math.baa"
float pi = 3.14;   // This appears to be from "math.baa" at line 50
```

### Pragma Directives
```baa
// Header file: utils.baa
#براغما مرة_واحدة
// This file will only be included once, even if #تضمين appears multiple times

#define UTILITY_VERSION 1

// Using _Pragma operator in macro
#تعريف ONCE_MACRO أمر_براغما("مرة_واحدة")
ONCE_MACRO  // Equivalent to #براغما مرة_واحدة

// Short form
براغما("unknown_directive")  // Silently ignored
```

### Combined Usage
```baa
#تعريف HEADER_GUARD(name) \
    #سطر 1 name ".baa" \
    أمر_براغما("مرة_واحدة")

HEADER_GUARD("config")
// Sets line to 1, filename to "config.baa", and enables pragma once
```

### Error Categories and Codes

The preprocessor uses a systematic error code system:

* **1000-1999**: Directive errors (unknown directives, malformed syntax)
* **2000-2999**: Macro errors (redefinition, expansion failures, argument mismatches)
* **3000-3999**: Expression errors (invalid operators, division by zero, undefined identifiers)
* **4000-4999**: File errors (file not found, circular includes, encoding issues)
* **5000-5999**: Memory errors (allocation failures, stack overflow)
* **6000-6999**: Syntax errors (unterminated strings, invalid characters)

## Current Known Issues and Limitations

* **Enhanced Error System Migration**: While the foundation for comprehensive error collection is in place, migration of all error-handling sites to the new system is ongoing. Some error sites may still halt processing immediately instead of attempting recovery.
* **Complex Token Pasting (`##`) during Rescanning**: While the `##` operator works correctly in direct macro bodies, complex interactions when `##` appears as part of a macro expansion output that is then rescanned, or when its operands are themselves complex macros, may not be fully robust. This requires careful review of the rescan loop and how it forms new tokens after pasting.
* **Error/Warning Location Precision**: While significantly improved, further refinement for precise column reporting in all error scenarios is an ongoing effort.
* **Performance Optimization**: The enhanced error system is designed to have minimal overhead during error-free processing, but performance optimization is still being refined.
* **Unimplemented Directives**: The following standard directives are planned but not yet implemented:
  * `#سطر` (line number control)
  * `#براغما` (pragma directives)
  * `أمر_براغما` (_Pragma operator)

## Performance Characteristics

The preprocessor is designed with the following performance targets:

* **Error-Free Processing**: < 5% overhead compared to basic implementation
* **Error Collection**: < 10% overhead with up to 10 errors
* **Memory Usage**: < 100KB additional memory for 100 collected errors
* **Error Formatting**: < 1ms per error message generation

## Thread Safety

The current preprocessor implementation is **not thread-safe**. Each preprocessing operation should use a separate `BaaPreprocessor` instance. The preprocessor maintains internal state that would be corrupted by concurrent access.

*For detailed ongoing tasks and future plans, please refer to `docs/PREPROCESSOR_ROADMAP.md`.*
