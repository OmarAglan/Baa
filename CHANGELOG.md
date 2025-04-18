# Changelog

All notable changes to the B (باء) compiler project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- LLVM integration for code generation
  - Basic LLVM IR generation infrastructure
  - Function generation with proper return types
  - Support for multiple target platforms (x86-64, ARM64, WebAssembly)
  - Integration with existing compiler pipeline
  - Arabic error messages for code generation failures
- Enhanced compiler driver
  - Integrated code generation into the compilation pipeline
  - Added support for generating LLVM IR output files
  - Improved error handling with Arabic messages
- New parser components
  - Added control_flow_parser.c for handling control structures
  - Added declaration_parser.c for variable and function declarations
  - Added expression_parser.c for expression parsing
  - Added function_parser.c for function handling
  - Added type_parser.c for type system parsing
- Enhanced AST implementation
  - Added new node types and structures
  - Added visitor interface for AST traversal
  - Added scope management system
  - Added comprehensive statement handling
  - Added literal value handling
- Improved testing infrastructure
  - Added unit tests for lexer functionality
  - Added number parsing tests
  - Added control flow tests
  - Added type and operator tests
  - Added test framework with assertion macros
- Memory management improvements
  - Added baa_malloc/baa_free for consistent memory handling
  - Added baa_strdup/baa_strndup for string operations
  - Added proper memory cleanup in all components

### Changed
- Updated build system
  - Added LLVM package detection
  - Added conditional compilation for LLVM features
  - Reorganized component subdirectories
  - Enhanced test configuration
  - Added memory leak detection
- Improved type system
  - Updated parameter handling from BaaFuncParam to BaaParameter
  - Enhanced type checking and validation
  - Added support for user-defined types
  - Improved type conversion system
- Enhanced error handling
  - Added Arabic error messages
  - Improved error recovery mechanisms
  - Added detailed error context
  - Enhanced error reporting system
- Updated project structure
  - Reorganized source files into logical components
  - Moved test files to appropriate directories
  - Enhanced documentation structure
  - Improved code organization

### Fixed
- Memory management issues
  - Fixed memory leaks in parser functions
  - Fixed string handling in parameter creation
  - Fixed resource cleanup in AST operations
  - Fixed memory management in test framework
- Type system issues
  - Fixed type declarations in parser functions
  - Fixed parameter handling in function declarations
  - Fixed type checking in expressions
  - Fixed type conversion edge cases
- Parser improvements
  - Fixed compilation errors in declaration_parser.c
  - Fixed control flow parsing issues
  - Fixed expression parsing bugs
  - Fixed function parsing problems
- Test framework issues
  - Fixed test result reporting
  - Fixed memory leak detection
  - Fixed test file organization
  - Fixed test execution flow

### Removed
- Removed deprecated components
  - Removed old control_flow.h
  - Removed old parser.h
  - Removed old tokens.h
  - Removed old types.h
- Removed redundant files
  - Removed duplicate test files
  - Removed old documentation files
  - Removed unused header files
  - Removed obsolete test framework files

## [0.1.9.8] - 2025-02-09

### Added
- Complete decimal number parsing implementation
  - Support for both Western (0-9) and Arabic-Indic (٠-٩) digits
  - Support for Western (.) and Arabic (٫) decimal separators
  - Comprehensive error handling with Arabic messages
  - Memory-safe number token management
- Enhanced lexer functionality
  - Added number validation and format checking
  - Implemented overflow protection
  - Added support for preserving original text representation

### Changed
- Updated lexer to handle both integer and decimal numbers
- Enhanced error handling system with detailed Arabic messages
- Improved memory management for number tokens
- Updated build system to include number parser module

### Fixed
- Memory leaks in number parsing
- Decimal point validation
- Overflow handling in large numbers
- Arabic numeral conversion accuracy

## [0.1.9.7] - 2025-02-08

### Added

- Restored decimal number parsing with improved implementation
- Completed control flow parsing implementation
  - Full support for إذا (if) statements
  - Integrated وإلا (else) statements
  - Implemented طالما (while) loops
- Added comprehensive test suite for control flow structures

### Changed

- Updated parser to handle decimal numbers efficiently
- Enhanced error handling for control flow statements
- Improved documentation to reflect completed features

### Fixed

- Edge cases in decimal number parsing
- Control flow statement nesting issues
- Parser error recovery in complex control structures

## [0.1.9] - 2025-02-06

### Added

- Enhanced parser debug output for better troubleshooting
- Added test files for Arabic program parsing
- Added UTF-8 support for Arabic identifiers in parser

### Changed

- Refactored parser functions for better error handling
- Simplified number parsing (temporarily removed decimal support)
- Improved error messages with more context

### Fixed

