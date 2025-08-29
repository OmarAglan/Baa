# B (باء) Compiler Development Guide

This guide provides information for developers contributing to the Baa (باء) compiler project. It covers environment setup, project organization, coding standards, and guidelines for implementing various components.

## Development Environment Setup

### Prerequisites

* **CMake**: Version 3.20 or higher.
* **C Compiler**: A C11 compliant compiler (e.g., Clang, GCC, MSVC). Clang-cl is used for Windows development in this project.
* **Git**: For version control.
* **LLVM Development Libraries (Optional)**: Required if you want to enable and work on the LLVM code generation backend. If not present, the compiler will use a stub backend.
* **Ninja (Recommended)**: For faster builds, especially with CMake.

### Building from Source

The project uses CMake for building. It is **strongly recommended to perform out-of-source builds**.

1. **Clone the repository:**

    ```bash
    git clone https://github.com/OmarAglan/Baa.git
    cd baa
    ```

2. **Create a build directory and configure:**

    ```bash
    mkdir build
    cd build
    # For Windows with Clang-cl (adjust paths to your Clang installation)
    cmake -G "Ninja" -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ..
    # For Linux/macOS with system Clang or GCC
    # cmake -G "Ninja" ..
    ```

    * You can add `-DUSE_LLVM=ON` to the cmake command if you have LLVM installed and want to enable the LLVM backend. You might need to provide `LLVM_DIR` if CMake doesn't find it automatically (e.g., `-DLLVM_DIR=/path/to/llvm/cmake`).

3. **Build the project:**

    ```bash
    cmake --build .
    # Or directly use ninja:
    # ninja
    ```

    The executables (`baa`, `baa_lexer_tester`, `baa_preprocessor_tester`) will be placed in the `build/` directory (or `build/bin/` if `RUNTIME_OUTPUT_DIRECTORY` is set for tools). Libraries (`.lib` or `.a`) will also be in the build directory, typically under subdirectories corresponding to their source location (e.g., `build/src/utils/baa_utils.lib`).

### Running Tests

(Assuming tests are configured and enabled in `tests/CMakeLists.txt`)

```bash
cd build
ctest --output-on-failure
```

## Project Organization

The project is organized into several main directories:

```
baa/
├── cmake/                  # Custom CMake modules (e.g., BaaCompilerSettings.cmake)
├── docs/                   # Project documentation
├── include/                # Public header files for the Baa libraries
│   └── baa/
│       ├── analysis/
│       ├── codegen/
│       ├── compiler.h
│       ├── diagnostics/    # (Currently AST-dependent, may need rework)
│       ├── lexer/
│       ├── operators/
│       ├── preprocessor/
│       ├── types/
│       └── utils/
├── src/                    # Source code for Baa libraries and compiler
│   ├── CMakeLists.txt      # CMake file that adds all component subdirectories
│   ├── analysis/
│   ├── codegen/
│   ├── compiler.c          # Core compilation logic (part of baa_compiler_lib)
│   ├── lexer/
│   ├── main.c              # Entry point for the 'baa' executable
│   ├── operators/
│   ├── preprocessor/
│   ├── types/
│   └── utils/
├── tests/                  # Unit and integration tests
│   ├── CMakeLists.txt
│   ├── framework/          # Simple test framework
│   ├── unit/
│   ├── integration/
│   └── resources/          # Test input files
└── tools/                  # Standalone tester executables
    ├── baa_lexer_tester.c
    ├── baa_parser_tester.c # (Currently depends on non-existent parser)
    └── baa_preprocessor_tester.c
```

Key Files:

* **Root `CMakeLists.txt`**: Main project configuration, defines executables, includes `cmake/` modules and `src/` subdirectory.
* **`src/CMakeLists.txt`**: Adds all component subdirectories (utils, types, lexer, etc.) and defines `baa_compiler_lib`.
* **`src/<component>/CMakeLists.txt`**: Defines the static library for each component (e.g., `baa_utils`, `baa_lexer`).
* **`cmake/BaaCompilerSettings.cmake`**: Defines an interface library `BaaCommonSettings` to propagate common compile definitions.

## Coding Standards

### C Code Style

