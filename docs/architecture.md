# Baa Language Architecture

## Overview
Baa (باء) is designed with a modular architecture that separates concerns into distinct components. Each component is responsible for a specific aspect of the compilation process, making the system maintainable and extensible.

## Core Components

### 0. Preprocessor (المعالج المسبق)
The initial stage that processes the source file before tokenization:
- **Directive Handling**: Processes directives like `#تضمين` (include) and `#تعريف` (define).
- **File Inclusion**: Merges included files into a single stream.
- **Macro Expansion**: Substitutes defined object-like and function-like macros. Supports stringification (`#`) and token pasting (`##`). Detects recursive expansion.
- **Input Encoding**: Expects UTF-16LE input files (checks BOM).
- **Output**: Produces a single `wchar_t*` stream for the lexer.
- **Circular Include Detection**: Prevents infinite include loops.
- **Conditional Compilation**: Handles `#إذا`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`. Evaluates constant integer expressions for `#إذا`/`#وإلا_إذا` (supports arithmetic, comparison, logical operators, `معرف()`; excludes bitwise ops).
- **Error Reporting**: Reports errors with file path and line number context.
- **Features & Status:**
  - Implemented as a separate stage.
  - Handles `#تضمين` (relative and standard paths).
  - Handles `#تعريف` (object-like and function-like macros, including `#`, `##`, and **rescanning of expansion results**).
  - Handles `#الغاء_تعريف` to undefine macros.
  - Handles conditional compilation (`#إذا_عرف`, `#إذا_لم_يعرف`, `#إلا`, `#نهاية_إذا`, `#إذا`, `#وإلا_إذا`) with expression evaluation (using `معرف` for `defined`; excludes bitwise ops).
  - Detects circular includes and recursive macro expansion (expansion stack prevents direct self-recursion during a single expansion's rescan).
  - Enforces UTF-16LE input.
  - Provides error messages with file/line context.
  - Predefined macros implemented: `__الملف__`, `__السطر__` (as integer), `__التاريخ__`, `__الوقت__`.
  - *Planned:* Bitwise operators in conditional expressions, remaining predefined macros (`__الدالة__`, `__إصدار_المعيار_باء__`), Variadic Macros (`وسائط_إضافية`, `__وسائط_متغيرة__`), other standard directives (`#خطأ`, `#تحذير`, `#سطر`, `#براغما`, `أمر_براغما`), UTF-8 input support, full macro expansion in conditional expressions.

### 1. Lexer
The lexical analyzer responsible for tokenizing source code. It has a modular structure:
  - `lexer.c`: Core dispatch logic (`baa_lexer_next_token`) and helper functions.
  - `token_scanners.c`: Implements specific scanning functions for identifiers, numbers, strings, and characters.
  - `lexer_char_utils.c`: Provides character classification utilities (e.g., for Arabic letters, digits).
- **Token Generation**: Converts source text (output from preprocessor) into a stream of tokens.
- **Unicode Support**: Full support for Arabic characters in identifiers, literals, and keywords. Recognizes Arabic-Indic digits.
- **Source Tracking**: Accurate line and column tracking for tokens and errors.
- **Error Detection**: Identifies lexical errors (e.g., unexpected characters, unterminated literals, invalid escapes) and provides diagnostic messages.
- **Numeric Literal Recognition**:
    - Identifies various number formats: integers (decimal, binary `0b`/`0B`, hexadecimal `0x`/`0X`), floats (using `.` or `٫` as decimal separator), and scientific notation (using `أ` as exponent marker).
    - Supports Arabic-Indic digits (`٠`-`٩`) and Western digits (`0`-`9`) within all parts of numbers.
    - Supports underscores (`_`) as separators for readability in numbers.
    - (Planned: Arabic literal suffixes like `غ`, `ط`, `طط`, `ح`).
    - The lexer's `scan_number` function (in `token_scanners.c`) handles the syntactic recognition and extracts the raw lexeme. The separate `number_parser.c` utility can be used later for converting these lexemes to actual numeric values.
- **Comment Handling**: Skips single-line (`//`, `#`) and multi-line (`/* */`) comments.
- **String/Char Literals**: Handles string (`"..."`) and character (`'...'`) literals. Currently implements standard C escapes. *Planned: Support for Baa-specific Arabic escape sequences (e.g., `\س` for newline, `\م` for tab, `\يXXXX` for Unicode) while retaining `\` as the escape character.*
- **Features & Status:**
  - Core lexer functionality and modular structure are implemented.
  - UTF-16LE encoding is processed (input from preprocessor).
  - Robust token handling for keywords, identifiers, operators, and various literal types.
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
- **Struct/Union/Enum Support**: AST nodes for `بنية` (struct), `اتحاد` (union), `تعداد` (enum) and member access (`::`, `->`).
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
- **Logical**: Uses symbols `&&` (و - AND), `||` (أو - OR), `!` (ليس - NOT).
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
- **Functions**: (Uses C-style declarations, no `دالة` keyword)
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
