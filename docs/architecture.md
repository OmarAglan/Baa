# Baa Language Architecture

## Overview
Baa (باء) is designed with a modular architecture that separates concerns into distinct components. Each component is responsible for a specific aspect of the compilation process, making the system maintainable and extensible.

## Core Components

### 0. Preprocessor (المعالج المسبق)
The initial stage that processes the source file before tokenization:
- **Directive Handling**: Processes directives like `#تضمين` (include) and `#تعريف` (define).
- **File Inclusion**: Merges included files into a single stream.
- **Macro Expansion**: Substitutes defined macros (currently parameterless).
- **Input Encoding**: Expects UTF-16LE input files.
- **Output**: Produces a single `wchar_t*` stream for the lexer.
- **Circular Include Detection**: Prevents infinite include loops.
- **Features & Status:**
  - Implemented as a separate stage.
  - Handles `#تضمين` (relative and standard paths).
  - Handles parameterless `#تعريف` macros with basic text substitution.
  - Handles `#الغاء_تعريف` to undefine macros.
  - Handles basic conditional compilation (`#إذا_عرف`, `#إذا_لم_يعرف`, `#إلا`, `#نهاية_إذا`).
  - Detects circular includes.
  - Enforces UTF-16LE input.
  - *Planned:* Function-like macros, `#if` with expressions, improved error reporting.

### 1. Lexer
The lexical analyzer responsible for tokenizing source code:
- **Token Generation**: Converts source text into tokens
- **Unicode Support**: Full support for Arabic characters and identifiers
- **Source Tracking**: Accurate line and column tracking
- **Error Detection**: Identifies lexical errors and provides diagnostics
- **Advanced Operators**: Support for compound assignments (+=, -=, etc.) and increment/decrement (++, --)
- **Boolean Literals**: Recognition of صحيح (true) and خطأ (false)
- **Number Parsing**: Support for various number formats including integers, floats, scientific notation, binary (`0b`), hex (`0x`), underscores (`_`), and Arabic-Indic digits. Lexer identifies syntax, `number_parser.c` handles value parsing.
- **Comment Handling**: Support for single-line (`//`, `#`) and multi-line (`/* */`) comments (skipped).
- **String/Char Literals**: Handles basic literals with escape sequences (`\n`, `\t`, `\r`, `\0`, `\uXXXX`, etc.).
- **Features & Status:**
  - Fully implemented base lexer functionality.
  - UTF-16LE encoding support assumed from preprocessor.
  - Token handling for keywords, identifiers, operators, literals.
  - Number format recognition (int, float, sci, bin, hex, underscores).
  - String and character literal support with escapes.
  - Source position tracking.
  - Error token generation.
  - *Planned:* Advanced error recovery, documentation comment tokens, multi-line/raw string literals.

### 2. Parser
Transforms tokens into a structured AST:
- **Recursive Descent**: Top-down parsing approach
- **Expression Parsing**: Handles precedence and associativity
- **Statement Parsing**: Converts token sequences into statement nodes
- **Declaration Parsing**: Handles variable and function declarations
- **Type Checking**: Basic type validation during parsing
- **Error Recovery**: Continues parsing after encountering errors
- **Source Location**: Tracks precise locations for error reporting
- **Features & Status:**
  - Implemented with recursive descent approach
  - Expression parsing with precedence
  - Statement parsing for basic control flow (if, while, for, return, switch, break, continue)
  - Declaration parsing for variables and functions
  - Basic type annotation support
  - Error detection and reporting
  - Source location tracking
  - *Planned:* Enhanced error recovery, complete type checking during parsing.

### 3. Abstract Syntax Tree (AST)
The AST module provides the foundation for representing code structure:
- **Node Types**: Comprehensive set of node types for all language constructs
- **Memory Management**: Efficient allocation and deallocation of nodes
- **Tree Operations**: Creation, traversal, and manipulation of the syntax tree
- **Visitor Pattern**: Support for tree traversal and transformation
- **Standardized Structure**: Clear hierarchy with expressions, statements, and declarations
- **Enhanced Function Parameters**: Support for optional parameters, rest parameters, and named arguments *(Note: AST supports this, but parser implementation is currently limited to basic parameters)*.
- **Control Flow Support**: If-else, while loops, for loops, switch/case, break/continue statements
- **Array Support**: Array creation, indexing, and manipulation
- **Features & Status:**
  - Full implementation
  - Node creation and management
  - Tree traversal utilities (Visitor Pattern)
  - Memory management for nodes (clear ownership rules)
  - Support for all basic language constructs
  - Standardized structure (expressions, statements, declarations)
  - Enhanced function parameter handling (optional, rest, named args)
  - Control flow statements (if-else, while, for, switch/case, break, continue)
  - Array creation and indexing
  - Full type system integration

