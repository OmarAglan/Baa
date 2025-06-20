# Baa Language Preprocessor Roadmap

This roadmap outlines the planned improvements and current status of the Baa language preprocessor implementation.

## Core Preprocessor Functionality

* [x] Separate processing stage before lexer
* [x] Recursive file processing
* [x] File encoding detection (UTF-8 with/without BOM, UTF-16LE default if no BOM)
* [x] Dynamic buffer for output generation
* [x] Error reporting via output parameter (now collects multiple diagnostics)
* [x] Input source abstraction (file, string). ([ ] Add stdin support)
* [x] Support for UTF-8 input files (with conversion to internal `wchar_t`)
* [x] Improved error reporting (unified system with original file, line, and column numbers, including through expansions)
* [x] Foundation for Error Recovery (accumulating multiple diagnostics instead of halting on first). (v0.1.17.0)

## Directive Handling

* **Includes:**
  * [x] `#تضمين "..."` (Relative path resolution)
  * [x] `#تضمين <...>` (Include path searching)
  * [x] Circular include detection
* **Macros:**
  * [x] `#تعريف NAME BODY` (Parameterless macro definition)
  * [x] Simple text substitution for parameterless macros
  * [x] `#الغاء_تعريف NAME` (undef)
  * [x] Function-like macros (with parameters)
  * [x] Stringification (`#`)
  * [x] Token Pasting (`##`) (concatenation)
  * [x] Macro recursion detection (via expansion stack)
  * [x] **C99 Support**: Variadic Macros (using `وسائط_إضافية` for `...` and `__وسائط_متغيرة__` for `__VA_ARGS__`). (v0.1.13.0)
  * [ ] Fully robust argument parsing (complex edge cases with literals/whitespace, especially within nested calls or tricky quote/comment scenarios).
* **Conditional Compilation:**
  * [x] `#إذا_عرف MACRO` (ifdef - checks if macro is defined, i.e., `معرف MACRO`)
  * [x] `#إذا_لم_يعرف MACRO` (ifndef - checks if macro is not defined, i.e., `!معرف MACRO`)
  * [x] `#إذا expression` (if - constant integer expression evaluation, uses `معرف` operator)
  * [x] `#وإلا_إذا expression` (elif - constant integer expression evaluation)
  * [x] `#إلا` (else)
  * [x] `#نهاية_إذا` (endif)
  * [x] Support for bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) in conditional expressions. (v0.1.12.0)
  * [x] Support for decimal, hexadecimal (`0x`), and binary (`0b`) integer literals in conditional expressions. (v0.1.15.0)
  * [x] **Ternary Operator Support (`? :`)**: Support for conditional expressions using the ternary operator `condition ? true_value : false_value` in preprocessor conditional expressions. (v0.1.28.0)
  * [x] **Zero-Parameter Function-Like Macro Bug**: Fix issue where zero-parameter function-like macros (e.g., `GET_BASE()`) expand incorrectly to `()` instead of their macro body. Fixed in `parse_macro_arguments()` function by properly allocating empty arguments array for zero-parameter cases. (v0.1.26.0)
  * [x] **Line Number Reporting Bug**: Fix critical issue where all preprocessor errors/warnings incorrectly reported `line 1:1` instead of actual line numbers. Fixed by adding `update_current_location()` function and enhancing fallback logic in `get_current_original_location()` to properly maintain location stack during file processing. (v0.1.27.0)
* **Other Standard Directives:**
  * [x] `#خطأ "message"` (Baa: `#خطأ "رسالة الخطأ"`) - Implemented (v0.1.15.0)
  * [x] `#تحذير "message"` (Baa: `#تحذير "رسالة التحذير"`) - Implemented (v0.1.15.0)
  * [ ] `#سطر رقم "اسم_الملف"` (Baa: `#سطر ١٠٠ "ملف.ب"`)
  * [ ] **C99 Support**: Implement `أمر_براغما` operator (Baa: `أمر_براغما("توجيه")`).
  * [ ] `#براغما directive` (Baa: `#براغما توجيه_خاص`) (Investigate C99 standard pragmas like `STDC FP_CONTRACT`, `STDC FENV_ACCESS`, `STDC CX_LIMITED_RANGE`, and common Baa-specific pragmas like `مرة_واحدة` for `#pragma once`).

## Key Areas for C99 Compliance and Improvement

