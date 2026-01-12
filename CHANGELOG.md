# Changelog

All notable changes to the Baa programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Planned (v0.2.8)
- **Warnings**: Diagnostic warnings for unused variables and dead code.
- **Warning Flags**: `-Wall`, `-Werror` to control warning behavior.

---
## [0.2.7] - 2026-01-12

### Added
- **Constant Keyword (`ثابت`)** — Declare immutable variables that cannot be reassigned after initialization.
  - Syntax: `ثابت صحيح حد = ١٠٠.` (const int limit = 100)
  - Works for both global and local variables.
- **Constant Arrays** — Support for immutable arrays: `ثابت صحيح قائمة[٥].`
- **Const Checking** — Semantic analysis now detects and reports:
  - Reassignment to constant variables.
  - Modification of constant array elements.
  - Constants declared without initialization.

### Changed
- **Symbol Table** — Added `is_const` flag to track constant status.
- **Parser** — Updated to recognize `ثابت` keyword before type declarations.
- **Semantic Analysis** — Enhanced to enforce immutability rules.

### Technical Details
- Constants must be initialized at declaration time.
- Functions cannot be declared as const.
- Const checking happens during semantic analysis phase (before code generation).

---
## [0.2.6] - 2026-01-11

### Added
- **Preprocessor Engine** – Fully integrated into Lexer (`lexer.c`) to handle directives before tokenization.
- **Macro Definitions** – Implemented `#تعريف <name> <value>` to define compile-time constants.
- **Conditional Compilation** – Implemented `#إذا_عرف`, `#وإلا`, and `#نهاية` to conditionally include or exclude code blocks.
- **Undefine Directive** – Implemented `#الغاء_تعريف` to remove macro definitions.
- **Macro Substitution** – Identifiers are automatically replaced with macro values during lexing.

### Fixed
- **Codegen State Leak** – Fixed critical bug where symbol tables, label counters, and loop stacks were not reset between compilation units.
- Added `reset_codegen()` function called at start of each `NODE_PROGRAM` in `codegen.c`.
- Prevents symbol collisions and invalid assembly when compiling multiple files.

### Technical Details
- Preprocessor directives are handled in `lexer_next_token()` before any tokenization.
- Include stack depth limited to 10 levels to prevent infinite recursion.
- Macro table size limited to 100 entries.
- File state (source, line, col, filename) properly tracked through include stack.

## [0.2.5] - 2026-01-04
### Added
- **Multi-File Compilation**: Full support for compiling multiple `.baa` files into a single executable.
  - Each file compiled to `.o` independently, then linked together.
  - Proper handling of compilation errors per file.
- **Function Prototypes**: New syntax `صحيح دالة().` to declare functions without definition (for cross-file usage).
- **Include Directive**: Implemented `#تضمين "file"` (include) for header files.
  - Nested includes supported up to 10 levels deep.
  - Proper filename tracking for error reporting.
- **File Extension Convention**: Standardized `.baa` for source, `.baahd` for headers.

### Changed
- **Documentation**: Updated all examples to use `.baa` extension.
- **CLI**: Multi-file compilation now primary workflow.

### Fixed
- **CLI Arguments**: Fixed crash when running `baa` without arguments.
- **Lowercase `-s` Flag**: Added support for `-s` as alias for `-S`.
- **Updater Version Logic**: Fixed semantic version comparison (now properly compares major.minor.patch).
- **Update Command Parsing**: `baa update` must now be used alone (no filename arguments).

## [0.2.4] - 2026-01-04

### Added
- **Semantic Analysis Pass** — Implemented a dedicated validation phase (`src/analysis.c`) that runs before code generation. It checks for:
    - **Symbol Resolution**: Ensures variables are declared before use.
    - **Type Checking**: Strictly enforces `TYPE_INT` vs `TYPE_STRING` compatibility.
    - **Scope Validation**: Tracks global vs local variable declarations.
    - **Control Flow Validation**: Ensures `break`/`continue` only used in valid contexts.
- **Shared Symbol Definitions** — Moved `Symbol` and `ScopeType` to `src/baa.h` to allow sharing between Analysis and Codegen modules.

### Changed
- **Compiler Pipeline** — Updated `src/main.c` to invoke the analyzer after parsing. Compilation now aborts immediately if semantic errors are found.
- **Code Generator** — Refactored `src/codegen.c` to rely on the shared symbol definitions.


### Technical Details
- Analyzer maintains separate symbol tables from codegen (isolation).
- Type inference implemented for all expression types.
- Nested scope tracking (global/local only - no block-level yet).

---

## [0.2.3] - 2025-12-29

### Added
- **Windows Installer** — Created a professional `setup.exe` using Inno Setup. It installs the compiler, documentation, and creates Start Menu shortcuts.
- **PATH Integration** – Installer automatically adds Baa to system `PATH` environment variable.
  - Enables `baa` command from any directory.
- **Auto Updater** — Added `baa update` command. It checks a remote server for new versions and downloads/installs updates automatically.
- **Update System** – Implemented `src/updater.c` using native Windows APIs.
  - Uses `URLDownloadToFileA` from `urlmon.lib`.
  - Downloads installer to temp directory.
  - Launches installer and exits current process.

### Technical Details
- Version checking via HTTP GET to `version.txt`.
- Semantic version comparison (major.minor.patch).
- Cache clearing to ensure fresh version data.

---

## [0.2.2] - 2025-12-29

### Added
- **Diagnostic Engine** (`src/error.c`) – Professional error reporting with source context.
  - Displays: `[Error] filename:line:col: message`
  - Shows actual source line with `^` pointer to error position.
  - Printf-style formatting support.
- **Panic Mode Recovery** – Parser continues after syntax errors to find multiple issues.
  - Synchronizes at statement boundaries (`;`, `.`, `}`, keywords).
  - Prevents cascading error messages.
- **Enhanced Token Tracking** – All tokens now store `filename`, `line`, and `col`.

### Fixed
- **Global String Initialization** – Fixed bug where `نص س = "text".` would print `(null)`.
  - Now correctly emits `.quad .Lstr_N` in data section.

---

## [0.2.1] - 2025-12-29

### Added
- **Executable Branding** — Added a custom icon (`.ico`) and version metadata to `baa.exe`.
    - Resource file: `src/baa.rc`
    - Displays in Windows Explorer properties.
- **Windows Integration** — File properties now show "Baa Programming Language" and version info.

---

## [0.2.0] - 2025-12-29

### Added
- **CLI Driver** – Complete rewrite of `main.c` as professional build system.
  - Argument parsing with flag support.
  - Multi-stage compilation pipeline.
  - Automatic invocation of GCC for assembly/linking.
- **Flags** — Added support for standard compiler flags:
    - `-o <file>`: Specify output filename.
    - `-S`: Compile to assembly only (skip assembler/linker).
    - `-c`: Compile to object file (skip linker).
    - `-v`: Verbose output.
    - `--help`: Print usage information.
    - `--version`: Print version and build date.
- **GCC Integration** – Automatic invocation via `system()` calls.

### Changed
- **Architecture** – Transformed from simple transpiler to full compilation toolchain.

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