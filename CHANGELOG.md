# Changelog

All notable changes to the Baa programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Planned (v0.2.7)
- **Constants**: Immutable variables (`ثابت`) and constant checking.
- **Warnings**: Diagnostic warnings for unused variables and dead code.

---
## [0.2.6] - 2026-01-11

### Added
- **Preprocessor Engine** — Integrated directly into the Lexer to handle directives before parsing.
- **Macro Definitions** — Implemented `#تعريف <name> <value>` to define compile-time constants.
- **Conditional Compilation** — Implemented `#إذا_عرف`, `#وإلا`, and `#نهاية` to conditionally include or exclude code blocks.
- **Undefine Directive** — Implemented `#الغاء_تعريف` to remove macro definitions.

### Fixed
- **Codegen State Leak** — Fixed a critical bug where symbol tables, label counters, and loop stacks were not reset when compiling multiple files, causing symbol collisions and invalid assembly generation.

## [0.2.5] - 2026-01-04
### Added
- **Multi-File Compilation**: Support for compiling multiple `.b` files and linking them into a single executable.
- **Function Prototypes**: New syntax `صحيح دالة().` to declare functions without definition (for cross-file usage).
- **Preprocessor**: Implemented `#تضمين` (`#include`) to include header files.
- **File Extensions**: Adopted `.baa` for source files and `.baahd` for headers.

### Changed
- **Defaults**: Examples and documentation now favor `.baa` over `.b`.

### Fixed
- **CLI Arguments**: Fixed crash when running `baa` without arguments.
- **Flags**: Added support for lowercase `-s` (same as `-S`).
- **Updater Logic**: Fixed version comparison to respect SemVer (e.g., 0.2.5 is newer than 0.2.4) instead of blindly flagging inequality as an update.
- **Auto-Update Trigger**: `baa update <file>` no longer triggers the updater; strict `baa update` syntax is now enforced.

## [0.2.4] - 2026-01-04

### Added
- **Semantic Analysis Pass** — Implemented a dedicated validation phase (`src/analysis.c`) that runs before code generation. It checks for:
    - **Symbol Resolution**: Ensures variables are declared before use.
    - **Type Checking**: strictly enforces `TYPE_INT` vs `TYPE_STRING` compatibility in assignments and operations.
    - **Scope Validation**: Tracks global vs local variable declarations.
- **Shared Symbol Definitions** — Moved `Symbol` and `ScopeType` to `src/baa.h` to allow sharing between Analysis and Codegen modules.

### Changed
- **Compiler Pipeline** — Updated `src/main.c` to invoke the analyzer after parsing. Compilation now aborts immediately if semantic errors are found.
- **Code Generator** — Refactored `src/codegen.c` to rely on the shared symbol definitions.

---

## [0.2.3] - 2025-12-29

### Added
- **Windows Installer** — Created a professional `setup.exe` using Inno Setup. It installs the compiler, documentation, and creates Start Menu shortcuts.
- **PATH Integration** — The installer automatically adds Baa to the system `PATH` environment variable, allowing `baa` to be run from any console.
- **Auto Updater** — Added `baa update` command. It checks a remote server for new versions and downloads/installs updates automatically.
- **Update Logic** — Implemented `src/updater.c` using native Windows APIs (`UrlMon`, `WinINet`) to handle downloads without external dependencies like curl.

---

## [0.2.2] - 2025-12-29

### Added
- **Diagnostic Engine** — New error reporting system that displays the filename, line number, column number, and the actual source code line with a pointer (`^`) to the error location.
- **Panic Recovery** — The parser now attempts to recover from syntax errors (by skipping tokens until a statement boundary) to report multiple errors in a single run.
- **Lexer Location** — Lexer now tracks column numbers and filenames.

### Fixed
- **Global Strings** — Fixed a bug where global string variables initialized with string literals would output `(null)` at runtime. They now correctly point to the `.rdata` label.

---

## [0.2.1] - 2025-12-29

### Added
- **Executable Branding** — Added a custom icon (`.ico`) and version metadata to `baa.exe`.
- **Windows Integration** — File properties now show "Baa Programming Language" and version info.

---

## [0.2.0] - 2025-12-29

### Added
- **CLI Driver** — Completely rewritten `main.c` to act as a proper compiler frontend.
- **Flags** — Added support for standard compiler flags:
    - `-o <file>`: Specify output filename.
    - `-S`: Compile to assembly only (skip assembler/linker).
    - `-c`: Compile to object file (skip linker).
    - `--help`: Print usage information.
    - `--version`: Print version and build date.
- **Pipeline** — The compiler now orchestrates `gcc` automatically for assembling and linking.

