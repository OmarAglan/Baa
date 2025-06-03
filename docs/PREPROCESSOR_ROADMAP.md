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
* [ ] **Full Macro Expansion in Conditional Expressions (#إذا, #وإلا_إذا)**:
  * While identifiers that are object-like macros expanding to integers are handled, full expansion of *function-like* macros within the expression string *before* evaluation is not yet complete. This includes handling their arguments and rescanning their results.
* [ ] **Token Pasting (`##`) during Rescanning (Complex Cases)**:
  * The `##` operator works in direct macro bodies.
  * Known Issue: Complex interactions when `##` appears as part of a macro expansion output that is then rescanned, or when its operands are themselves complex macros, may not be fully robust. This requires careful review of the rescan loop and how it forms new tokens after pasting.
* [ ] **Macro Redefinition Warnings/Errors**: Implement checks for macro redefinitions. Issue warnings or errors for incompatible redefinitions, as per C99 standard behavior (currently, redefinitions replace silently).
* [ ] **Error Recovery Mechanisms (Full Implementation):** (Foundation laid in v0.1.17.0)
  * **Task:** Systematically update all error reporting sites in directive parsing (`preprocessor_directives.c`) to use `add_preprocessor_diagnostic` and implement robust line-level synchronization (e.g., skip to end of directive line, or try to find next valid directive).
  * **Task:** Systematically update error reporting in macro expansion (`preprocessor_expansion.c`, `preprocessor_line_processing.c`) to use `add_preprocessor_diagnostic` and attempt to continue line processing (e.g., by outputting the unexpanded macro name or skipping the problematic expansion).
  * **Task:** Refine conditional expression error handling (`preprocessor_expr_eval.c`) to use `add_preprocessor_diagnostic` and ensure the conditional stack is safely managed (e.g., by assuming false on evaluation error and attempting to find matching `#نهاية_إذا`).
  * **Task:** Define and implement clear synchronization strategies (e.g., skip to EOL for most directive errors, attempt to find matching `#نهاية_إذا` for unterminated conditionals).

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