* **Indentation**: 4 spaces.
* **Line Length**: Aim for a maximum of 80-100 characters for readability.
* **Naming**:
  * Functions: `baa_component_action()` (e.g., `baa_lexer_next_token()`). Internal static functions can use `helper_action()`.
  * Structs/Enums/Typedefs: `BaaComponentType` (e.g., `BaaLexer`, `BaaTokenType`).
  * Macros/Constants: `BAA_UPPER_CASE` (e.g., `BAA_TOKEN_EOF`).
* **Comments**: Use `//` for single-line and `/* ... */` for multi-line. Doxygen-style comments are encouraged for public APIs.
  * Provide comments in English. Arabic comments can be added for clarification where beneficial.

    ```c
    /**
     * @brief Brief English description.
     * @arabic الوصف بالعربية هنا إذا لزم الأمر.
     * @param param_name Description of parameter.
     * @return Description of return value.
     */
    // BaaError baa_util_some_function(BaaSomeType* param);
    ```

* **Headers**: Use `#pragma once` or standard include guards.

    ```c
    #ifndef BAA_COMPONENT_MY_HEADER_H
    #define BAA_COMPONENT_MY_HEADER_H

    // ... content ...

    #endif // BAA_COMPONENT_MY_HEADER_H
    ```

* **Braces**: K&R style (opening brace on the same line for functions and control structures).

    ```c
    void function_name(int arg) {
        if (arg > 0) {
            // ...
        } else {
            // ...
        }
    }
    ```

## Build System (CMake)

The project uses a modular, target-centric CMake build system:

* Each component in `src/` (lexer, types, utils, etc.) is built as a **static library** (e.g., `baa_lexer`, `baa_utils`).
* Each component library defines its own sources, include directories (`target_include_directories`), compile definitions (`target_compile_definitions`), and dependencies on other Baa libraries (`target_link_libraries`).
* Public headers are placed in `include/baa/<component>/` and made available using `target_include_directories(... PUBLIC ${PROJECT_SOURCE_DIR}/include)`.
* Internal headers are co-located with sources or in a private include directory for that component, specified with `target_include_directories(... PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})`.
* The main `baa` executable (from `src/main.c`) links against `baa_compiler_lib`.
* `baa_compiler_lib` (from `src/compiler.c`) links against all other necessary component libraries (`baa_lexer`, `baa_preprocessor`, `baa_codegen`, `baa_types`, `baa_utils`, etc.).
* Common compile definitions (like `UNICODE`, `_UNICODE`, `_CRT_SECURE_NO_WARNINGS`) are managed by an interface library `BaaCommonSettings` defined in `cmake/BaaCompilerSettings.cmake`. All targets link this interface library to inherit these properties.

## Implementation Guidelines by Component

### Utils (`src/utils/`)

* Provides common utility functions (memory allocation wrappers `baa_malloc`, `baa_free`, `baa_realloc`; string duplication `baa_strdup`, `baa_strdup_char`; file I/O `baa_file_content`, `baa_read_file`).
* Defines common error handling mechanisms (`BaaError` enum, `baa_set_error`, etc.).

### Types (`src/types/`)

* Defines the Baa language's type system (`BaaType` struct, `BaaTypeKind` enum).
* Initializes and provides access to global predefined types (e.g., `baa_type_int`, `baa_type_float`).
* Implements type comparison and basic convertibility checks.

### Operators (`src/operators/`)

* Defines Baa operators, their symbols, precedence, and associativity (`BaaOperatorInfo`, `BaaOperatorType`, `BaaOperatorPrecedence`).
* Provides functions for validating operator usage with given operand types (e.g., `baa_validate_binary_op`).

### Preprocessor (`src/preprocessor/`)

* Handles directives (`#تضمين`, `#تعريف`, conditionals like `#إذا_عرف`, `#إذا`, etc.).
* Performs macro expansion (object-like, function-like, variadic, stringification, token pasting) with rescanning.
* Manages include paths, file encoding detection (UTF-8/UTF-16LE), and circular include detection.
* Outputs a single preprocessed `wchar_t*` stream for the lexer.
* Uses an internal location stack (`PpSourceLocation`) to provide accurate error reporting from original source files.

