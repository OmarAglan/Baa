# B (باء) Programming Language

B (باء) is a programming language designed to support Arabic syntax while maintaining full compatibility with K&R C features. It allows developers to write code using Arabic keywords and identifiers while following established C programming patterns.

## Current Status (v0.1.10.0)

The project now supports:

### Core Features

- **Preprocessor Directives:**
  - `#تضمين <...>` and `#تضمين "..."` (Include files)
  - `#تعريف NAME ...` (Parameterless and function-like macro definition/substitution, **including rescanning of expansion results**).
  - `#الغاء_تعريف NAME` (Undefine macro)
  - Conditional compilation (`#إذا_عرف`, `#إذا_لم_يعرف`, `#إذا`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`) (using `معرف` for `defined`)
  - Stringification (`#`) and Token Pasting (`##`) operators in macros.
  - Predefined Arabic macros: `__الملف__` (FILE), `__السطر__` (LINE - expands to integer), `__التاريخ__` (DATE), `__الوقت__` (TIME). (Planned: `__الدالة__` for `__func__`, `__إصدار_المعيار_باء__` for Baa version).
  - Variadic Macros (using `وسائط_إضافية` for `...`, and `__وسائط_متغيرة__` for `__VA_ARGS__`). - *[Implemented]*
  - Additional Predefined Macros: `__الدالة__` (expands to placeholder `L"__BAA_FUNCTION_PLACEHOLDER__"`), `__إصدار_المعيار_باء__` (expands to `10010L`). - *[Implemented]*
  - Other Standard Directives (Planned: `#خطأ`, `#تحذير`, `#سطر`, `#براغما`, and `أمر_براغما` operator).

- **Basic Type System (نظام الأنواع الأساسي):** (K&R C compatibility with Arabic keywords)
  - `عدد_صحيح` (int) - 32-bit integer
  - `عدد_حقيقي` (float) - 32-bit float
  - `حرف` (char) - 16-bit UTF-16 character
  - `منطقي` (boolean) - Logical true/false values (`صحيح`, `خطأ`)
  - `فراغ` (void) - No value type
  - (Planned: `عدد_صحيح_طويل_جدا` for long long int)

- **Core Operator System (نظام المعاملات الأساسي):**
  - Arithmetic operators (+, -, *, /, %)
  - Comparison operators (==, !=, <, >, <=, >=)
  - Assignment operator (=)
  - Compound assignment operators (+=, -=, *=, /=, %=)
  - Increment/decrement operators (`++`, `--`)
  - Logical operators (`&&`, `||`, `!`)
  - Type checking and validation
  - Arabic operator names

- **Control Flow Structures (بنى التحكم في التدفق):**
  - `إذا`/`وإلا` (if/else)
  - `طالما` (while)
  - `لكل` (for - uses C-style semicolons internally)
  - `إرجع` (return)
  - `توقف` (break)
  - `أكمل` (continue)
  - `اختر`/`حالة`/`افتراضي` (switch/case/default)
  - (Planned: `افعل` for do-while, `تعداد` for enum)

- **Function System (نظام الدوال):** (Uses C-style declarations: `return_type name(params)`)
  - Regular function parameters
  - Optional parameters (with default values)
  - Rest parameters (variable argument lists)
  - Named arguments support
  - Method vs. function distinction

- Literals
  - **Numeric Literals:**
    - Integers: Decimal (Western `0-9` & Arabic-Indic `٠-٩`), Binary (`0b`/`0B`), Hexadecimal (`0x`/`0X`). (Planned: Arabic suffixes like `غ` for unsigned, `ط` for long, `طط` for long long).
    - Floats: Using `.` or Arabic `٫` as decimal separator. Scientific notation (using `أ` as exponent marker). (Planned: Arabic suffix `ح` for float).
    - Underscores (`_`) supported as separators in all number types.
    - Examples: `123`, `١٢٣غ`, `0b1010طط`, `0xFACEط`, `1_000_000`, `3.14ح`, `٣٫١٤`, `1.5أ-2`.
  - **String Literals:** `"..."` (Planned: Arabic escapes like `\س`, `\م` using `\` as escape char).
  - **Character Literals:** `'...'` (Planned: Arabic escapes like `\س`, `\م` using `\` as escape char).
  - **Boolean Literals:** `صحيح` (true), `خطأ` (false).
  - (Planned: `تعداد` keyword for enumerations).
  - (Planned: Struct/union member access using `::` for direct and `->` for pointer).

- Arabic File Support
  - Native `.ب` extension
  - UTF-16LE encoding support
  - Arabic file naming
  - Build system integration

For detailed information about Arabic support, see [Arabic Support Documentation](docs/arabic_support.md).

### Project Status Overview

#### What's Working

- **Preprocessor**: Handles includes (`#تضمين`), object-like and function-like macros (`#تعريف` with parameters, including variadic macros using `وسائط_إضافية`/`__وسائط_متغيرة__`, `#`, `##`, and **rescanning**), undefines (`#الغاء_تعريف`), conditional compilation (including expression evaluation for `#إذا`/`#وإلا_إذا`), predefined Arabic macros (`__الملف__`, `__السطر__` (as integer), `__التاريخ__`, `__الوقت__`). Error reporting is unified and provides original source locations (file, line, column).
- **Core Architecture**: Well-defined architecture with clear separation of concerns
- **Type System**: Basic types including Boolean, type conversion rules, and type checking
- **AST (Abstract Syntax Tree)**: Comprehensive node structure, program, function nodes, enhanced parameter handling
- **Lexer**: UTF-16LE file processing (from preprocessor output). Modularized structure. Robust token recognition for keywords, identifiers (Arabic/English), types, boolean literals, all operators. Advanced numeric literal scanning (Arabic-Indic digits, binary/hex prefixes, underscores, Arabic decimal separator `٫`, scientific notation). String and character literal scanning with standard and Unicode escapes. Accurate line/column tracking and error reporting.
- **Parser**: Recursive descent implementation, expression parsing with precedence, statement parsing
- **Utils**: Memory management, string handling, error infrastructure
- **Preprocessor integration with include and basic macro support.**
- **Additional Predefined Macros**: `__الدالة__` (as placeholder) and `__إصدار_المعيار_باء__` are implemented in the preprocessor.

#### What's Not Working

- **Code Generation**: LLVM integration is incomplete, mostly placeholder code
- **Parser Integration**: Individual components exist but full integration is incomplete
- **Standard Library**: Defined but not implemented
- **Testing Infrastructure**: Framework defined but implementation is limited

#### What Needs Improvement

- **Arabic Support**: RTL text handling, more comprehensive error messages
- **Error Handling**: More detailed messages, better recovery mechanisms
- **Documentation**: Generally updated and consolidated. Specific component details may need further review.
- **Build System**: Cross-platform support, dependency management

#### What Needs Fixing

- **Lexer Issues**: UTF-16LE handling, Arabic character recognition
- **Parser Issues**: Expression precedence, statement termination, control flow parsing
- **Memory Management**: Potential leaks, inconsistent allocation
- **Integration Issues**: Component integration, compilation pipeline gaps

### Changelog

**Added:**

- Boolean type (منطقي) with literals صحيح (true) and خطأ (false)
- Compound assignment operators (+=, -=, *=, /=, %=)
- Increment/decrement operators (++, --)
- Enhanced function parameter handling (optional, rest, named arguments)
- Method vs. function distinction in AST
- Enhanced parser debug output for better troubleshooting
- Test files for Arabic program parsing
- UTF-8 support for Arabic identifiers in the parser
- LLVM code generation infrastructure (placeholder)
- Improved error handling system (including unified preprocessor error reporting)

**Changed:**

- Refactored parser functions for improved error handling
- Simplified number parsing (temporarily removed decimal support)
- Improved error messages with more context
- Updated project structure to match architecture documentation
- Focus on Parser Enhancements, Code Generation, Lexer Enhancements, and AST Improvements.
- *See [docs/roadmap.md](docs/roadmap.md) for the high-level plan and component-specific roadmaps (e.g., `docs/LEXER_ROADMAP.md`, `docs/PREPROCESSOR_ROADMAP.md`) for details.*

**Fixed:**

- Memory leaks in parser error handling
- Function declaration parsing
- Identifier parsing for UTF-16LE characters
- File handling for UTF-16LE encoded files

### Roadmap

**Completed Features:**

- Core components (AST, Type System, Operators, Memory Management, Error Handling)
- Advanced operators (compound assignment, increment/decrement)
- Boolean type support
- Enhanced function parameter system
- Arabic support (UTF-16LE encoding, Arabic identifiers, Basic RTL support, Arabic error messages)
- Documentation (Language specification, Arabic support guide, C comparison, Architecture overview, Component documentation)

**In Progress:**

- Parser Development (Grammar definition, Token handling, Error recovery, Source location tracking)
- Code Generation (LLVM integration planning, Basic code emission, Debug information support)
- Standard Library (Basic I/O functions, String manipulation, Memory management, File operations)

**Short-term Goals (0.2.0):**

- Focus on Parser Enhancements, Code Generation, Lexer Enhancements, and AST Improvements.
- *See [docs/roadmap.md](docs/roadmap.md) for the high-level plan and component-specific roadmaps (e.g., `docs/LEXER_ROADMAP.md`) for details.*

### Project Structure

```
baa/
├── include/
│   └── baa/           # Public header files
│       ├── ast/       # AST definitions
│       ├── lexer/     # Lexer interface
│       ├── parser/    # Parser interface
│       ├── preprocessor/ # Preprocessor interface
│       ├── types/     # Type system interface
│       ├── utils/     # Utility functions
│       └── codegen/   # Code generation interface
├── src/
│   ├── ast/          # AST implementation
│   ├── lexer/        # Lexer implementation
│   ├── parser/       # Parser implementation
│   ├── preprocessor/ # Preprocessor implementation
│   ├── types/        # Type system implementation
│   ├── operators/    # Operator system implementation
│   ├── codegen/      # Code generation (incomplete)
│   ├── utils/        # Utility functions
│   ├── control_flow/ # Control flow implementation
│   ├── compiler.c    # Main compilation logic
│   └── main.c        # Executable entry point
├── tests/
│   ├── unit/         # Unit tests (limited)
│   └── integration/  # Integration tests (placeholder)
└── docs/             # Documentation
    ├── preprocessor.md     # Preprocessor documentation
    ├── PREPROCESSOR_ROADMAP.md # Detailed Preprocessor roadmap
    ├── architecture.md     # System architecture overview & component status
    ├── language.md         # Language specification
    ├── project_structure.md # Detailed directory layout (Arabic)
    ├── roadmap.md          # High-level development roadmap
    ├── lexer.md            # Lexer structure & usage
    ├── LEXER_ROADMAP.md    # Detailed Lexer roadmap
    ├── AST_STRUCTURE.md    # AST structure details
    ├── AST_ROADMAP.md      # Detailed AST roadmap
    ├── PARSER_STRUCTURE.md # Parser structure details
    ├── PARSER_ROADMAP.md   # Detailed Parser roadmap
    ├── arabic_support.md   # Arabic support details
    ├── c_comparison.md     # Comparison with C
    ├── development.md      # Developer guide
    └── ...                 # Other specific docs
```

## Building from Source

### Prerequisites

- CMake 3.20 or higher
- K&R C compliant compiler
- Git for version control
- LLVM development libraries (optional, for code generation)
- Unicode support libraries

### Build Steps

```bash
git clone <repository-url>
cd baa
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### Preprocessor Tester

A standalone tool is available to test the preprocessor in isolation:

```bash
# After building the project:
./build/bin/baa_preprocessor_tester <path/to/your/file.baa>
```

Note: The preprocessor currently expects input files to be encoded in UTF-16LE with a BOM.

## Features

### Type Safety

- Strong type checking
- Explicit conversion rules
- Error type for invalid operations
- UTF-16 support for Arabic text

### Operator System

- Type-safe operations
- Arabic operator names
- K&R C operator precedence
- Comprehensive error handling

### Documentation

- Detailed Arabic error messages
- Comprehensive documentation in both English and Arabic
- Examples and usage guides
- Development guidelines

## File Extensions

The Baa programming language supports two file extensions:

- `.ب` - The primary Arabic file extension
- `.baa` - Alternative Latin file extension

### Example Program

```baa
#تضمين <مكتبة_طباعة> // Assuming a Baa standard library header
#تعريف EXIT_SUCCESS 0

// مثال برنامج بسيط بلغة باء
// Baa uses C-style function declarations
عدد_صحيح رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع EXIT_SUCCESS.
}
```

## Known Issues and Limitations

- Code generation is not fully implemented yet
- Complex language constructs may not parse correctly
- Error messages may not be fully localized
- Limited testing coverage
- UTF-16LE handling has edge cases with certain characters
- Parser error recovery is limited

## Contributing

We welcome contributions! Please see our [development guide](docs/development.md) for details on:

- Code style and standards
- Testing requirements
- Documentation guidelines
- Pull request process

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

Special thanks to:

- The C language designers for their foundational work
- The Arabic programming community for their support and feedback
- All contributors who have helped shape this project