- Fixed memory leaks in parser error handling
- Fixed function declaration parsing
- Fixed identifier parsing for UTF-8 characters

### Development

- Added more debug output in parse_declaration and parse_program
- Reorganized parser code structure
- Added test cases for basic language features

## [0.1.8] - 2025-02-01

### Added

- Full implementation of AST (Abstract Syntax Tree)
- Complete type system implementation
- Operator system with precedence rules
- Control flow structures (if, while, return)
- Error handling with Arabic message support
- Memory management utilities
- Support for Arabic file extension `.ب`
- UTF-8 encoding support for source files
- Basic compiler infrastructure for Arabic source files
- Example program with Arabic file name (`برنامج.ب`)
- EditorConfig settings for Arabic source files
- `#تضمين` directive for imports
- Updated character type from `محرف` to `حرف`

### Changed

- Removed placeholder implementations
- Updated build system to exclude unimplemented components
- Improved project structure and organization
- Build system now handles Arabic file extensions
- Updated CMake configuration for UTF-8 support
- Enhanced file handling for Arabic source files
- Improved project documentation with Arabic examples
- Refined language specification with proper Arabic keywords
- Updated type system documentation to use `حرف` instead of `محرف`
- Added proper import syntax using `#تضمين`

### Fixed

- Corrected character type naming convention
- Standardized import directive syntax
- Aligned documentation with actual language specification

## [0.1.7] - 2025-02-01

### Added

- Basic control flow structures implementation:
  - إذا/وإلا (if/else) statement support
  - طالما (while) loop support
  - إرجع (return) statement support
- New control_flow.h header with comprehensive control flow API
- Memory-safe statement creation and management functions
- Unit tests for all control flow structures
- Integration with existing build system

### Changed

- Updated CMakeLists.txt to include control flow implementation
- Enhanced project structure with new control flow components
- Updated roadmap to reflect completed control flow milestone

## [0.1.6] - 2025-01-31

### Added

- Basic type system implementation with K&R C compatibility
- Core operator system with arithmetic and comparison operators
- Comprehensive test suite for type system and operators
- Type conversion and validation system
- Arabic operator names and error messages
- UTF-16 support for character type

### Changed

- Updated project structure with src/ and tests/ directories
- Enhanced type safety with strict conversion rules
- Improved error handling in type system

### Documentation

- Added type system implementation details
- Added operator system specifications
- Updated development guide with testing procedures
- Added code examples for type and operator usage

## [0.1.5] - 2025-01-31

### Added

- Comprehensive K&R C feature comparison in `docs/c_comparison.md`
- Detailed implementation roadmap for K&R C compatibility
- Arabic equivalents for all K&R C keywords and operators
- Standard library function mappings with Arabic names

### Changed

- Updated `docs/language.md` to align with K&R C specification
- Updated `docs/development.md` to follow K&R coding style
- Enhanced documentation with UTF-16LE and bidirectional text support details
- Improved error message documentation with Arabic language support

### Documentation

- Added detailed type system documentation
- Added memory model specifications
- Added preprocessor directive documentation
- Added comprehensive code examples
- Updated build system documentation

## [0.1.0] - 2025-01-31

### Added

- Initial project structure and build system
- Basic file reading with UTF-16LE support
- Support for Arabic text in source files
- Modular component architecture:
  - Lexer component with UTF-16 file handling
  - AST component with basic node structure
  - Parser component (placeholder)
  - Code generator component (placeholder)
  - Utils component with error handling and memory management
- Comprehensive test suite:
  - Lexer tests for file handling
  - Utils tests for memory and error handling
- Project documentation:
  - README with build instructions
  - API documentation
  - Example programs
- CMake-based build system with:
  - Cross-platform support
  - Test integration
  - Modular component builds
- Development tools:
  - Automated testing framework
  - Error handling system
  - Memory management utilities
- Initial project setup
- Basic file reading functionality
- UTF-16LE support for Arabic text
- CMake build system configuration
- Project documentation
- Code organization and structure

### Changed

- Organized project structure into logical components
- Improved build system with proper dependency management
- Enhanced file handling with proper Unicode support
- Improved project structure
- Enhanced build system configuration
- Updated documentation

### Fixed

- UTF-16LE file handling and BOM detection
- Memory management in AST operations
- File size calculations for wide character content
- File handling improvements
- Better error messages in Arabic

## [0.0.1] - 2025-01-31

### Added

- Initial release
- Basic file reading capabilities
- Command-line interface
- Simple error reporting

## [Unreleased]

### Planned

- Parser implementation
- Code generation
- Symbol table
- Type system
- Error recovery
- Optimization passes
- Arabic language syntax definition
- Comprehensive test suite expansion
- Documentation in both English and Arabic
