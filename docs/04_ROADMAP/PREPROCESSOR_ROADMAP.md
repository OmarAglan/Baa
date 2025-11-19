# Baa Language Preprocessor Roadmap

**Status: ⚠️ CRITICAL MAINTENANCE - v0.2.0.0**
**Current Focus: Fixing Macro Expansion Regressions & Memory Safety**

This roadmap outlines the status of the Baa preprocessor. While previously considered production-ready (v0.1.27.0), recent updates (v0.2.0.0) to the error system have introduced regressions in advanced macro functionality.

## 🚨 Current Status Summary (v0.2.0.0)

**Overall Health: 67% Passing Tests**

The preprocessor is currently **NOT PRODUCTION READY** due to regressions in the macro expansion pipeline.

### ✅ Stable Components

* **Core Directives:** `#تضمين` (Include), `#تعريف` (Simple Define), `#إذا` (Conditionals).
* **Arabic Syntax:** Full support for Arabic directives and keywords.
* **Enhanced Error System:** New multi-diagnostic collection system is functional.
* **Pragma System:** `#براغما` and `أمر_براغما` are implemented.
* **File Processing:** Basic file reading and encoding detection (UTF-16LE/UTF-8) are stable.

### ❌ Critical Regressions (Immediate Priorities)

1. **Macro Expansion Failure:**
    * Stringification (`#`) operator is broken (returns literal call).
    * Token Pasting (`##`) operator is broken.
    * Variadic Macros (`__وسائط_متغيرة__`) are not expanding correctly.
    * Complex rescanning logic appears disrupted.
2. **Memory Safety:**
    * Intermittent SEGFAULTS in conditional processing logic.
3. **Character Encoding:**
    * Display issues with Arabic macro names/values in specific error reporting contexts.

---

## Phase 1: Regression Fixes (IMMEDIATE PRIORITY)

**Goal:** Restore functionality to v0.1.27.0 standards while keeping the new Error System.

1. **Fix Memory Safety (SEGFAULTS):**
    * **Task:** Debug `test_preprocessor` and `test_preprocessor_conditionals` crashes.
    * **Action:** Audit buffer bounds in `preprocessor_conditionals.c` and Arabic text handling in nested contexts.
    * **Status:** 🔴 **PENDING**

2. **Restore Advanced Macro Expansion:**
    * **Task:** Fix the macro expansion pipeline in `preprocessor_expansion.c`.
    * **Action:**
        * Debug `stringify_argument` implementation.
        * Debug `substitute_macro_body` specifically for the `##` case.
        * Verify `parse_macro_arguments` correctly handles variadic commas.
    * **Status:** 🔴 **PENDING**

3. **Fix Character Encoding/Display:**
    * **Task:** Ensure Arabic characters render correctly in test outputs and error messages.
    * **Status:** 🟠 **HIGH PRIORITY**

---

## Phase 2: Core Features & Compliance (Maintained)

These features were implemented and must be verified as working after regression fixes.

* **Directives:**
  * [x] `#تضمين` (Include) - Relative/Standard paths.
  * [x] `#تعريف` (Define) - Object/Function-like.
  * [x] `#الغاء_تعريف` (Undef).
  * [x] `#خطأ` / `#تحذير` (Error/Warning).
  * [x] `#سطر` (Line control).
  * [x] `#براغما` (Pragma once).

* **Conditional Compilation:**
  * [x] `#إذا`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`.
  * [x] Defined check: `#إذا_عرف`, `#إذا_لم_يعرف`.
  * [x] Expression evaluation (`&`, `|`, `^`, `<<`, `>>`, `+`, `-`, `*`, `/`).
  * [x] `معرف` (defined) operator.

* **Predefined Macros:**
  * [x] `__الملف__`, `__السطر__`, `__التاريخ__`, `__الوقت__`.
  * [x] `__إصدار_المعيار_باء__`.

---

## Phase 3: Enhanced Error System (Completed & Integrated)

The new error system is implemented but needs tuning to ensure it doesn't interfere with logic.

* [x] **Diagnostic Categories:** Fatal, Error, Warning, Note.
* [x] **Recovery Strategies:**
  * Skip directive / Sync to next line.
  * Auto-terminate strings/comments.
* [x] **Unified Reporting:** `add_preprocessor_diagnostic_ex` API.

---

## Phase 4: Future Enhancements (Post-Fix)

* **Optimization:**
  * [ ] SIMD optimizations for large file scanning.
  * [ ] String interning for macro names.
* **Tooling:**
  * [ ] Macro expansion tracing (for debugging `baa` code).
  * [ ] Integration with Language Server (LSP).

---

## Testing Strategy

1. **Run Comprehensive Suite:** `tests/scripts/run_preprocessor_tests.py`.
2. **Targeted Debugging:**
    * Run `test_preprocessor_advanced_macros` individually to isolate expansion bugs.
    * Run `test_preprocessor_conditionals` with Valgrind/ASan to isolate memory errors.
3. **Success Criteria:**
    * All 12/12 test suites pass.
    * No memory leaks detected.
    * Arabic output is legible.