### Changed
- **Architecture** — Moved from a simple "transpiler script" model to a multi-stage pipeline model.

---

## [0.1.3] - 2025-12-27

### Added
- **Extended If** — Added support for `وإلا` (Else) and `وإلا إذا` (Else If) blocks.
- **Switch Statement** — Implemented `اختر` (Switch), `حالة` (Case), and `افتراضي` (Default) for clean multi-way branching.
- **Constant Folding** — Compiler now optimizes arithmetic expressions with constant operands at compile-time (e.g., `2 * 3 + 4` generates `10` directly).

### Changed
- **Parser** — Enhanced expression parsing to support immediate evaluation of constant binary operations.
- **Codegen** — Improved label management for nested control structures (`if`, `switch`, `loops`).

---

## [0.1.2] - 2025-12-27

### Added
- **Recursion** — Functions can now call themselves recursively (e.g., Fibonacci, Factorial).
- **String Variables** — Introduced `نص` keyword to declare string variables (behaves like `char*`).
- **Loop Control** — Added `توقف` (break) to exit loops immediately.
- **Loop Control** — Added `استمر` (continue) to skip to the next iteration.
- **Type System** — Internal symbol table now strictly tracks `TYPE_INT` vs `TYPE_STRING`.

### Fixed
- **Stack Alignment** — Enforced 16-byte stack alignment in generated x64 assembly to prevent crashes during external API calls and deep recursion.
- **Register Names** — Fixed double-percent typo in register names in `codegen.c`.

---

## [0.1.1] - 2025-12-26 - Structured Data

### Added
- **Arrays** — Declaration (`صحيح قائمة[٥]`), access (`قائمة[٠]`), and assignment (`قائمة[٠] = ١`)
- **For Loop** — `لكل` with Arabic semicolon `؛` separator
- **Postfix Operators** — Increment (`++`) and decrement (`--`)
- **Logic Operators** — `&&` (AND), `||` (OR), `!` (NOT) with short-circuit evaluation

---

## [0.1.0] - 2025-12-26 - Text & Unary

### Added
- **String Literals** — `"text"` support
- **Character Literals** — `'x'` support
- **Unary Minus** — Negative numbers (`-5`)

### Changed
- Updated `اطبع` to support printing strings via `%s`
- Implemented string table generation in codegen

---

## [0.0.9] - 2025-12-26 - Advanced Math

### Added
- **Multiplication** (`*`), **Division** (`/`), and **Modulo** (`%`)
- **Relational Operators** — `<`, `>`, `<=`, `>=`

### Changed
- **Parser** — Implemented operator precedence (PEMDAS) for correct expression evaluation

---

## [0.0.8] - 2025-12-26 - Functions

### Added
- **Function Definitions** — `صحيح func(...) {...}` syntax
- **Function Calls** — `func(...)` syntax
- **Scoping** — Global vs local variable scope
- **Entry Point** — Detection of `الرئيسية` (Main) as mandatory entry point
- **Windows x64 ABI** — Stack frames, register argument passing (RCX, RDX, R8, R9), shadow space

### Fixed
- Global variables now correctly use their initializers

### Changed
- **Architecture** — Program structure changed from linear script to list of declarations

---

## [0.0.7] - 2025-12-25 - Loops

### Added
- **While Loop** — `طالما` statement
- **Variable Reassignment** — `x = 5.` syntax for updating existing variables

### Changed
- Implemented loop code generation using label jumps

---

## [0.0.6] - 2025-12-25 - Control Flow

### Added
- **If Statement** — `إذا` conditional
- **Block Scopes** — `{ ... }` grouping
- **Comparison Operators** — `==`, `!=`

### Changed
- Implemented label generation and conditional jumps in codegen
- Comprehensive documentation update (Internals & API)

---

## [0.0.5] - 2025-12-25 - Type System

### Changed
- **Breaking:** Renamed `رقم` to `صحيح` (int) to align with C types

### Added
- Single line comments (`//`)

---

## [0.0.4] - 2025-12-24 - Variables

### Added
- `رقم` (Int) type keyword
- Variable declaration (`رقم name = val.`)
- Variable usage in expressions

---

## [0.0.3] - 2025-12-24 - I/O

### Added
- `اطبع` (Print) statement
- Support for multiple statements in a program
- Integration with C Standard Library (`printf`)

---

## [0.0.2] - 2025-12-24 - Math

### Added
- Arabic numeral support (٠-٩)
- Addition (`+`) and subtraction (`-`)

---

## [0.0.1] - 2025-12-24 - Initial Release

### Added
- Initial compiler implementation
- Compiles `إرجع <number>.` to executable
- Basic pipeline: Lexer → Parser → Codegen → GCC