### 4. Type System
A robust type system that supports both C compatibility and Arabic type names:
- **Basic Types**: عدد_صحيح (int), عدد_حقيقي (float), حرف (char)
- **Type Checking**: Static type checking with detailed error messages
- **Type Conversion**: Implicit and explicit type conversion rules
- **Type Validation**: Compile-time type validation
- **Features & Status:**
  - Complete implementation
  - Basic types (int, float, char, etc.)
  - Boolean type (منطقي) with literals (صحيح/خطأ)
  - Type checking and conversion rules
  - Type initialization support
  - Arabic type names support
  - Array type support

### 5. Operators
Operator system with full Arabic support:
- **Arithmetic**: جمع (+), طرح (-), ضرب (*), قسمة (/)
- **Comparison**: يساوي (==), أكبر_من (>), أصغر_من (<)
- **Logical**: و (&&), أو (||), ليس (!)
- **Precedence**: Clear operator precedence rules
- **Extensibility**: Easy addition of new operators
- **Features & Status:**
  - Full implementation
  - Operator precedence table defined
  - Binary and unary operators
  - Assignment operators (=, +=, -=, *=, /=, %=)
  - Increment/decrement operators (++, --)
  - Comparison operators
  - Arabic operator keyword support (planned/partial)

### 6. Control Flow
Control structures with Arabic keywords:
- **Conditions**: إذا (if), وإلا (else)
- **Loops**: طالما (while), من_أجل (for)
- **Functions**: دالة (function)
- **Return**: إرجع (return)
- **Features & Status:**
  - Complete implementation
  - If statements (إذا/وإلا)
  - While loops (طالما)
  - For loops (من_أجل)
  - Switch/case statements
  - Break and continue statements
  - Return statements (إرجع)
  - Basic block scoping
  - Condition handling

### 7. Utils
Utility functions and support features:
- **Error Handling**: Detailed Arabic error messages (planned)
- **Memory Management**: Safe memory allocation and tracking
- **String Handling**: UTF-16LE wide character string operations
- **File Operations**: Source file handling with Arabic support (UTF-16LE reading)
- **Features & Status:**
  - Memory management utilities
  - String utilities (wchar_t based)
  - File handling (reading UTF-16LE)
  - UTF-16LE support functions
  - *Planned:* Error handling with detailed Arabic messages.

### 8. Code Generation (Planned)
Transforms the AST into executable code or intermediate representation.
- **Features & Status:**
  - Not implemented
  - *Planned:* LLVM integration, optimization passes, debug information generation, source mapping.

## Build System
The build system is based on CMake and provides:
- **Component Management**: Each component built as a separate library
- **Test Integration**: Automated testing for each component
- **Cross-Platform**: Support for multiple operating systems
- **Configuration**: Flexible build configuration options

## Testing Framework
Comprehensive testing infrastructure:
- **Unit Tests**: Tests for individual components
- **Integration Tests**: Tests for component interaction
- **Arabic Test Cases**: Test cases with Arabic input
- **Coverage**: Test coverage tracking

## Error Handling
Robust error handling system:
- **Error Types**: Syntax, type, memory, and runtime errors
- **Arabic Messages**: Error messages in Arabic (planned for some areas)
- **Error Recovery**: Graceful error recovery where possible
- **Debug Info**: Detailed debugging information

## Memory Management
Memory management strategy:
- **Allocation Tracking**: Track all memory allocations
- **Garbage Collection**: Future support for garbage collection
- **Memory Safety**: Checks for memory leaks and buffer overflows
- **Resource Cleanup**: Automatic resource cleanup

## Future Extensions
Planned enhancements:
- **Code Generation**: LLVM-based code generator (see section above)
- **Optimizer**: Code optimization passes
- **IDE Integration**: Support for code editors and IDEs
