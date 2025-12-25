# Changelog

## [0.0.9] - Completed
- **Math:** Added Multiplication (`*`), Division (`/`), and Modulo (`%`).
- **Logic:** Added Relational Operators: `<`, `>`, `<=`, `>=`.
- **Parser:** Implemented Operator Precedence (PEMDAS) to handle complex expressions correctly (e.g., `1 + 2 * 3` is `7`, not `9`).

## [0.0.8] - Completed
- **Major Architecture Shift:** Program structure changed from a linear script to a list of Declarations (Functions & Globals).
- **Functions:** Added Function Definitions (`صحيح func(...) {...}`) and Function Calls (`func(...)`).
- **Scoping:** Implemented Global vs Local variable scope.
- **Entry Point:** Added detection for `الرئيسية` (Main) function as the mandatory entry point.
- **Windows x64 ABI:** Implemented compliant Stack Frames, Register Argument Passing (RCX, RDX, R8, R9), and Shadow Space allocation.
- **Fix:** Resolved issue where global variables were ignoring initializers.

## [0.0.7] - Completed
- Added **Loops**: `طالما` (While loop).
- Added **Variable Re-assignment**: `x = 5.` (updating existing variables).
- Implemented loop code generation using label jumps.

## [0.0.6] - Completed
- Added `إذا` (If) statement.
- Added Block Scopes `{ ... }`.
- Added Comparison Operators (`==`, `!=`).
- Implemented Label generation and Conditional Jumps in Codegen.
- Comprehensive Documentation Update (Internals & API).

## [0.0.5] - Completed
- **Breaking:** Renamed `رقم` to `صحيح` (int) to align with C types.
- Added support for Single Line Comments (`//`).

## [0.0.4] - Completed
- Added `رقم` (Int) type keyword. 
- Added Variable Declaration (`رقم name = val.`).
- Added Variable Usage in expressions.

## [0.0.3] - Completed
- Added `اطبع` (Print) statement.
- Support for multiple statements in a program.
- Integration with C Standard Library (`printf`).

## [0.0.2] - Completed
- Added support for Arabic Numerals (٠-٩).
- Added support for Addition (+) and Subtraction (-).

## [0.0.1] - Completed
- Initial release.
- Compiles `إرجع <number>.` to executable.