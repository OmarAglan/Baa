# Baa Language CMake Build System Roadmap

This document outlines the evolution and future improvements for the Baa project's CMake build system, aiming for increased modularity, maintainability, and adherence to modern CMake practices.

**Current Status (as of Build System v0.1.18.0):**

The CMake build system has undergone a major refactoring towards a target-centric and modular design.

**Completed Refactoring Steps:**

1. **Enforce Out-of-Source Builds:**
    * A check has been added to the root `CMakeLists.txt` to prevent in-source builds and guide users to create a separate build directory.

2. **Adopt Target-Centric Approach:**
    * **Global Settings Removed/Migrated:** Global `include_directories()` and `add_definitions()` have been removed from the root `CMakeLists.txt`.
    * **Per-Target Properties:** Component libraries (e.g., `baa_utils`, `baa_lexer`) and executables now use `target_include_directories()`, `target_compile_definitions()`, and `target_compile_options()` with appropriate visibility (`PUBLIC`, `PRIVATE`, `INTERFACE`).
    * Common compile definitions like `UNICODE`, `_UNICODE`, `_CRT_SECURE_NO_WARNINGS` are managed by an interface library `BaaCommonSettings`.

3. **Declare Dependencies Explicitly:**
    * Each component's `CMakeLists.txt` (in `src/*`) now explicitly links against its direct Baa library dependencies using `target_link_libraries(... PRIVATE ...)` (e.g., `baa_types` links `baa_utils`). This allows CMake to correctly manage build order and transitive dependencies.

4. **Refine Executable Linking:**
    * The main `baa` executable and tool executables (`baa_lexer_tester`, `baa_preprocessor_tester`) now link against the component static libraries they require, instead of compiling all sources directly.
    * A `baa_compiler_lib` static library has been created from `src/compiler.c`. The `baa` executable (from `src/main.c`) links against `baa_compiler_lib`, which in turn consolidates dependencies on other Baa components (lexer, preprocessor, types, utils, etc.).

5. **CMake Policies:**
    * Relevant CMake policies (CMP0074, CMP0067, CMP0042) have been set in the root `CMakeLists.txt` to ensure modern and consistent CMake behavior.

6. **LLVM Handling Centralized:**
    * The decision to use LLVM (`USE_LLVM` option and `find_package(LLVM)`) is made in the root `CMakeLists.txt`.
    * The `baa_codegen` library's `CMakeLists.txt` now conditionally includes LLVM sources (`llvm_codegen.c` or `llvm_stub.c`), sets `LLVM_AVAILABLE` definition, adds LLVM include directories, and links LLVM libraries based on the `USE_LLVM` option and `LLVM_FOUND` status.

**Benefits Achieved:**

* Improved modularity and clear separation of concerns for each Baa component.
* A clearer dependency graph, managed by CMake.
* More robust and maintainable handling of include paths and compile options/definitions.
* Better alignment with modern CMake best practices.

---

## Future Improvements / Remaining Roadmap Items

While the major refactoring is complete, the following items from the original roadmap or new ideas can be considered for further enhancement:

1. **Complete `cmake/` Directory Modularization:**
    * **LLVM Setup Module:** Move the `option(USE_LLVM ...)` and `if(USE_LLVM) find_package(LLVM ...) endif()` logic from the root `CMakeLists.txt` into a dedicated module (e.g., `cmake/LLVMSetup.cmake`).
        * The root `CMakeLists.txt` would then simply `include(LLVMSetup)`.
        * This module could set cache variables like `BAA_LLVM_FOUND`, `BAA_LLVM_INCLUDE_DIRS`, `BAA_LLVM_LIBRARIES_LIST` that `src/codegen/CMakeLists.txt` can then consume.
    * **Refine `BaaCompilerSettings.cmake`:**
        * Consider moving the C standard settings (`set(CMAKE_C_STANDARD 11)`, `set(CMAKE_C_STANDARD_REQUIRED ON)`) into this module and applying them via the `BaaCommonSettings` interface library or a dedicated function.
        * Review if other project-wide compiler flags (like `-finput-charset=UTF-8 -fexec-charset=UTF-8` currently in `tests/CMakeLists.txt`) should be part of `BaaCommonSettings` if applicable to all targets.

2. **Testing Infrastructure Integration:**
    * Ensure `enable_testing()` and `add_subdirectory(tests)` are correctly placed and functional in the root `CMakeLists.txt` when tests are fully active.
    * Review `tests/CMakeLists.txt` for applying common settings (like `BaaCommonSettings`) to test executables.

3. **Installation Rules:**
    * Define `install()` rules for the `baa` executable, tool executables, and any public headers/libraries if the project is intended to be installable.
    * Consider using `GNUInstallDirs` for standard installation paths.

4. **Package Configuration File (for `find_package`):**
    * If `baa` itself is intended to be used as a library by other CMake projects, generate a `BaaConfig.cmake` and `BaaConfigVersion.cmake` file upon installation. This allows downstream projects to easily find and use Baa components via `find_package(Baa)`.

5. **Code Coverage Setup (Optional):**
    * Integrate tools for generating code coverage reports (e.g., using `gcov`/`lcov` with GCC/Clang).

6. **Static Analysis Integration (Optional):**
    * Add options to integrate static analysis tools like `clang-tidy` or `cppcheck` into the build process.

7. **Conditional Component Logic (Review):**
    * The original roadmap mentioned: "Move conditional logic (like `SKIP_PARSER`) from component `CMakeLists.txt` files to the root file, wrapping the corresponding `add_subdirectory()` calls."
    * With the parser removed, this specific example is moot. However, if other components become optional in the future, this pattern should be followed. Ensure the current `USE_LLVM` logic correctly gates the LLVM-specific parts of `baa_codegen`.

These future steps aim to further refine the build system's organization, add more advanced features like installation and packaging, and integrate more development tools. The current refactored state provides a solid foundation.
