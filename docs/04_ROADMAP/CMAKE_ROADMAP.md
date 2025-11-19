# Baa Language CMake Build System Roadmap

**Status: ✅ STABLE / MATURE (v0.2.0.0)**
**Current Focus: Packaging, Installation & Sanitizers**

This document outlines the evolution and future improvements for the Baa project's CMake build system. The system has successfully transitioned to a modern, target-centric, and modular design that supports the complex compiler pipeline.

## Current Status (v0.2.0.0)

The build system is **fully operational**. It correctly manages dependencies between the Preprocessor, Lexer, Parser, and AST libraries, and integrates a comprehensive test suite.

### ✅ Completed Milestones

1. **Modular Target Structure:**
    * Specific static libraries for each component (`baa_lexer`, `baa_parser`, `baa_ast`, etc.).
    * Strict dependency management (e.g., `baa_parser` links `baa_lexer` PRIVATEly).
    * Interface library `BaaCommonSettings` for project-wide definitions (`UNICODE`, `_CRT_SECURE_NO_WARNINGS`).

2. **Test Infrastructure Integration:**
    * Full `CTest` integration.
    * Automatic discovery of tests.
    * Custom test runners (Python) integrated via `add_test`.
    * Platform-specific flags (UTF-8 enforcement for GCC/Clang).

3. **Conditional Backend Support:**
    * `USE_LLVM` option implemented.
    * Graceful fallback to `llvm_stub.c` if LLVM is not found or disabled.
    * Proper handling of LLVM target definitions.

4. **Safety & Quality:**
    * AddressSanitizer (ASan) enabled for Debug builds on Linux/macOS.
    * Out-of-source build enforcement.

---

## Future Improvements & Roadmap

While the core is done, the following features are planned to support distribution and advanced development.

### Phase 1: Sanitizer & Analysis Modules (Immediate)

* [ ] **Formalize Sanitizers:** Move the manual ASan flags from `tests/CMakeLists.txt` to a dedicated `cmake/Sanitizers.cmake` module.
  * Add support for MSVC Address Sanitizer (experimental).
  * Add UndefinedBehaviorSanitizer (UBSan) options.
* [ ] **Static Analysis:** Add `cmake` options to auto-run `clang-tidy` or `cppcheck` during build.

### Phase 2: Installation & Distribution

* [ ] **Install Rules:** Implement `install()` commands.
  * Executable: `bin/baa`.
  * Headers: `include/baa/`.
  * Libraries: `lib/libbaa_*.a` (if exposing as an SDK).
* [ ] **Standard Paths:** Use `GNUInstallDirs` to respect system conventions (`/usr/local`, etc.).

### Phase 3: Packaging (CPack)

* [ ] **CPack Integration:** Allow generating `.deb`, `.rpm`, or `.zip` releases via `cpack`.
* [ ] **Package Config:** Generate `BaaConfig.cmake` so other CMake projects can do `find_package(Baa)`.

### Phase 4: Documentation Generation

* [ ] **Doxygen Integration:** Add a `doc` target to auto-generate API documentation from the source headers.

---

## Development Guidelines

* **New Components:** Must define their own `CMakeLists.txt` in `src/NewComponent` and be added via `add_subdirectory`.
* **Dependencies:** Always use `target_link_libraries` with `PRIVATE` unless the header files of the dependency are exposed in the public API of the component (then use `PUBLIC`).
* **Testing:** Every new component must have a corresponding entry in `tests/unit/CMakeLists.txt`.
