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
* [x] **Error Recovery Mechanisms:** ()
  * **Directive Parsing Error Recovery:**
    * [x] Update [`#تضمين`](src/preprocessor/preprocessor_directives.c) directive parsing to use [`add_preprocessor_diagnostic()`](src/preprocessor/preprocessor_directives.c) for file not found errors and continue processing
    * [x] Enhance [`#تعريف`](src/preprocessor/preprocessor_directives.c) macro definition error handling to recover from malformed parameter lists and invalid macro names
    * [x] Implement [`#الغاء_تعريف`](src/preprocessor/preprocessor_directives.c) error recovery for undefined macro warnings while continuing directive processing
    * [x] Add robust error handling for [`#خطأ`](src/preprocessor/preprocessor_directives.c) and [`#تحذير`](src/preprocessor/preprocessor_directives.c) directive malformed message strings
    * [x] Implement line-level synchronization in directive parsing to skip to next valid directive after encountering parse errors
  * **Macro Expansion Error Recovery:**
    * [x] Update [`expand_macro()`](src/preprocessor/preprocessor_expansion.c) to use [`add_preprocessor_diagnostic()`](src/preprocessor/preprocessor_expansion.c) for undefined macro references and continue expansion
    * [x] Enhance function-like macro argument parsing in [`parse_macro_arguments()`](src/preprocessor/preprocessor_expansion.c) to recover from mismatched parentheses and argument count errors
    * [x] Implement stringification error recovery for [`#`](src/preprocessor/preprocessor_expansion.c) operator with invalid operands
    * [x] Add token pasting error recovery for [`##`](src/preprocessor/preprocessor_expansion.c) operator when operands cannot form valid tokens
    * [x] Improve recursive macro detection recovery to safely unwind expansion stack and continue processing
    * [x] Enhance variadic macro expansion error handling for [`وسائط_إضافية`](src/preprocessor/preprocessor_expansion.c) and [`__وسائط_متغيرة__`](src/preprocessor/preprocessor_expansion.c) edge cases
  * **Conditional Expression Error Recovery:**
    * [x] Update [`evaluate_constant_expression()`](src/preprocessor/preprocessor_expr_eval.c) to use [`add_preprocessor_diagnostic()`](src/preprocessor/preprocessor_expr_eval.c) for syntax errors and return safe default values
    * [x] Implement division by zero error recovery in conditional expression evaluation with appropriate warnings
    * [x] Add [`معرف`](src/preprocessor/preprocessor_expr_eval.c) operator error handling for malformed identifier arguments
    * [x] Enhance conditional stack management to safely handle malformed [`#إذا`](src/preprocessor/preprocessor_expr_eval.c)/[`#نهاية_إذا`](src/preprocessor/preprocessor_expr_eval.c) nesting
    * [x] Implement recovery strategies for invalid integer literals and overflow conditions in conditional expressions
  * **Synchronization Strategy Implementation:**
    * [x] Define directive-level synchronization points to resume parsing after encountering invalid directives
    * [x] Implement expression-level recovery to continue evaluation after encountering invalid operators or operands
    * [x] Add conditional stack validation and automatic cleanup for unmatched [`#إذا`](src/preprocessor/preprocessor_conditionals.c)/[`#نهاية_إذا`](src/preprocessor/preprocessor_conditionals.c) pairs
    * [x] Create line-boundary synchronization to ensure errors in one line don't corrupt subsequent line processing
  * **Configurable Error Management:**
    * [x] Add configurable error limits with separate thresholds for errors, warnings, and notes
    * [x] Implement smart error suppression to avoid cascading errors from the same root cause
    * [x] Create error recovery decision logic based on error severity and context
    * [x] Add option to treat certain recoverable errors as warnings to improve development workflow

## Enhanced Error System Implementation

### Unified Error Collection System (v0.1.27.0+)

* [x] **Foundation**: Error collection infrastructure with diagnostic storage
* [x] **Basic Recovery**: Accumulating multiple diagnostics instead of halting on first error
* [x] **Enhanced Error System Architecture**: Comprehensive unified error collection system (v0.1.30.0+)
  * [x] **Phase 1: Foundation** (Completed)
    * [x] Enhanced data structures with severity levels (fatal, error, warning, note)
    * [x] Error limit management with configurable thresholds
    * [x] Enhanced diagnostic collection API with categorization (`add_preprocessor_diagnostic_ex()`)
    * [x] Recovery state tracking and management (`PpRecoveryState`, `PpErrorLimits`)
  * [x] **Phase 2: Core Module Updates** (Completed)
    * [x] Update `preprocessor_directives.c` with enhanced error reporting
    * [x] Update `preprocessor_expansion.c` with diagnostic collection
    * [x] Update `preprocessor_expr_eval.c` with error recovery
    * [x] Update `preprocessor_line_processing.c` with error handling
  * [x] **Phase 3: Recovery & Testing** (Completed)
    * [x] Implement context-aware recovery strategies (`determine_recovery_action()`, `execute_recovery_action()`)
    * [x] Add synchronization points for different error types (`sync_to_next_directive()`, `sync_to_next_line()`, `sync_expression_parsing()`)
    * [x] Comprehensive error recovery testing framework
    * [x] Performance optimization for error-free processing

