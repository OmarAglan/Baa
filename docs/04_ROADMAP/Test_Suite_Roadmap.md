# Test Suite Roadmap

**Status: ✅ INFRASTRUCTURE COMPLETE / 🔄 REGRESSION TESTING**
**Current Version: v0.2.0.0**

This document outlines the comprehensive testing strategy for the Baa compiler project. The testing infrastructure is fully operational, supporting Unit, Component, and Integration testing via CTest and custom Python scripts.

## 🚨 Current Critical Objective

**Restore Preprocessor Stability:** The test suite has identified critical regressions in the Preprocessor (v0.2.0.0). Immediate focus is on getting `tests/unit/preprocessor/` to 100% pass rate.

---

## Testing Architecture (Implemented)

### 1. Core Framework (`tests/framework/`)

* **Status:** ✅ **COMPLETE**
* **Features:**
  * `TEST_SETUP()` / `TEST_TEARDOWN()` macros.
  * Memory leak detection (`TRACK_MEMORY`, `ASSERT_NO_MEMORY_LEAKS`).
  * Rich assertions (`ASSERT_WSTR_EQ`, `ASSERT_AST_STRUCTURE`).
  * Standardized result reporting.

### 2. Unit Tests (`tests/unit/`)

* **Status:** ✅ **COMPLETE**
* **Modules:**
  * **AST:** `test_ast_types`, `test_ast_program`, `test_ast_literals`, `test_ast_identifiers`, `test_ast_binary_expressions`.
  * **Lexer:** `test_lexer`, `test_tokens`, `test_lexer_arabic` (comprehensive).
  * **Parser:** `test_parser_core`, `test_parser_expressions`, `test_parser_statements`.
  * **Preprocessor:** `test_preprocessor`, `test_preprocessor_macros`, `test_preprocessor_conditionals`, etc.
  * **Core/Utils:** `test_types`, `test_utils`, `test_operators`.

### 3. Integration Tests (`tests/integration/`)

* **Status:** ✅ **COMPLETE**
* **Modules:**
  * `test_pipeline_integration`: Verifies Source → Preprocessor → Lexer → Parser → AST.
  * `test_component_interactions`: Tests hand-offs between specific modules.
  * `test_real_world_scenarios`: Compiles larger, realistic Baa code snippets.

### 4. Tooling

* **Status:** ✅ **COMPLETE**
* **Tools:**
  * `baa_lexer_tester`: CLI tool to inspect token streams.
  * `baa_preprocessor_tester`: CLI tool to inspect expanded source.
  * `baa_parser_tester`: CLI tool to inspect AST generation.
  * `run_preprocessor_tests.py`: Python runner for the complex preprocessor suite.

---

## Implementation Plan & Status

### Phase 1: Foundation (Infrastructure)

* [x] Create unified testing framework (C-based).
* [x] Set up CMake/CTest integration.
* [x] Implement memory leak tracking.

### Phase 2: Component Coverage (Basic)

* [x] **AST:** Node creation/destruction, hierarchy checks.
* [x] **Lexer:** Token types, Arabic numerals, Keywords.
* [x] **Parser:** Expression precedence, Statement dispatch.
* [x] **Preprocessor:** Directives, Macros, Conditionals.

### Phase 3: Advanced Scenarios (Integration)

* [x] End-to-end pipeline validation.
* [x] Arabic language compliance (right-to-left logic in tests).
* [x] Error recovery testing (Parser synchronization).

### Phase 4: Semantic Analysis Testing (Upcoming)

* [ ] **Symbol Table:** Scope entry/exit, lookup, shadowing.
* [ ] **Type Checking:** Validating `int + int`, rejecting `int + string`.
* [ ] **Control Flow:** Detecting unreachable code.

---

## Current Priorities (Maintenance & Regression)

### 1. Preprocessor Regression Fixes

The following tests are failing in v0.2.0.0 and must be fixed:

* `test_preprocessor_advanced_macros`: Stringification (`#`) and Token Pasting (`##`) are broken.
* `test_preprocessor_conditionals`: Segfaults occurring in nested conditions.
* **Action:** Debug `src/preprocessor/preprocessor_expansion.c`.

### 2. Lexer & Parser Validation

* Ensure the Parser tests pass with the new `BaaNode` AST structure (v3).
* Verify error messages are correctly localized (Arabic) in the test output.

### 3. Code Generation Tests

* **Status:** ⏸️ **PAUSED**
* The tests in `tests/codegen_tests/` are currently referencing outdated AST structures.
* **Action:** Disable or update these tests once the LLVM Codegen refactoring (Phase 4 of Master Roadmap) begins.

---

## Usage Guide

### Running All Tests

```bash
cd build
ctest --output-on-failure
```

### Running Specific Component Tests

```bash
# Preprocessor specific
./tests/unit/preprocessor/test_preprocessor

# Parser specific
./tests/unit/parser/test_parser_statements
```

### Running the Python Runner

```bash
python3 ../tests/scripts/run_preprocessor_tests.py --build-dir .
```
