# Changelog

All notable changes to the Baa programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Planned
- Recursion support
- String variables (`نص` type)
- Break/Continue statements (`توقف`, `استمر`)

---

## [0.1.1] - Structured Data

### Added
- **Arrays** — Declaration (`صحيح قائمة[٥]`), access (`قائمة[٠]`), and assignment (`قائمة[٠] = ١`)
- **For Loop** — `لكل` with Arabic semicolon `؛` separator
- **Postfix Operators** — Increment (`++`) and decrement (`--`)
- **Logic Operators** — `&&` (AND), `||` (OR), `!` (NOT) with short-circuit evaluation

---

## [0.1.0] - Text & Unary

### Added
- **String Literals** — `"text"` support
- **Character Literals** — `'x'` support
- **Unary Minus** — Negative numbers (`-5`)

### Changed
- Updated `اطبع` to support printing strings via `%s`
- Implemented string table generation in codegen

---

## [0.0.9] - Advanced Math

### Added
- **Multiplication** (`*`), **Division** (`/`), and **Modulo** (`%`)
- **Relational Operators** — `<`, `>`, `<=`, `>=`

### Changed
- **Parser** — Implemented operator precedence (PEMDAS) for correct expression evaluation

---

## [0.0.8] - Functions

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

## [0.0.7] - Loops

### Added
- **While Loop** — `طالما` statement
- **Variable Reassignment** — `x = 5.` syntax for updating existing variables

### Changed
- Implemented loop code generation using label jumps

---

## [0.0.6] - Control Flow

### Added
- **If Statement** — `إذا` conditional
- **Block Scopes** — `{ ... }` grouping
- **Comparison Operators** — `==`, `!=`

### Changed
- Implemented label generation and conditional jumps in codegen
- Comprehensive documentation update (Internals & API)

---

## [0.0.5] - Type System

### Changed
- **Breaking:** Renamed `رقم` to `صحيح` (int) to align with C types

### Added
- Single line comments (`//`)

---

## [0.0.4] - Variables

### Added
- `رقم` (Int) type keyword
- Variable declaration (`رقم name = val.`)
- Variable usage in expressions

---

## [0.0.3] - I/O

### Added
- `اطبع` (Print) statement
- Support for multiple statements in a program
- Integration with C Standard Library (`printf`)

---

## [0.0.2] - Math

### Added
- Arabic numeral support (٠-٩)
- Addition (`+`) and subtraction (`-`)

---

## [0.0.1] - Initial Release

### Added
- Initial compiler implementation
- Compiles `إرجع <number>.` to executable
- Basic pipeline: Lexer → Parser → Codegen → GCC