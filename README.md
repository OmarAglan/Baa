# Baa (باء) Programming Language

A compiled, Arabic-syntax systems programming language built from scratch in C.

## Features
*   **Native:** Compiles to x86_64 Assembly / Windows Executable.
*   **Arabic Syntax:** Full support for Arabic keywords and numerals.
*   **Control Flow:** Supports conditionals, loops, and blocks.
*   **Simple:** Clean, period-terminated syntax.

## Example

```baa
// عد تنازلي
صحيح س = ٥.

طالما (س != ٠) {
    اطبع س.
    س = س - ١.
}

إرجع ٠.
```

## Documentation
*   [User Guide (How to Use)](docs/USER_GUIDE.md)
*   [Language Specification (Syntax)](docs/LANGUAGE.md)
*   [Compiler Internals](docs/INTERNALS.md)
*   [Roadmap](ROADMAP.md)
*   [Changelog](CHANGELOG.md)

## Building from Source

**Prerequisites:** PowerShell, CMake, MinGW (GCC).

```powershell
mkdir build
cd build
cmake ..
cmake --build .
```

## Running
```powershell
.\baa.exe ..\test.b
.\out.exe
```