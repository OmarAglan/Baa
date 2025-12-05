# Baa Project Structure

## Overview

This document describes the organizational structure of the Baa compiler project. The structure aims to achieve clarity, separation of concerns, and ease of development and maintenance.

## Main Directories

```
baa/
├── .editorconfig            # Editor settings for consistent code style
├── .gitignore               # Files ignored by Git
├── CHANGELOG.md             # Version history and release notes
├── CMakeLists.txt           # Main CMake build file
├── LICENSE                  # MIT License
├── README.md                # Project overview and introduction
│
├── cmake/                   # Custom CMake modules
│   └── BaaCompilerSettings.cmake # Shared compiler settings
│
├── docs/                    # Documentation
│   ├── index.md             # Documentation entry point
│   ├── 00_OVERVIEW/         # Overview and getting started
│   ├── 01_LANGUAGE_SPECIFICATION/  # Language reference
│   ├── 02_COMPILER_ARCHITECTURE/   # Compiler internals
│   ├── 03_DEVELOPMENT/      # Development guides
│   ├── 04_ROADMAP/          # Development roadmaps
│   └── 05_API_REFERENCE/    # API documentation
│
├── include/                 # Public header files
│   └── baa/                 # Main namespace for Baa libraries
│       ├── analysis/        # Semantic analysis interfaces
│       │   ├── flow_analysis.h
│       │   └── flow_errors.h
│       ├── ast/             # AST definitions
│       │   ├── ast.h
│       │   └── ast_types.h
│       ├── codegen/         # Code generation interfaces
│       │   ├── codegen.h
│       │   └── llvm_codegen.h
│       ├── compiler.h       # Main compiler interface
│       ├── diagnostics/     # Error reporting
│       │   └── diagnostics.h
│       ├── lexer/           # Lexer interfaces
│       │   ├── lexer.h
│       │   ├── lexer_char_utils.h
│       │   └── token_scanners.h
│       ├── operators/       # Operator system
│       │   └── operators.h
│       ├── parser/          # Parser interface
│       │   └── parser.h
│       ├── preprocessor/    # Preprocessor interface
│       │   └── preprocessor.h
│       ├── types/           # Type system interface
│       │   └── types.h
│       └── utils/           # Utility interfaces
│           ├── errors.h
│           └── utils.h
│
├── src/                     # Source code
│   ├── CMakeLists.txt       # Adds component subdirectories
│   │
│   ├── analysis/            # Semantic analysis implementation
│   │   ├── CMakeLists.txt
│   │   ├── flow_analysis.c
│   │   └── flow_errors.c
│   │
│   ├── ast/                 # AST implementation
│   │   ├── CMakeLists.txt
│   │   ├── ast.c
│   │   └── ast_nodes.c
│   │
│   ├── codegen/             # Code generation implementation
│   │   ├── CMakeLists.txt
│   │   ├── codegen.c        # General code generation logic
│   │   ├── llvm_codegen.c   # LLVM backend (when enabled)
│   │   └── llvm_stub.c      # Stub when LLVM is disabled
│   │
│   ├── compiler.c           # Main compilation logic
│   │
│   ├── lexer/               # Lexer implementation
│   │   ├── CMakeLists.txt
│   │   ├── lexer.c          # Core lexer logic
│   │   ├── lexer_char_utils.c # Character utilities
│   │   ├── number_parser.c  # Number literal conversion
│   │   └── token_scanners.c # Token scanning functions
│   │
│   ├── main.c               # Main entry point for `baa` executable
│   │
│   ├── operators/           # Operator implementation
│   │   ├── CMakeLists.txt
│   │   └── operators.c
│   │
│   ├── parser/              # Parser implementation
│   │   ├── CMakeLists.txt
│   │   ├── parser.c
│   │   ├── declaration_parser.c
│   │   ├── expression_parser.c
│   │   └── statement_parser.c
│   │
│   ├── preprocessor/        # Preprocessor implementation
│   │   ├── CMakeLists.txt
│   │   ├── preprocessor.c
│   │   ├── preprocessor_conditionals.c
│   │   ├── preprocessor_core.c
│   │   ├── preprocessor_directives.c
│   │   ├── preprocessor_expansion.c
│   │   ├── preprocessor_expr_eval.c
│   │   ├── preprocessor_internal.h
│   │   ├── preprocessor_line_processing.c
│   │   ├── preprocessor_macros.c
│   │   └── preprocessor_utils.c
│   │
│   ├── types/               # Type system implementation
│   │   ├── CMakeLists.txt
│   │   └── types.c
│   │
│   └── utils/               # Utility implementation
│       ├── CMakeLists.txt
│       └── utils.c
│
├── tests/                   # Test suite
│   ├── CMakeLists.txt       # Test configuration
│   │
│   ├── framework/           # Simple test framework
│   │   ├── test_framework.c
│   │   └── test_framework.h
│   │
│   ├── codegen_tests/       # Code generation tests
│   │   └── CMakeLists.txt
│   │
│   ├── integration/         # Integration tests (planned)
│   │   └── CMakeLists.txt
│   │
│   ├── resources/           # Test input files
│   │   ├── lexer_test_cases/
│   │   ├── parser_tests/
│   │   └── preprocessor_test_cases/
│   │
│   └── unit/                # Unit tests for each component
│       ├── CMakeLists.txt
│       ├── core/
│       ├── lexer/
│       ├── preprocessor/
│       └── utils/
│
└── tools/                   # Developer/user tools
    ├── baa_lexer_tester.c
    ├── baa_parser_tester.c
    └── baa_preprocessor_tester.c
```

## Notes on Structure

### Header Files (`include/baa/`)

Contains public header files (`.h`) that form the API for each compiler component. These files should be as independent as possible and not expose internal implementation details.

### Source Files (`src/<component>/`)

Each major compiler component (e.g., `lexer`, `preprocessor`, `types`) has its own directory inside `src/`:

- **`src/<component>/CMakeLists.txt`**: Defines how to build the static library for that component
- **`src/<component>/*.c`**: Source files implementing the component
- **`src/<component>/*_internal.h`**: Internal headers shared between `.c` files *within the same component only*

### Build System

- **Root `CMakeLists.txt`**: Main project configuration, options, package finding
- **`cmake/BaaCompilerSettings.cmake`**: Shared compiler settings via `BaaCommonSettings` interface library
- **Target-Centric Approach**: Build properties applied to specific targets, not globally

### Main Files

- **`src/compiler.c`**: Orchestrates the compilation process (preprocessing, lexing, parsing, etc.)
- **`src/main.c`**: Entry point for the `baa` executable

This structure provides an organized and scalable foundation for the Baa project.
