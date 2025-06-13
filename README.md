# B (باء) Programming Language

B (باء) is a programming language designed to support Arabic syntax while maintaining conceptual alignment with C features. It allows developers to write code using Arabic keywords and identifiers. The project aims to provide a complete compiler toolchain, including a preprocessor, lexer, parser, semantic analyzer, and code generator.

## Current Status (Reflects Build System v0.1.26.0 / Preprocessor v0.1.26.0 / Lexer conceptual v0.1.21.0)

The project is actively under development. Key components and their status:

### Core Features & Status

* **Build System (CMake):**
  * **Refactored:** Now uses a modular, target-centric approach. Component libraries (utils, types, lexer, preprocessor, etc.) are built as static libraries and linked explicitly.
  * **Out-of-Source Builds Enforced.**
  * **Common Compile Definitions:** Managed via an interface library `BaaCommonSettings` (defined in `cmake/BaaCompilerSettings.cmake`).
  * LLVM integration is conditional and handled by the `baa_codegen` library.

* **Preprocessor (`src/preprocessor/`):** - *Actively Developed & Largely Functional*
  * Handles file inclusion (`#تضمين`) with relative and standard path resolution, circular include detection.
  * Supports macro definitions (`#تعريف`) including object-like, function-like (with parameters, stringification `#`, token pasting `##`, variadic arguments `وسائط_إضافية`/`__وسائط_متغيرة__`), and rescanning of expansion results.
  * **C99-Compliant Macro Redefinition Checking:** Intelligent comparison with whitespace normalization, warnings for incompatible redefinitions, errors for predefined macro redefinitions, silent acceptance of identical redefinitions.
  * Handles `#الغاء_تعريف` (undefine).
  * Implements conditional compilation (`#إذا`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`) with expression evaluation (supports arithmetic, comparison, logical, bitwise operators, decimal/hex/binary literals, and the `معرف` operator).
  * Provides predefined Arabic macros (`__الملف__`, `__السطر__` as integer, `__التاريخ__`, `__الوقت__`, `__الدالة__` placeholder, `__إصدار_المعيار_باء__`).
  * Supports `#خطأ` and `#تحذير` directives.
  * Input file encoding detection (UTF-8 default, UTF-16LE with BOM). Outputs UTF-16LE `wchar_t*` stream.
  * Comprehensive error recovery system with multi-error reporting and smart synchronization.
  * *See `docs/PREPROCESSOR_ROADMAP.md` and `CHANGELOG.md` for latest features and known issues.*

* **Lexer (`src/lexer/`):** - *Actively Developed & Largely Functional*
  * Tokenizes preprocessed UTF-16LE stream.
  * Supports Arabic/English identifiers, Arabic-Indic digits, Arabic keywords.
  * Tokenizes whitespace, newlines, and all comment types (`//`, `/*...*/`, `/**...*/`) as distinct tokens.
  * Comment tokens have lexemes containing only their content (delimiters excluded).
  * Handles numeric literals:
    * Integers: decimal, hex `0x`, binary `0b`, underscores, Arabic-Indic digits. Arabic suffixes (`غ`, `ط`, `طط`) are tokenized.
    * Floats: decimal point (`.` or `٫`), Arabic exponent marker `أ` (English `e`/`E` not supported), underscores, Arabic-Indic digits. Arabic float suffix `ح` tokenized.
  * Handles string (`"..."`, multiline `"""..."""`, raw `خ"..."`) and character (`'...'`) literals.
  * Processes **Baa-specific Arabic escape sequences** (e.g., `\س` for newline, `\م` for tab, `\يXXXX` for Unicode, `\هـHH` for hex byte). Standard C escapes like `\n`, `\t`, `\uXXXX` are **not** supported.
  * String and character literal tokens have lexemes containing the processed content (escapes resolved).
  * Accurate line/column tracking and error reporting for lexical errors.
  * *See `docs/LEXER_ROADMAP.md` for planned enhancements and known issues (e.g., C99 hex floats, deeper Unicode for identifiers).*

* **Parser (`src/parser/`):** - **Under Redesign/Re-implementation**
  * The previous parser implementation has been removed to facilitate a complete redesign.
  * **(In Progress - v0.1.19.0)** Core parser structure (token handling, error reporting) implemented.
  * **(Planned)** Will transform the token stream from the lexer into an Abstract Syntax Tree (AST).
  * **(Planned)** Will use a recursive descent approach and focus on syntactic validation, deferring semantic checks.
  * *Refer to `docs/PARSER.md` and `docs/PARSER_ROADMAP.md` for new design plans.*

* **Abstract Syntax Tree (AST):** - **Under Redesign/Re-implementation**
  * The previous AST implementation has been removed.
  * **(In Progress - v0.1.18.0)** Core AST types (`BaaNode`, `BaaSourceSpan`, `BaaNodeKind`) and basic literal expression nodes implemented.
  * **(Planned)** Will provide a structured representation of the parsed code.
  * **(Planned)** Node types for expressions, statements, declarations, etc., with standardized memory management.
  * *Refer to `docs/AST.md` and `docs/AST_ROADMAP.md` for new design plans.*

* **Type System (`src/types/`):** - *Core Implemented*
  * Basic types implemented (`عدد_صحيح` int, `عدد_حقيقي` float, `حرف` char, `منطقي` bool, `فراغ` void).
  * Basic type checking and conversion rules defined.
  * Array type support (`BAA_TYPE_ARRAY`) exists at the type system level.

* **Operators (`src/operators/`):** - *Core Implemented*
  * Definitions for arithmetic, comparison, logical, and assignment operators.
  * Operator precedence table defined.
  * Basic type validation for operators.

* **Analysis (`src/analysis/`):** - *Initial Placeholders / AST Dependent*
  * `flow_analysis.c` contains stubs for control flow analysis, previously dependent on the old AST.
  * **(Planned)** Semantic analysis, including symbol table management, name resolution, full type checking, and advanced flow analysis, will be developed to work with the new AST.
  * *See `docs/SEMANTIC_ANALYSIS.md` and `docs/SEMANTIC_ANALYSIS_ROADMAP.md`.*

* **Code Generation (`src/codegen/`):** - *Basic Stubs / LLVM Integration Conditional*
  * LLVM backend is optional (controlled by `USE_LLVM` CMake option).
  * If LLVM is disabled or not found, a stub backend (`llvm_stub.c`) is used.
  * Actual LLVM IR generation from an AST is **pending** the new Parser/AST implementation and further development of `llvm_codegen.c`.
  * *See `docs/LLVM_CODEGEN.md` and `docs/LLVM_CODEGEN_ROADMAP.md`.*

* **Utilities (`src/utils/`):** - *Implemented*
  * Memory management wrappers (`baa_malloc`, `baa_free`, `baa_strdup`).
  * String handling utilities.
  * Basic file I/O helpers (including UTF-16LE support for Baa sources).

### Overall Project Status

* **What's Working Well:**
  * **Preprocessor:** Handles most C99-level features with Arabic syntax.
  * **Lexer:** Robust tokenization of Baa syntax, including good Arabic numeral and string support (with new Arabic escapes conceptually).
  * **Core Types & Operators:** Foundational system in place.
  * **CMake Build System:** Modular and correctly links components.
* **What's Actively Being Rebuilt/Designed:**
  * **Parser** (Core structure in place, detailed parsing rules in progress).
  * **Abstract Syntax Tree (AST)** (Core structure and some nodes in place, ongoing).
* **What's Planned/Blocked:**
  * **Semantic Analysis** (depends on new AST).
  * **Code Generation** (LLVM IR from AST, depends on new AST & Semantic Analysis).
  * Comprehensive **Standard Library**.
* **General:**
  * Error reporting is being continuously improved for precision and clarity.
  * Full Arabic localization of all error messages and tool outputs is ongoing.

### Roadmap

* **Short-term Goals (towards 0.2.0 and beyond):**
    1. **Critical: Continue implementation of new Parser and AST** based on new designs. This is the current main focus.
    2. Resolve identified Lexer bugs and fully test new escape/exponent features.
    3. Begin foundational Semantic Analysis once the new AST is more complete.
    4. Develop Code Generation (LLVM) based on the new, semantically-analyzed AST.
* *For a high-level overview, see `docs/roadmap.md`. For component-specific details, refer to `LEXER_ROADMAP.md`, `PARSER_ROADMAP.md`, `AST_ROADMAP.md`, `PREPROCESSOR_ROADMAP.md`, etc.*

## Project Structure

The project is organized modularly:

```

baa/
├── cmake/                  # Custom CMake modules (e.g., BaaCompilerSettings.cmake)
├── docs/                   # Documentation
├── include/                # Public header files (organized by component)
│   └── baa/
├── src/                    # Source code (organized by component)
│   ├── CMakeLists.txt      # Adds component subdirectories
│   ├── analysis/
│   ├── ast/                  # New AST implementation
│   ├── codegen/
│   ├── compiler.c          # Core compiler logic (part of baa_compiler_lib)
│   ├── lexer/
│   ├── main.c              # Main executable entry point
│   ├── operators/
│   ├── parser/             # New Parser implementation
│   ├── preprocessor/
│   ├── types/
│   └── utils/
├── tests/                  # Unit and integration tests
└── tools/                  # Standalone utility executables

```

*For a more detailed visual structure, see `docs/project_structure.md`.*

## Building from Source

### Prerequisites

* CMake (3.20 or higher)
* A C11 compliant C compiler (Clang-cl is used for Windows in CI)
* Git
* (Optional) LLVM development libraries (for the LLVM backend)

### Build Steps

It is **required** to perform an out-of-source build.

1. Clone the repository:

    ```bash
    git clone <repository-url>
    cd baa
    ```

2. Create a build directory:

    ```bash
    mkdir build
    cd build
    ```

3. Configure using CMake (from the `build` directory):
    * **Windows (with Clang-cl from LLVM tools):**

        ```bash
        cmake -G "Ninja" -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ..
        ```

        *(Adjust path to `clang-cl.exe` as needed.)*
    * **Linux/macOS (using system default C compiler, e.g., GCC or Clang):**

        ```bash
        cmake -G "Ninja" ..
        ```

    * **To enable the LLVM backend (if LLVM is installed):**
        Add `-DUSE_LLVM=ON` to your cmake command. You might need `-DLLVM_DIR=/path/to/llvm/lib/cmake/llvm` if CMake can't find LLVM automatically.

4. Build the project:

    ```bash
    cmake --build .
    # Or, if using Ninja generator:
    # ninja
    ```

    Executables will be in the `build` directory (e.g., `baa.exe`, `baa_lexer_tester.exe`).

### Running Tool Executables

* **Preprocessor Tester:**

    ```bash
    # After building:
    ./build/tools/baa_preprocessor_tester <path/to/your/file.baa>
    ```

* **Lexer Tester:**

    ```bash
    # After building:
    ./build/tools/baa_lexer_tester <path/to/your/file.baa>
    ```

    (Output is now written to `lexer_test_output.txt` in the current working directory)

* **Parser Tester (when available):**

    ```bash
    # After building:
    ./build/tools/baa_parser_tester <path/to/your/file.baa>
    ```

## Language Highlights (Current & Planned)

* **Arabic Syntax:** Keywords, identifiers, and planned support for more Arabic constructs.
* **Preprocessor:** C99-compliant features with Arabic directives.
* **Type System:** Static typing with basic C types and Arabic names.
* **Literals:**
  * Support for Arabic-Indic digits (`٠-٩`).
  * Arabic decimal separator (`٫`).
  * Arabic exponent marker `أ` for floats (replaces `e`/`E`).
  * Arabic suffixes for integers (`غ`, `ط`, `طط`) and floats (`ح`).
  * Baa-specific Arabic escape sequences (e.g., `\س`, `\م`, `\يXXXX`) for strings and characters; C-style escapes like `\n`, `\uXXXX` are not supported.
  * Various string literal formats (regular, multiline `"""..."""`, raw `خ"..."`).
* **Statement Terminator:** Uses `.` (dot) instead of `;`.
* *For full details, see `docs/language.md` and `docs/arabic_support.md`.*

### Example Program

```baa
// program.baa
#تضمين <مكتبة_افتراضية_للطباعة> // Assuming a standard Baa print library

#تعريف EXIT_SUCCESS 0

// Baa uses C-style function declarations
عدد_صحيح رئيسية() {
    اطبع("!مرحباً بالعالم"). // Hypothetical print function
    إرجع EXIT_SUCCESS.
}
```

## Contributing

Contributions are welcome! Please refer to `docs/development.md` for coding standards, build system details, and component implementation guidelines. Key areas for contribution currently include the Parser and AST redesign and implementation, and resolving identified lexer bugs.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
