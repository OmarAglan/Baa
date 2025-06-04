# Baa Preprocessor Documentation (توثيق المعالج المسبق)

## Overview (نظرة عامة)

The Baa preprocessor is a crucial initial stage in the Baa compilation pipeline. It operates on Baa source files **before** lexical analysis (tokenization). Its primary responsibilities include:

* Handling preprocessor directives (lines starting with `#`).
* Including the content of other files (`#تضمين`).
* Defining and expanding macros (`#تعريف`, including object-like, function-like, and variadic macros).
* Performing conditional compilation (`#إذا`, `#إذا_عرف`, `#وإلا_إذا`, etc.).
* Stripping comments.

The preprocessor takes a Baa source file (or string) as input, processes these directives, and produces a single, unified translation unit (as a wide character string, typically UTF-16LE) which is then passed to the Baa lexer.

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
  * **Expression Evaluation:** Supports arithmetic (`+`, `-`, `*`, `/`, `%`), comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), logical (`&&`, `||`, `!`), and bitwise (`&`, `|`, `^`, `~`, `<<`, `>>`) operators with standard C precedence. Also supports the `معرف(IDENTIFIER)` or `معرف IDENTIFIER` operator. Integer literals can be decimal, hexadecimal (`0x...`), or binary (`0b...`).
  * **Full Macro Expansion in Conditionals:** Function-like macros (with arguments) are now fully expanded within `#إذا` and `#وإلا_إذا` expressions before evaluation, including nested function-like macro calls and complex rescanning scenarios. The `معرف` operator arguments are correctly preserved without expansion.
* **`#خطأ "message"` (Error):** Halts preprocessing and reports the specified message as a fatal error.
* **`#تحذير "message"` (Warning):** Prints the specified message to `stderr` and preprocessing continues.

### 3. Predefined Macros (الماكروهات المدمجة)

The Baa preprocessor automatically defines the following macros:

* `__الملف__`: Expands to the current source file name (string literal).
* `__السطر__`: Expands to the current line number (integer constant).
* `__التاريخ__`: Expands to the compilation date (string literal, e.g., `"May 21 2025"`).
* `__الوقت__`: Expands to the compilation time (string literal, e.g., `"10:30:00"`).
* `__الدالة__`: Expands to a placeholder string literal `L"__BAA_FUNCTION_PLACEHOLDER__"`. (Actual function name substitution occurs in later compiler stages).
* `__إصدار_المعيار_باء__`: Expands to a long integer constant representing the Baa language version (e.g., `10170L` for v0.1.17.0).

### 4. Error Handling and Reporting (معالجة الأخطاء)

* **Comprehensive Error Recovery System:** The preprocessor now includes a robust error recovery system that allows continued processing after encountering errors, reporting multiple errors in a single compilation pass.
* **Diagnostic System:** Centralized error and warning collection with precise source location tracking (file, line, column), even through include files and macro expansions.
* **Error Recovery Utilities:** Smart synchronization mechanisms to recover from various error types:
  - **Directive Recovery:** Handles unknown directives and malformed directive syntax
  - **Expression Recovery:** Graceful handling of malformed conditional expressions with division by zero protection
  - **Include Recovery:** Attempts alternative file extensions when includes fail
  - **Conditional Stack Validation:** Automatic cleanup of unmatched conditional directives
* **Error Limits:** Configurable error thresholds (default: 25 errors per file, 50 globally) to prevent overwhelming users with error messages
* **Continued Processing:** After recoverable errors, the preprocessor continues processing valid code sections, providing more comprehensive error reporting
* **Arabic Error Messages:** All error messages are provided in Arabic with consistent formatting and precise location information
* **Enhanced Robustness:** Prevents crashes from malformed input and ensures proper memory management during error conditions

### 5. Error Recovery Features (ميزات استرداد الأخطاء)

* **Multiple Error Reporting:** Users can see and fix multiple errors at once instead of iterative single-error fixing
* **Smart Synchronization:** Uses preprocessor directives as natural synchronization points for recovery
* **Graceful Degradation:** When errors occur, valid code sections are still processed and included in output where possible
* **Memory Safety:** Ensures no memory leaks during error recovery with proper cleanup of partially processed content
* **IDE Integration Ready:** Provides comprehensive diagnostic information for better language server implementation

## Preprocessor Structure (بنية المعالج المسبق)

The preprocessor is implemented as a modular component within the `src/preprocessor/` directory, with functionalities split into several files (e.g., `preprocessor_directives.c`, `preprocessor_macros.c`, `preprocessor_expansion.c`, `preprocessor_expr_eval.c`). It uses an internal header `preprocessor_internal.h` for shared definitions. The public API is defined in `include/baa/preprocessor/preprocessor.h`.

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

## Current Known Issues and Limitations

* **Zero-Parameter Function-Like Macro Bug:** Zero-parameter function-like macros (e.g., `GET_BASE()`) may expand incorrectly to `()` instead of their macro body in conditional expressions. This affects expressions like `#إذا IS_EQUAL(GET_BASE(), 42)` where `GET_BASE()` should expand to its defined value.
* **Ternary Operator Support (`? :`):** The expression evaluator does not yet support ternary conditional expressions using `condition ? true_value : false_value` syntax in `#إذا`/`#وإلا_إذا` expressions.
* **Full Error Recovery:** While the foundation for accumulating multiple errors is in place, most error-handling sites in the preprocessor still need to be updated to fully utilize this by attempting synchronization and continuation instead of immediately halting.
* **Token Pasting (`##`) during Rescanning (Complex Cases):** Complex interactions of `##` when it appears as part of a macro expansion output, or when its operands are complex macros, may not be fully robust.
* **Error/Warning Location Precision:** While significantly improved, further refinement for precise column reporting in all error scenarios is an ongoing effort.

*For detailed ongoing tasks and future plans, please refer to `docs/PREPROCESSOR_ROADMAP.md`.*