### Error Categories and Recovery Strategies (Implemented)

* [x] **Directive Errors** (1000-1999 range):
  * [x] Unknown directive → Skip directive, continue at next line
  * [x] Missing `#نهاية_إذا` → Auto-insert at EOF with warning
  * [x] Invalid include path → Skip include, continue processing
  * [x] Malformed directive syntax → Synchronize to next line
* [x] **Macro Errors** (2000-2999 range):
  * [x] Redefinition warning → Use new definition, continue
  * [x] Argument count mismatch → Skip expansion, continue parsing
  * [x] Invalid parameter name → Skip macro definition, next line
  * [x] Recursive macro expansion → Detect and prevent infinite recursion
* [x] **Expression Errors** (3000-3999 range):
  * [x] Undefined identifier → Treat as 0, continue evaluation
  * [x] Division by zero → Return 0 with warning, continue evaluation
  * [x] Invalid operator → Skip operator, synchronize to next operand
  * [x] Malformed integer literals → Use 0, continue parsing
* [x] **File Errors** (4000-4999 range):
  * [x] File not found → Skip include, continue processing
  * [x] Circular include → Skip include, continue processing
  * [x] Encoding error → Report error, skip file
  * [x] Permission denied → Report error, skip file
* [x] **Memory Errors** (5000-5999 range):
  * [x] Allocation failure → Halt immediately (fatal)
  * [x] Stack overflow → Halt immediately (fatal)
  * [x] Buffer overflow → Halt immediately (fatal)
* [x] **Syntax Errors** (6000-6999 range):
  * [x] Unterminated string → Auto-terminate at end of line
  * [x] Invalid character → Skip character, continue parsing
  * [x] Malformed tokens → Synchronize to next valid token

### Migration Implementation Plan

* [ ] **Error Code Assignment**: Systematic assignment of error codes across all 266+ error sites
* [ ] **Severity Classification**: Proper categorization of all error conditions
* [ ] **Recovery Logic**: Implementation of context-aware recovery strategies
* [ ] **Synchronization Points**: Safe stopping points for error recovery
* [ ] **Performance Optimization**: Maintain <5% overhead for error-free processing
* [ ] **Backward Compatibility**: Preserve existing public API and error message formats

## Known Issues / Areas for Refinement

* **Error/Warning Location Precision (Remaining Areas)**:
  * Column tracking within conditional expressions and for some directive arguments/macro call arguments has been enhanced (v0.1.16.0, v0.1.17.0, v0.1.22.0).
  * Further refinement for precise column reporting is an ongoing effort across all error sites.
* **Enhanced Error System Migration**:
  * Most error-handling sites still need updates to utilize the new diagnostic collection system
  * Synchronization strategies need implementation for different error contexts
  * Error limit enforcement and cascading error prevention needs refinement

## Testing and Validation

* [x] Basic preprocessor tester tool (`tools/baa_preprocessor_tester.c`).
* [x] Consolidated test file (`tests/resources/preprocessor_test_cases/preprocessor_test_all.baa`) covering many features.
* [ ] **Enhanced Error System Testing**:
  * [ ] **Unit Tests**: Error collection functions, severity classification, limit management, recovery decision logic
  * [ ] **Integration Tests**: End-to-end processing with multiple errors, multi-file scenarios, complex macro expansion with errors
  * [ ] **Recovery Tests**: Directive error recovery, macro error recovery, expression error recovery, cascading error handling
  * [ ] **Performance Tests**: Memory usage with many errors, processing speed impact, large file error handling
  * [ ] **Regression Tests**: Backward compatibility, existing error messages, API compatibility, error format consistency
* [ ] **More Unit Tests:** Develop granular unit tests for:
  * Directive parsing logic (`preprocessor_directives.c`).
  * Include path resolution.
  * Macro definition, expansion (especially edge cases for `#`, `##`, variadics, rescanning), and undefinition.
  * Conditional compilation logic and expression evaluation (all operators, `معرف`).
  * Error reporting and recovery mechanisms.
* [ ] **Integration Tests:** Expand tests that verify interaction with the lexer (e.g., ensuring preprocessor output is correctly tokenized).
* [ ] **Standard Compliance Tests:** Consider adapting or creating tests based on C99 preprocessor conformance suites.

## Performance Targets

* **Error-Free Processing**: < 5% overhead compared to current implementation
* **Error Collection**: < 10% overhead with up to 10 errors
* **Memory Usage**: < 100KB additional memory for 100 collected errors
* **Error Formatting**: < 1ms per error message generation

This roadmap reflects the current understanding and priorities. It will be updated as development progresses.