* [x] **Macro Expansion Rescanning**: Full rescanning of macro expansion results implemented. (v0.1.13.0)
* [x] **`__السطر__` (`__LINE__`) Expansion**: Corrected to expand to an integer constant. (v0.1.13.0)
* [x] **Predefined `__func__`**: Implemented `__الدالة__` (expands to placeholder `L"__BAA_FUNCTION_PLACEHOLDER__"`). (v0.1.14.0)
* [x] **Predefined `__STDC_VERSION__` equivalent**: Implemented `__إصدار_المعيار_باء__` (e.g., `10150L`). (v0.1.14.0, value updated v0.1.15.0)
* [x] **`معرف` Operator C Standard Compliance**: Fixed critical bug where `معرف` operator arguments were incorrectly macro-expanded before evaluation. Now correctly preserves argument identifiers for proper `defined` operator behavior. (v0.1.22.0)
* [x] **Full Macro Expansion in Conditional Expressions (#إذا, #وإلا_إذا)**:
  * Complete support for function-like macro expansion within conditional expression strings before evaluation, including argument handling and result rescanning. Preserves correct `معرف` operator behavior. (v0.1.23.0)
* [ ] **Token Pasting (`##`) during Rescanning (Complex Cases)**:
  * The `##` operator works in direct macro bodies.
  * Known Issue: Complex interactions when `##` appears as part of a macro expansion output that is then rescanned, or when its operands are themselves complex macros, may not be fully robust. This requires careful review of the rescan loop and how it forms new tokens after pasting.
* [x] **Macro Redefinition Warnings/Errors**: Implemented comprehensive C99-compliant macro redefinition checking with intelligent comparison system. Issues warnings for incompatible redefinitions and errors for predefined macro redefinitions, with silent acceptance of identical redefinitions as per C99 standard. (v0.1.25.0)
* [ ] **Error Recovery Mechanisms:** ()
  * **Directive Parsing Error Recovery:**
    * [ ] Update [`#تضمين`](src/preprocessor/preprocessor_directives.c) directive parsing to use [`add_preprocessor_diagnostic()`](src/preprocessor/preprocessor_directives.c) for file not found errors and continue processing
    * [ ] Enhance [`#تعريف`](src/preprocessor/preprocessor_directives.c) macro definition error handling to recover from malformed parameter lists and invalid macro names
    * [ ] Implement [`#الغاء_تعريف`](src/preprocessor/preprocessor_directives.c) error recovery for undefined macro warnings while continuing directive processing
    * [ ] Add robust error handling for [`#خطأ`](src/preprocessor/preprocessor_directives.c) and [`#تحذير`](src/preprocessor/preprocessor_directives.c) directive malformed message strings
    * [ ] Implement line-level synchronization in directive parsing to skip to next valid directive after encountering parse errors
  * **Macro Expansion Error Recovery:**
    * [ ] Update [`expand_macro()`](src/preprocessor/preprocessor_expansion.c) to use [`add_preprocessor_diagnostic()`](src/preprocessor/preprocessor_expansion.c) for undefined macro references and continue expansion
    * [ ] Enhance function-like macro argument parsing in [`parse_macro_arguments()`](src/preprocessor/preprocessor_expansion.c) to recover from mismatched parentheses and argument count errors
    * [ ] Implement stringification error recovery for [`#`](src/preprocessor/preprocessor_expansion.c) operator with invalid operands
    * [ ] Add token pasting error recovery for [`##`](src/preprocessor/preprocessor_expansion.c) operator when operands cannot form valid tokens
    * [ ] Improve recursive macro detection recovery to safely unwind expansion stack and continue processing
    * [ ] Enhance variadic macro expansion error handling for [`وسائط_إضافية`](src/preprocessor/preprocessor_expansion.c) and [`__وسائط_متغيرة__`](src/preprocessor/preprocessor_expansion.c) edge cases
  * **Conditional Expression Error Recovery:**
    * [ ] Update [`evaluate_constant_expression()`](src/preprocessor/preprocessor_expr_eval.c) to use [`add_preprocessor_diagnostic()`](src/preprocessor/preprocessor_expr_eval.c) for syntax errors and return safe default values
    * [ ] Implement division by zero error recovery in conditional expression evaluation with appropriate warnings
    * [ ] Add [`معرف`](src/preprocessor/preprocessor_expr_eval.c) operator error handling for malformed identifier arguments
    * [ ] Enhance conditional stack management to safely handle malformed [`#إذا`](src/preprocessor/preprocessor_expr_eval.c)/[`#نهاية_إذا`](src/preprocessor/preprocessor_expr_eval.c) nesting
    * [ ] Implement recovery strategies for invalid integer literals and overflow conditions in conditional expressions
  * **Synchronization Strategy Implementation:**
    * [ ] Define directive-level synchronization points to resume parsing after encountering invalid directives
    * [ ] Implement expression-level recovery to continue evaluation after encountering invalid operators or operands
    * [ ] Add conditional stack validation and automatic cleanup for unmatched [`#إذا`](src/preprocessor/preprocessor_conditionals.c)/[`#نهاية_إذا`](src/preprocessor/preprocessor_conditionals.c) pairs
    * [ ] Create line-boundary synchronization to ensure errors in one line don't corrupt subsequent line processing
  * **Configurable Error Management:**
    * [ ] Add configurable error limits with separate thresholds for errors, warnings, and notes
    * [ ] Implement smart error suppression to avoid cascading errors from the same root cause
    * [ ] Create error recovery decision logic based on error severity and context
    * [ ] Add option to treat certain recoverable errors as warnings to improve development workflow

## Known Issues / Areas for Refinement (from `CHANGELOG.md`)

* **Error/Warning Location Precision (Remaining Areas)**:
  * Column tracking within conditional expressions and for some directive arguments/macro call arguments has been enhanced (v0.1.16.0, v0.1.17.0, v0.1.22.0).
  * Further refinement for precise column reporting is an ongoing effort across all error sites.

## Testing and Validation

* [x] Basic preprocessor tester tool (`tools/baa_preprocessor_tester.c`).
* [x] Consolidated test file (`tests/resources/preprocessor_test_cases/preprocessor_test_all.baa`) covering many features.
* [ ] **More Unit Tests:** Develop granular unit tests for:
  * Directive parsing logic (`preprocessor_directives.c`).
  * Include path resolution.
  * Macro definition, expansion (especially edge cases for `#`, `##`, variadics, rescanning), and undefinition.
  * Conditional compilation logic and expression evaluation (all operators, `معرف`).
  * Error reporting and recovery mechanisms.
* [ ] **Integration Tests:** Expand tests that verify interaction with the lexer (e.g., ensuring preprocessor output is correctly tokenized).
* [ ] **Standard Compliance Tests:** Consider adapting or creating tests based on C99 preprocessor conformance suites.

This roadmap reflects the current understanding and priorities. It will be updated as development progresses.