### Lexer (`src/lexer/`)

* Tokenizes the preprocessed `wchar_t*` stream.
* Supports Arabic identifiers, keywords, and numeric literals (including Arabic-Indic digits, hex/binary prefixes, underscores).
* Handles string/character literals with standard escapes. Planned: Baa-specific Arabic escapes.
* Recognizes documentation comments (`/** ... */`).
* Modular structure:
  * `lexer.c`: Core dispatch logic, keyword table.
  * `token_scanners.c`: Functions for scanning specific token categories (identifiers, numbers, strings).
  * `lexer_char_utils.c`: Character classification utilities.
  * `number_parser.c`: (Utility often called by lexer or parser) Converts numeric lexemes to values.

### Parser (`src/parser/`) - **Currently under Redesign/Removal**

* **(In Progress - New Design v2)** Defines the structure of AST nodes using a unified `BaaNode`.
  * Each `BaaNode` has a `kind` (`BaaNodeKind`), a `BaaSourceSpan`, and a `void* data` pointing to kind-specific data.
  * Type-safe accessor macros are planned.
* **(In Progress)** Manages memory for AST nodes via `baa_ast_new_node` and `baa_ast_free_node`, with helpers for specific node data.
* **(In Progress)** Provides functions for creating specific AST nodes (e.g., `baa_ast_new_literal_int_node`).

* *Refer to `docs/PARSER.md` and `docs/PARSER_ROADMAP.md` for the new design plans.*

### AST (Abstract Syntax Tree) - **Currently under Redesign/Removal**

* **(Planned)** Defines the structure of the AST nodes (`BaaNode`, `BaaExpr`, `BaaStmt`, specific node data structs).
* **(Planned)** Manages memory for AST nodes.
* **(Planned)** Provides functions for creating and freeing AST nodes.
* *Refer to `docs/AST.md` and `docs/AST_ROADMAP.md` for the new design plans.*

### Analysis (`src/analysis/`)

* Contains semantic analysis passes.
* `flow_analysis.c`: Control flow checks (e.g., break/continue validity, return path analysis).
  * *(Currently contains stubs as it was AST-dependent; needs integration with new AST/semantic analysis framework).*
* **(Planned)** Symbol table management, name resolution, type checking, and other semantic validations.

### Code Generation (`src/codegen/`)

* Transforms the (semantically analyzed) AST into target code.
* `codegen.c`: Main codegen dispatch logic.
* `llvm_codegen.c`: LLVM IR generation implementation (used if `USE_LLVM=ON` and LLVM is found).
  * *(Currently contains stubs or AST-dependent code that needs refactoring to work with the new/planned AST or intermediate representation).*
* `llvm_stub.c`: Stub implementation used when LLVM is not available/enabled.

### Compiler (`src/compiler.c` - part of `baa_compiler_lib`)

* Orchestrates the compilation pipeline: Preprocessing -> Lexing -> (Future: Parsing -> Semantic Analysis) -> Code Generation.
* `compile_baa_file()` is the main entry point.

### Main Executable (`src/main.c` - builds `baa.exe`)

* Handles command-line argument parsing.
* Calls `compile_baa_file()`.

## Error Handling

* Utilize `baa_set_error()` from `baa/utils/errors.h` for general errors.
* Lexer reports errors by returning a `BAA_TOKEN_ERROR` token with the message in `lexeme`.
* Preprocessor accumulates diagnostics in its internal state and reports them.
* Parser (when re-implemented) will have its own error reporting and recovery.
* Aim for clear, actionable error messages, preferably in Arabic or bilingual.

## Testing

* Unit tests are located in `tests/unit/<component>/`.
* Integration tests (planned) in `tests/integration/`.
* Test resources (input files) are in `tests/resources/`.
* The `tests/framework/` provides a simple assertion-based test framework.
* Tests are added to CMake in their respective `CMakeLists.txt` files and run via `ctest`.
* Strive for high test coverage for all components.

Remember to consult the specific roadmap documents for each component (`LEXER_ROADMAP.md`, `PREPROCESSOR_ROADMAP.md`, etc.) for detailed plans and status.
