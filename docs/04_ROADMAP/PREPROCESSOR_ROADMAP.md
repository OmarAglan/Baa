# Baa Language Preprocessor Roadmap

This roadmap outlines the planned improvements and current status of the Baa language preprocessor implementation.

## Current Status Summary (v0.2.0.0)

**Implementation Status: 67% STABLE - REQUIRES CRITICAL FIXES**

Based on comprehensive test analysis, the Baa preprocessor has solid core functionality but critical regressions in advanced features prevent production deployment.

### STABLE FUNCTIONALITY (67% of tests passing)
* **Basic Macro System**: Object-like and function-like macros working
* **Predefined Macros**: All predefined macros functional
* **Core Directives**: Include, define, conditional compilation working
* **Enhanced Error System**: Multi-error collection functional
* **Arabic Language Support**: Basic Arabic directive support working
* **Pragma System**: Pragma directives and operators working

### CRITICAL REGRESSIONS (33% test failures)
* **Advanced Macro Features**: Stringification (#), token pasting (##), and variadic macros completely broken
* **Memory Safety**: SEGFAULT crashes in conditional processing
* **Complex Rescanning**: Advanced macro expansion not working
* **Character Encoding**: Display issues with Arabic predefined macros in some contexts

### PRODUCTION READINESS
* **Core functionality**: Stable and reliable
* **Advanced features**: BROKEN - Critical regressions
* **Memory safety**: UNSTABLE - Crashes occurring
* **Overall status**: NOT PRODUCTION READY

**STATUS**: REQUIRES IMMEDIATE CRITICAL FIXES BEFORE DEPLOYMENT

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
  * [ ] **BROKEN**: Stringification (`#`) - Not working in advanced tests
  * [ ] **BROKEN**: Token Pasting (`##`) - Not working in advanced tests
  * [x] Macro recursion detection (via expansion stack)
  * [ ] **BROKEN**: C99 Support: Variadic Macros - Not expanding correctly in tests
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
  * [x] `#سطر رقم "اسم_الملف"` (Baa: `#سطر ١٠٠ "ملف.ب"`) - Implemented (v0.1.31.0)
  * [x] **C99 Support**: `أمر_براغما` operator (Baa: `أمر_براغما("توجيه")` and `براغما("توجيه")`) - Implemented (v0.1.31.0)
  * [x] `#براغما directive` (Baa: `#براغما توجيه_خاص`) - Implemented with `مرة_واحدة` (pragma once) support, unknown pragma silently ignored per C99 standard (v0.1.31.0)

## Key Areas for C99 Compliance and Improvement

* [x] **Macro Expansion Rescanning**: Full rescanning of macro expansion results implemented. (v0.1.13.0)
* [x] **`__السطر__` (`__LINE__`) Expansion**: Corrected to expand to an integer constant. (v0.1.13.0)
* [x] **Predefined `__func__`**: Implemented `__الدالة__` (expands to placeholder `L"__BAA_FUNCTION_PLACEHOLDER__"`). (v0.1.14.0)
* [x] **Predefined `__STDC_VERSION__` equivalent**: Implemented `__إصدار_المعيار_باء__` (e.g., `10150L`). (v0.1.14.0, value updated v0.1.15.0)
* [x] **`معرف` Operator C Standard Compliance**: Fixed critical bug where `معرف` operator arguments were incorrectly macro-expanded before evaluation. Now correctly preserves argument identifiers for proper `defined` operator behavior. (v0.1.22.0)
* [x] **Full Macro Expansion in Conditional Expressions (#إذا, #وإلا_إذا)**:
  * Complete support for function-like macro expansion within conditional expression strings before evaluation, including argument handling and result rescanning. Preserves correct `معرف` operator behavior. (v0.1.23.0)
* [ ] **Token Pasting (`##`) during Rescanning (Complex Cases)**:
  * **CRITICAL REGRESSION**: The `##` operator is currently not working at all in tests.
  * Advanced tests show complete failure of token pasting functionality.
  * Root cause: Macro expansion pipeline appears to be broken for advanced features.
* [x] **Macro Redefinition Warnings/Errors**: Implemented comprehensive C99-compliant macro redefinition checking with intelligent comparison system. Issues warnings for incompatible redefinitions and errors for predefined macro redefinitions, with silent acceptance of identical redefinitions as per C99 standard. (v0.1.25.0)
* [x] **Error Recovery Mechanisms:** ()
  * **Directive Parsing Error Recovery:**
    * [x] Update [`#تضمين`](../../src/preprocessor/preprocessor_directives.c) directive parsing to use [`add_preprocessor_diagnostic()`](../../src/preprocessor/preprocessor_directives.c) for file not found errors and continue processing
    * [x] Enhance [`#تعريف`](../../src/preprocessor/preprocessor_directives.c) macro definition error handling to recover from malformed parameter lists and invalid macro names
    * [x] Implement [`#الغاء_تعريف`](../../src/preprocessor/preprocessor_directives.c) error recovery for undefined macro warnings while continuing directive processing
    * [x] Add robust error handling for [`#خطأ`](../../src/preprocessor/preprocessor_directives.c) and [`#تحذير`](../../src/preprocessor/preprocessor_directives.c) directive malformed message strings
    * [x] Implement line-level synchronization in directive parsing to skip to next valid directive after encountering parse errors
  * **Macro Expansion Error Recovery:**
    * [x] Update [`expand_macro()`](../../src/preprocessor/preprocessor_expansion.c) to use [`add_preprocessor_diagnostic()`](../../src/preprocessor/preprocessor_expansion.c) for undefined macro references and continue expansion
    * [x] Enhance function-like macro argument parsing in [`parse_macro_arguments()`](../../src/preprocessor/preprocessor_expansion.c) to recover from mismatched parentheses and argument count errors
    * [x] Implement stringification error recovery for [`#`](../../src/preprocessor/preprocessor_expansion.c) operator with invalid operands
    * [x] Add token pasting error recovery for [`##`](../../src/preprocessor/preprocessor_expansion.c) operator when operands cannot form valid tokens
    * [x] Improve recursive macro detection recovery to safely unwind expansion stack and continue processing
    * [x] Enhance variadic macro expansion error handling for [`وسائط_إضافية`](../../src/preprocessor/preprocessor_expansion.c) and [`__وسائط_متغيرة__`](../../src/preprocessor/preprocessor_expansion.c) edge cases
  * **Conditional Expression Error Recovery:**
    * [x] Update [`evaluate_constant_expression()`](../../src/preprocessor/preprocessor_expr_eval.c) to use [`add_preprocessor_diagnostic()`](../../src/preprocessor/preprocessor_expr_eval.c) for syntax errors and return safe default values
    * [x] Implement division by zero error recovery in conditional expression evaluation with appropriate warnings
    * [x] Add [`معرف`](../../src/preprocessor/preprocessor_expr_eval.c) operator error handling for malformed identifier arguments
    * [x] Enhance conditional stack management to safely handle malformed [`#إذا`](../../src/preprocessor/preprocessor_expr_eval.c)/[`#نهاية_إذا`](../../src/preprocessor/preprocessor_expr_eval.c) nesting
    * [x] Implement recovery strategies for invalid integer literals and overflow conditions in conditional expressions
  * **Synchronization Strategy Implementation:**
    * [x] Define directive-level synchronization points to resume parsing after encountering invalid directives
    * [x] Implement expression-level recovery to continue evaluation after encountering invalid operators or operands
    * [x] Add conditional stack validation and automatic cleanup for unmatched [`#إذا`](../../src/preprocessor/preprocessor_conditionals.c)/[`#نهاية_إذا`](../../src/preprocessor/preprocessor_conditionals.c) pairs
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

### Migration Implementation Plan (Completed)

* [x] **Error Code Assignment**: Systematic assignment of error codes across all error sites (1000-6999 ranges)
* [x] **Severity Classification**: Proper categorization of all error conditions (Fatal, Error, Warning, Note)
* [x] **Recovery Logic**: Implementation of context-aware recovery strategies with decision logic
* [x] **Synchronization Points**: Safe stopping points for error recovery implemented
* [x] **Performance Optimization**: Maintains <5% overhead for error-free processing
* [x] **Backward Compatibility**: Preserves existing public API and error message formats

## Known Issues / Areas for Refinement

* **Error/Warning Location Precision**:
  * [x] Column tracking within conditional expressions and directive arguments (v0.1.16.0, v0.1.17.0, v0.1.22.0)
  * [x] Precise column reporting implemented across all major error sites (v0.1.30.0+)
  * [ ] Minor refinements for edge cases in complex macro expansions
* **Enhanced Error System**:
  * [x] All error-handling sites updated to utilize the new diagnostic collection system
  * [x] Synchronization strategies implemented for different error contexts
  * [x] Error limit enforcement and cascading error prevention implemented
  * [ ] Performance optimization for very large files with many errors

## Testing and Validation

* [x] Basic preprocessor tester tool (`tools/baa_preprocessor_tester.c`).
* [x] Consolidated test file (`tests/resources/preprocessor_test_cases/preprocessor_test_all.baa`) covering many features.
* [x] **Enhanced Error System Testing** (v0.1.30.0+):
  * [x] **Unit Tests**: Error collection functions, severity classification, limit management, recovery decision logic
  * [x] **Integration Tests**: End-to-end processing with multiple errors, multi-file scenarios, complex macro expansion with errors
  * [x] **Recovery Tests**: Directive error recovery, macro error recovery, expression error recovery, cascading error handling
  * [x] **Performance Tests**: Memory usage with many errors, processing speed impact, large file error handling
  * [x] **Regression Tests**: Backward compatibility, existing error messages, API compatibility, error format consistency
* [ ] **More Unit Tests:** Develop granular unit tests for:
  * Directive parsing logic (`preprocessor_directives.c`).
  * Include path resolution.
  * Macro definition, expansion (especially edge cases for `#`, `##`, variadics, rescanning), and undefinition.
  * Conditional compilation logic and expression evaluation (all operators, `معرف`).
  * Error reporting and recovery mechanisms.
* [ ] **Integration Tests:** Expand tests that verify interaction with the lexer (e.g., ensuring preprocessor output is correctly tokenized).
* [ ] **Standard Compliance Tests:** Consider adapting or creating tests based on C99 preprocessor conformance suites.

## Performance Targets (Achieved)

* [x] **Error-Free Processing**: < 5% overhead compared to basic implementation ✅
* [x] **Error Collection**: < 10% overhead with up to 10 errors ✅
* [x] **Memory Usage**: < 100KB additional memory for 100 collected errors ✅
* [x] **Error Formatting**: < 1ms per error message generation ✅

## Future Enhancements (Low Priority)

* [x] **Standard Directive Completion**: ✅ **COMPLETED (v0.1.31.0)**
  * [x] `#سطر رقم "اسم_الملف"` (line number control) - Full implementation with filename override support
  * [x] `#براغما directive` (pragma directives) - Complete with pragma once functionality and C99-compliant unknown pragma handling
  * [x] `أمر_براغما` operator (_Pragma operator) - Both Arabic forms supported with full string literal processing
* [ ] **Advanced Features**:
  * [ ] Preprocessor macro debugging support
  * [ ] Enhanced macro expansion tracing
  * [ ] Integration with IDE language servers
* [ ] **Performance Optimizations**:
  * [ ] SIMD optimizations for large file processing
  * [ ] Memory pool allocation for frequent operations
  * [ ] Parallel processing for independent include files

## Recent Completions (v0.1.31.0)

### ✅ **Line Control Directive (`#سطر`)**
* **Implementation**: Full C99-compliant line number and filename override functionality
* **Features**:
  * Basic line number control: `#سطر 100`
  * Filename override: `#سطر 50 "custom_file.baa"`
  * Integration with `__السطر__` predefined macro
  * Error reporting location override
  * Macro expansion in directive arguments
* **Testing**: Comprehensive unit test suite with 8 test cases
* **Files Modified**: `preprocessor_directives.c`, `preprocessor_utils.c`, `preprocessor_line_processing.c`

### ✅ **Pragma Directives (`#براغما`)**
* **Implementation**: Full C99-compliant pragma directive processing
* **Features**:
  * Pragma once functionality: `#براغما مرة_واحدة`
  * Unknown pragma silently ignored (C99 standard compliance)
  * Empty pragma handling
  * File inclusion tracking system
  * Integration with preprocessor core processing
* **Testing**: Complete test suite with 9 test cases (100% pass rate)
* **Files Modified**: `preprocessor_directives.c`, `preprocessor_utils.c`, `preprocessor_core.c`

### ✅ **_Pragma Operator (`أمر_براغما`)**
* **Implementation**: Full C99-compliant _Pragma operator with Arabic syntax
* **Features**:
  * Dual syntax support: `أمر_براغما("directive")` and `براغما("directive")`
  * String literal parsing with escape sequence support (`\n`, `\t`, `\"`, `\\`)
  * Integration with macro expansion system
  * Dynamic pragma generation during preprocessing
  * Comprehensive error handling for syntax errors
* **Testing**: Extensive test suite with 10 test cases (100% pass rate)
* **Files Modified**: `preprocessor_line_processing.c`, `preprocessor_utils.c`

### 📊 **Implementation Statistics**
* **Total Test Cases**: 27 new tests added
* **Test Success Rate**: 100% for new features (pragma directives and _Pragma operator)
* **Documentation**: Complete updates to preprocessor.md with examples and usage
* **Build Integration**: Full CMake integration with proper test targets

---

## Final Test Analysis & Current Status (v0.2.0.0)

**Test Run Date**: After comprehensive testing and edge case resolution attempt

### Current Status Summary

**Overall Test Pass Rate: 67% (8/12 tests passing)**

### Working Functionality
* **Basic Macro System**: 100% pass rate
  * Object-like macros
  * Function-like macros
  * Nested expansion
  * Arabic macro names
  * Macro redefinition
  * Recursive prevention

* **Predefined Macros**: 100% pass rate
  * `__الملف__`, `__السطر__`, `__التاريخ__`, `__الوقت__`
  * `__الدالة__`, `__إصدار_المعيار_باء__`
  * Expression evaluation with predefined macros

* **Core Features**: 100% pass rate
  * Enhanced error system
  * Basic directives
  * Expressions
  * Pragma directives
  * Pragma operator

### Critical Issues Identified

#### 1. SEGFAULT Issues (CRITICAL)
* `test_preprocessor` - Complete system crash
* `test_preprocessor_conditionals` - Complete system crash
* **Root Cause**: Memory access violations in conditional processing
* **Impact**: System instability

#### 2. Advanced Macro Feature Regressions (HIGH PRIORITY)

**A. Stringification Operator (`#`) - BROKEN**
```
Expected: "hello"
Actual:   STRINGIFY(hello)  // Not expanded at all
```

**B. Token Pasting Operator (`##`) - BROKEN**
```
Expected: helloworld
Actual:   CONCAT(hello, world)  // Not expanded at all
```

**C. Variadic Macros - BROKEN**
```
Expected: log()
Actual:   SIMPLE_LOG()  // Not expanded at all
```

**D. Complex Macro Rescanning - BROKEN**
```
Expected: test test
Actual:   DOUBLE(VALUE)  // Not expanded at all
```

#### 3. Line Directive Issues (MEDIUM PRIORITY)

**Character Encoding/Display Issues**
```
Expected: 101, 201, 44, 4
Actual:   __?????__  // Arabic predefined macro not displaying correctly
```

### Root Cause Analysis

#### Pattern 1: Complete Macro Expansion Failure
Advanced macro tests show macros not expanding at all - literal macro call text is returned instead of expanded result. This indicates:
* Macro expansion pipeline disrupted
* Function-like macro argument parsing broken
* Rescanning loop not executing

#### Pattern 2: Character Encoding Issues
* Predefined macros work in isolated tests
* Display as `__?????__` in line directive context
* Character encoding/display problem in specific test environments

#### Pattern 3: Memory Management Issues
* SEGFAULTs in conditional processing
* Buffer overflows or dangling pointers
* Related to Arabic text processing in conditional contexts

### Required Improvements (Prioritized)

#### IMMEDIATE (Critical)
1. **Fix SEGFAULT Issues**
   * Debug with AddressSanitizer/Valgrind
   * Check buffer bounds in conditional processing
   * Validate Arabic text handling in nested contexts

2. **Restore Advanced Macro Functionality**
   * Investigate why macro expansion pipeline stopped working
   * Check stringification (`#`) operator implementation
   * Verify token pasting (`##`) operator functionality
   * Debug variadic macro argument handling
   * Fix complex rescanning logic

#### HIGH PRIORITY
3. **Fix Character Encoding Issues**
   * Investigate predefined macro display in different contexts
   * Ensure consistent UTF-16 handling across all test environments
   * Fix Arabic character rendering in test outputs

#### MEDIUM PRIORITY
4. **Line Directive Synchronization**
   * Debug line number calculation after `#سطر` directives
   * Ensure proper predefined macro updates
   * Test conditional context line directive handling

### Production Readiness Assessment

**Current Status: NOT PRODUCTION READY**
* Core functionality: Stable
* Advanced features: SEVERELY REGRESSED
* Memory safety: CRITICAL SEGFAULTS
* Overall readiness: ~67% (DOWN from claimed 100%)

**Required for Production:**
1. Zero SEGFAULTS
2. Complete macro system functionality
3. All advanced features working
4. Character encoding stability

### Conclusion

While basic preprocessor functionality is solid, there are critical regressions in advanced features that make the current implementation NOT production-ready. The most concerning issues are:

1. Complete failure of advanced macro features (stringification, token pasting, variadic macros)
2. Memory safety issues causing SEGFAULTs
3. Character encoding instabilities

**Immediate intervention required** to restore the preprocessor to production-ready status.

---

**Last Updated**: After comprehensive test analysis (v0.2.0.0)
**Status**: REQUIRES CRITICAL FIXES BEFORE PRODUCTION USE
