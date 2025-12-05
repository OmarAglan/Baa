# Baa Programming Language (لغة البرمجة باء)

---

## English

Baa is a programming language designed to support Arabic syntax while maintaining conceptual compatibility with C language features. It allows developers to write code using Arabic keywords and identifiers. The project aims to provide a complete compiler toolchain, including a preprocessor, lexer, parser, semantic analyzer, and code generator.

## Current Status (Updated 2025-01-09 - Core Compiler Complete)

The Baa language compiler has reached a significant milestone with the completion of **Priority 4: Function Definitions and Calls** in July 2025. The core compiler infrastructure is now fully functional and production-ready, supporting a comprehensive set of C-like language constructs with Arabic keywords and syntax. Key components and their status:

### Core Features and Status

* **Build System (CMake):**
  * **Restructured:** Now uses a modular, target-focused approach. Component libraries (utils, types, lexer, preprocessor, etc.) are built as static libraries and explicitly linked.
  * **Out-of-source builds enforced.**
  * **Shared compilation definitions:** Managed via `BaaCommonSettings` interface library (defined in `cmake/BaaCompilerSettings.cmake`).
  * LLVM integration is conditional and managed by `baa_codegen` library.

* **Preprocessor (`src/preprocessor/`):** - ✅ **Production Ready (100% Complete)**
  * Handles file inclusion (`#تضمين`) with relative and standard path resolution, circular inclusion detection.
  * Supports macro definitions (`#تعريف`) including object-like, function-like (with parameters, stringification `#`, token pasting `##`, variadic parameters `وسائط_إضافية`/`__وسائط_متغيرة__`), and rescanning of expansion results.
  * C99-compatible macro redefinition checking with intelligent comparison.
  * Handles `#الغاء_تعريف` (undefine).
  * Implements conditional compilation (`#إذا`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`) with expression evaluation.
  * Provides Arabic predefined macros (`__الملف__`, `__السطر__`, `__التاريخ__`, `__الوقت__`, etc.).
  * Supports `#خطأ` and `#تحذير` directives.
  * Input file encoding detection (UTF-8 default, UTF-16LE with BOM).
  * Comprehensive error recovery system with multiple error reporting.

* **Lexer (`src/lexer/`):** - ✅ **Production Ready (100% Complete)**
  * Tokenizes preprocessed UTF-16LE stream.
  * Supports Arabic/English identifiers, Arabic-Indic numerals, Arabic keywords.
  * Tokenizes whitespace, newlines, and all comment types as separate tokens.
  * Handles numeric literals with Arabic-Indic digits, prefixes, and underscores.
  * Handles string and character literals with custom Baa Arabic escape sequences.
  * Precise line/column tracking and lexical error reporting.

* **Parser (`src/parser/`):** - ✅ **Production Ready (Core Features Complete)**
  * ✅ **Complete redesign finished** - Modern recursive descent parser with comprehensive error handling.
  * ✅ **Priority 4 Complete (2025-07-06)** - Function definitions and calls fully implemented.
  * ✅ **Full expression support** - Precedence climbing algorithm with all operators.
  * ✅ **Control flow statements** - If/else, while, for, return, break, continue.
  * ✅ **Variable declarations** - Type specifiers, initialization, const qualifier.
  * ✅ **Function support** - Complete function definitions with parameters and calls.
  * ✅ **Arabic syntax** - Native support for Arabic keywords throughout the language.

* **Abstract Syntax Tree (AST):** - ✅ **Production Ready (Core Features Complete)**
  * ✅ **Complete redesign finished** - Unified `BaaNode` structure with tagged union.
  * ✅ **Priority 4 Complete (2025-07-06)** - Function, parameter, and call expression nodes.
  * ✅ **Comprehensive node types** - All expressions, statements, and declarations implemented.
  * ✅ **Memory management** - Robust creation, cleanup, and error handling.
  * ✅ **Source spans** - Precise source location tracking for all nodes.

* **Type System (`src/types/`):** - *Basics Implemented*
  * Basic types implemented (`عدد_صحيح`, `عدد_حقيقي`, `حرف`, `منطقي`, `فراغ`).
  * Basic type checking and conversion rules defined.
  * Array type support (`BAA_TYPE_ARRAY`) exists at type system level.

* **Operators (`src/operators/`):** - *Basics Implemented*
  * Definitions for arithmetic, comparison, logical, and assignment operators.
  * Operator precedence table defined.
  * Basic type checking for operators.

### 🚀 Current Capabilities (Priority 4 Complete - July 2025)

* **Complete core language:** Variables, functions, control flow, expressions with full Arabic syntax
* **Production-ready function system:** Function definitions with parameters, function calls, and return statements
* **Robust error handling:** Comprehensive error reporting with Arabic messages and intelligent recovery
* **Native Arabic integration:** Full Unicode support with Arabic keywords, identifiers, and escape sequences
* **Complete tokenization:** Arabic numerals, string literals, comments, and all language constructs

### 📋 Next Development Phase (Priority 5 - Planned)

* **Advanced type system:** Arrays, structs, unions, enums, and pointer types
* **Semantic analysis:** Symbol tables, type checking, scope resolution, and control flow analysis
* **Code generation:** LLVM IR generation, optimization, and target code generation
* **Standard library:** Comprehensive Arabic standard library with I/O, math, and string functions

### 🎯 Project Maturity

* **Core compiler:** Production-ready with comprehensive testing and documentation.
* **Arabic localization:** Complete Arabic error messages and keyword support.
* **Documentation:** Extensive documentation with API references and examples.

### Roadmap

* **Current Status (After Priority 4 Completion):**
    1. **✅ Parser and AST implementation complete:** Modern recursive descent parser with unified AST system
    2. **✅ Function system complete:** Full support for function definitions, parameters, and calls
    3. **✅ Core language features complete:** Variables, control flow, expressions, and statements
    4. **📋 Ready for Priority 5:** Advanced language features and semantic analysis
* **Next Phase Goals (Priority 5 and Beyond):**
    1. **Advanced type system:** Implement arrays, structs, unions, enums, and pointers
    2. **Semantic analysis engine:** Symbol tables, type checking, and scope resolution
    3. **Code generation backend:** LLVM IR generation and optimization integration
    4. **Standard library development:** Localized Arabic standard library functions

## Project Structure

The project is organized in a modular way:

```text
baa/
├── cmake/                  # Custom CMake modules
├── docs/                   # Documentation
├── include/                # Public header files (organized by component)
│   └── baa/
├── src/                    # Source code (organized by component)
│   ├── CMakeLists.txt      # Adds subdirectories for components
│   ├── analysis/           # Semantic analysis and flow analysis
│   ├── ast/                # New AST implementation
│   ├── codegen/            # Code generation
│   ├── compiler.c          # Core compiler logic
│   ├── lexer/              # Lexical analyzer
│   ├── main.c              # Main executable entry point
│   ├── operators/          # Operators
│   ├── parser/             # New parser implementation
│   ├── preprocessor/       # Preprocessor
│   ├── types/              # Types
│   └── utils/              # Utilities
├── tests/                  # Unit and integration tests
└── tools/                  # Standalone executable helper tools
```

*For a more detailed visual structure, see [Project Structure](docs/project_structure.md).*

## Documentation

### 📚 Complete Documentation

For comprehensive documentation, visit:

* **[Documentation Guide](docs/_navigation/index.md)** - Complete documentation index
* **[Quick Start Guide](docs/00_OVERVIEW/QUICK_START.md)** - Get started with Baa in 5 minutes
* **[Current Status Summary](docs/00_OVERVIEW/CURRENT_STATUS.md)** - Current implementation status

### 🚀 Quick Links

* **[Language Specification](docs/01_LANGUAGE_SPECIFICATION/LANGUAGE_OVERVIEW.md)** - Complete Baa language specification
* **[Arabic Features](docs/01_LANGUAGE_SPECIFICATION/ARABIC_FEATURES.md)** - Arabic language support details
* **[Architecture Overview](docs/02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)** - Compiler architecture overview
* **[Development Guide](docs/03_DEVELOPMENT/CONTRIBUTING.md)** - Building and contributing guide
* **[Roadmap](docs/04_ROADMAP/ROADMAP_OVERVIEW.md)** - Future development plans

## Building from Source

### Prerequisites

* CMake (3.20 or newer)
* C11-compatible C compiler (Clang-cl for Windows is used in CI)
* Git
* (Optional) LLVM development libraries (for LLVM backend)

### Build Steps

**Out-of-source builds are required.**

1. Clone the repository:

    ```bash
    git clone <repository-url>
    cd baa
    ```

2. Create build directory:

    ```bash
    mkdir build
    cd build
    ```

3. Configure with CMake (from `build` directory):
    * **Windows (with Clang-cl from LLVM tools):**

        ```bash
        cmake -G "Ninja" -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ..
        ```

        *(Adjust `clang-cl.exe` path as needed.)*
    * **Linux/macOS (using system default C compiler, e.g., GCC or Clang):**

        ```bash
        cmake -G "Ninja" ..
        ```

    * **To enable LLVM backend (if LLVM is installed):**
        Add `-DUSE_LLVM=ON` to your cmake command. You may need `-DLLVM_DIR=/path/to/llvm/lib/cmake/llvm` if CMake can't find LLVM automatically.

4. Build the project:

    ```bash
    cmake --build .
    # Or, if using Ninja generator:
    # ninja
    ```

    Executables will be in the `build` directory (e.g., `baa.exe`, `baa_lexer_tester.exe`).

### Running the Tools

* **Test the preprocessor:**

    ```bash
    # After building:
    ./build/tools/baa_preprocessor_tester <path/to/your/file.baa>
    ```

* **Test the lexer:**

    ```bash
    # After building:
    ./build/tools/baa_lexer_tester <path/to/your/file.baa>
    ```

    (Output is now written to `lexer_test_output.txt` in the current working directory)

* **Test the parser (when available):**

    ```bash
    # After building:
    ./build/tools/baa_parser_tester <path/to/your/file.baa>
    ```

## Language Highlights (Current and Planned)

* **Arabic syntax:** Keywords, identifiers, and planned support for more Arabic constructs.
* **Preprocessor:** C99-compatible features with Arabic directives.
* **Type system:** Static typing with basic C types and Arabic names.
* **Literals:**
  * Arabic-Indic numeral support (`٠-٩`).
  * Arabic decimal separator (`٫`).
  * Arabic exponent marker `أ` for decimal numbers (replaces `e`/`E`).
  * Arabic suffixes for integers (`غ`, `ط`, `طط`) and decimals (`ح`).
  * Custom Baa Arabic escape sequences (e.g., `\س`, `\م`, `\يXXXX`) for strings and characters; C-style escapes like `\n`, `\uXXXX` are not supported.
  * Various string formats (normal, multiline `"""..."""`, raw `خ"..."`).
* **Statement terminator:** Uses `.` (period) instead of `;`.

### Example Program

```baa
// program.baa
#تضمين <مكتبة_افتراضية_للطباعة> // Assuming a standard Baa print library

#تعريف EXIT_SUCCESS 0

// Baa uses C-style function declarations
عدد_صحيح رئيسية() {
    اطبع("!مرحباً بالعالم"). // Default print function
    إرجع EXIT_SUCCESS.
}
```

## Quick Start

### Requirements

- **CMake 3.20+**
- **C11-compatible compiler** (GCC 9+, Clang 10+, MSVC 2019+)
- **LLVM 15+ (optional)** - for code generation

### Building

```bash
# Clone the repository
git clone https://github.com/OmarAglan/baa.git
cd baa

# Create build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build
cmake --build .

# Run tests
ctest
```

### Quick Example

```baa
// Simple Baa program example
#تضمين <stdio.h>

دالة عدد_صحيح الرئيسية() {
    عدد_صحيح العدد = ١٠.
    إذا (العدد > ٥) {
        طباعة("العدد أكبر من خمسة\س").
    }
    إرجع ٠.
}
```

## Contributing

Contributions are welcome! Please refer to `docs/development.md` for coding standards, build system details, and component implementation guidelines. Key areas for contribution currently include semantic analysis and code generation.

For contributing guidelines, see:

- **[Contributing Guide](docs/03_DEVELOPMENT/CONTRIBUTING.md)** - Development workflow and standards
- **[Architecture Overview](docs/02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)** - Understanding the codebase
- **[Current Status](docs/00_OVERVIEW/CURRENT_STATUS.md)** - What's implemented and what's needed

## License

This project is licensed under the MIT License - see the [LICENSE](./LICENSE) file for details.

## Support

- **Documentation:** [docs/](docs/)
- **Issues:** [GitHub Issues](https://github.com/OmarAglan/baa/issues)
- **Discussions:** [GitHub Discussions](https://github.com/OmarAglan/baa/discussions)

---

**Baa Programming Language - Arabic Programming for the Future** 🚀
