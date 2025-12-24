# Baa (باء) Programming Language

A compiled, Arabic-syntax systems programming language built from scratch in C.

## Features
*   **Native:** Compiles to x86_64 Assembly / Windows Executable.
*   **Arabic Syntax:** Full support for Arabic keywords and numerals.
*   **Simple:** Clean, period-terminated syntax.

## Example

```baa
صحيح س = ٤٢.
اطبع س + ٨.
إرجع ٠.
```

## Documentation
*   [Language Guide (Syntax)](docs/LANGUAGE.md)
*   [Compiler Internals (Architecture)](docs/INTERNALS.md)
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