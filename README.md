# Baa (باء) Programming Language

A compiled, Arabic-syntax systems programming language built from scratch in C.

## Features
*   **Native:** Compiles to x86_64 Assembly / Windows Executable.
*   **Arabic Syntax:** Full support for Arabic keywords and numerals.
*   **Functions:** Define and call functions with arguments.
*   **Scoping:** Supports Global and Local variables.
*   **Control Flow:** Supports conditionals, loops, and blocks.
*   **Math:** Full arithmetic (+, -, *, /, %) and comparisons (<, >, <=, >=).
*   **Text:** Support for String (`"..."`) and Character (`'...'`) literals.
*   **Simple:** Clean, period-terminated syntax.

## Example

```baa
// متغير عام (Global Variable)
صحيح عامل = ٢.

// تعريف دالة (Function Definition)
صحيح ضاعف(صحيح س) {
    إرجع س + س.
}

// نقطة البداية (Entry Point)
صحيح الرئيسية() {
    اطبع "بدأ البرنامج...".
    صحيح ن = ١٠.
    صحيح م = ضاعف(ن).
    
    // ٢٠ * ٢ + ١ = ٤١
    اطبع (م * عامل) + ١.
    
    إرجع ٠.
}
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