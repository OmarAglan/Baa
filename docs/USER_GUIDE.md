# Baa User Guide

Welcome to Baa (باء)! This guide will help you write your first Arabic computer program and use the Baa compiler toolchain.

---

## Table of Contents

- [Installation](#1-installation)
- [Your First Program](#2-your-first-program)
- [Command Line Interface](#3-command-line-interface)
- [Common Patterns](#4-common-patterns)
- [Troubleshooting](#5-troubleshooting)

---

## 1. Installation

Currently, you must build Baa from source.

### Prerequisites
- **CMake** 3.10 or higher
- **MinGW-w64** (GCC compiler)
- **PowerShell** (Windows)

### Build Steps

```powershell
# Clone the repository
git clone https://github.com/OmarAglan/Baa.git
cd Baa

# Build
mkdir build
cd build
cmake ..
cmake --build .
```

After building, you will have `baa.exe` in your `build` directory.

---

## 2. Your First Program

Create a file named `hello.baa` using any text editor. **Important:** Save the file as **UTF-8** encoding.


```baa
// هذا برنامجي الأول
صحيح الرئيسية() {
    اطبع "مرحباً بالعالم!".
    إرجع ٠.
}
```

### Understanding the Code

| Code | Meaning |
|------|---------|
| `//` | Single-line comment |
| `صحيح` | Integer type (like `int` in C) |
| `ثابت` | Constant modifier (immutable variable) |
| `الرئيسية` | Main function (program entry point) |
| `اطبع` | Print statement |
| `إرجع` | Return statement |
| `٠` | Arabic numeral zero |
| `.` | Statement terminator (like `;` in C) |

---

## 3. Command Line Interface

The Baa compiler `baa.exe` is a full-featured command-line tool (since v0.2.0).

### Basic Compilation and Execution

```powershell
# Compile and link to default output (out.exe)
.\baa.exe hello.baa

# Run the program
.\out.exe```

### Command-Line Flags

| Flag | Description | Example |
|------|-------------|---------|
| `<input.baa>` | Source file(s) to compile. | `.\baa.exe main.baa lib.baa` |
| `update` | **Self Update.** Checks the server for the latest version and updates Baa. | `.\baa.exe update` |
| `-o <file>` | Specify the output filename (e.g., `myapp.exe`, `mylib.o`, `output.s`). | `.\baa.exe main.baa -o myapp.exe` |
| `-S`, `-s` | **Compile only to Assembly.** Produces `.s` file, does not invoke assembler/linker. | `.\baa.exe -S main.baa` (creates `main.s`) |
| `-c` | **Compile and Assemble.** Produces object file (`.o`), does not link. | `.\baa.exe -c main.baa` (creates `main.o`) |
| `-v` | Enable verbose output (shows all compilation steps with timing). | `.\baa.exe -v main.baa` |
| `--help`, `-h` | Display help message and usage. | `.\baa.exe --help` |
| `--version` | Display compiler version. | `.\baa.exe --version` |

### Warning Flags (v0.2.8+)

| Flag | Description | Example |
|------|-------------|---------|
| `-Wall` | Enable all warnings. | `.\baa.exe -Wall main.baa` |
| `-Werror` | Treat warnings as errors (compilation fails). | `.\baa.exe -Wall -Werror main.baa` |
| `-Wunused-variable` | Warn about unused variables. | `.\baa.exe -Wunused-variable main.baa` |
| `-Wdead-code` | Warn about unreachable code after `إرجع`/`توقف`. | `.\baa.exe -Wdead-code main.baa` |
| `-Wno-<warning>` | Disable a specific warning. | `.\baa.exe -Wall -Wno-unused-variable main.baa` |
| `-Wcolor` | Force colored output. | `.\baa.exe -Wall -Wcolor main.baa` |
| `-Wno-color` | Disable colored output. | `.\baa.exe -Wall -Wno-color main.baa` |


### Compilation Workflow

The compiler can handle multiple source files and produce a single executable.

#### 1. Simple Build (Recommended)
Compile multiple files and link them automatically:
```powershell
.\baa.exe main.baa utils.baa math.baa -o myapp.exe
.\myapp.exe
```

#### 2. Manual Steps (`-o`, `-S`, `-c`)
You can control the compilation stages if needed:

1.  **Source to Assembly**:
    ```powershell
    .\baa.exe -S program.baa -o program.s
    # This will create 'program.s'
    ```
2.  **Assembly to Object File (via GCC)**:
    ```powershell
    # Requires 'gcc' in PATH
    gcc -c program.s -o program.o
    ```
3.  **Link Object Files to Executable (via GCC)**:
    ```powershell
    # Requires 'gcc' in PATH
    gcc program.o -o program.exe
    ```
4.  **Full Pipeline (default)**:
    ```powershell
    .\baa.exe program.baa -o program.exe
    # This is equivalent to steps 1-3 if gcc is installed
    ```

---

## 4. File Types

| Extension | Type | Description | Example |
|-----------|------|-------------|---------|
| `.baa` | Source File | Main Baa source code | `main.baa`, `lib.baa` |
| `.baahd` | Header File | Function prototypes and declarations | `math.baahd` |
| `.s` | Assembly | Generated assembly code (with `-S`) | `main.s` |
| `.o` | Object File | Compiled binary object (with `-c`) | `main.o` |
| `.exe` | Executable | Final linked program | `out.exe` |

**Note:** Header files (`.baahd`) are included using `#تضمين` directive and contain only declarations (prototypes), not implementations.

---

## 5. Organizing Code (Multi-File Projects)

As your program grows, you should split it into multiple files.

### Using Headers (`.baahd`)

Use header files for function prototypes and shared declarations.

**math.baahd** (Header):
```baa
// Function prototype (declaration only)
صحيح جمع(صحيح أ, صحيح ب).
```

**math.baa** (Implementation):
```baa
// Actual function implementation
صحيح جمع(صحيح أ, صحيح ب) {
    إرجع أ + ب.
}
```

**main.baa** (Main Program):
```baa
// Include the header
#تضمين "math.baahd"

صحيح الرئيسية() {
    اطبع جمع(١٠, ٢٠).
    إرجع ٠.
}
```

**Compilation:**
```powershell
.\baa.exe main.baa math.baa -o myapp.exe
```

> **Note:** The `#تضمين` directive looks for files relative to the directory where you run the compiler.

---

## 6. Common Patterns

### Variables and Constants

```baa
صحيح الرئيسية() {
    // Regular variable (can be changed)
    صحيح س = ١٠.
    س = ٢٠.  // ✓ OK
    
    // Constant (cannot be changed)
    ثابت صحيح ص = ٣٠.
    // ص = ٤٠.  // ✗ Error: Cannot reassign constant
    
    صحيح مجموع = س + ص.
    اطبع مجموع.  // يطبع ٥٠
    
    إرجع ٠.
}
```

### Loops

```baa
صحيح الرئيسية() {
    // طباعة الأرقام من ١ إلى ٥
    لكل (صحيح س = ١؛ س <= ٥؛ س++) {
        اطبع س.
    }
    إرجع ٠.
}
```

### Conditionals

```baa
// If-else statement
صحيح الرئيسية() {
    صحيح العمر = ١٨.
    
    إذا (العمر >= ١٨) {
        اطبع "أنت بالغ".
    } وإلا {
        اطبع "لست بالغاً".
    }
    
    إرجع ٠.
}
```

### Functions

```baa
صحيح مربع(صحيح س) {
    إرجع س * س.
}

صحيح الرئيسية() {
    // Calculate square
    صحيح نتيجة = مربع(٥).
    اطبع نتيجة.  // يطبع ٢٥
    إرجع ٠.
}
```

### Reading Input

You can read integer input from the user using the `اقرأ` statement.

```baa
صحيح الرئيسية() {
    صحيح العمر.
    اطبع "أدخل عمرك: ".
    اقرأ العمر.
    
    إذا (العمر >= ١٨) {
        اطبع "أنت بالغ.".
    } وإلا {
        اطبع "أنت قاصر.".
    }
    إرجع ٠.
}
```

### Boolean Logic

Use `منطقي` variables to store true (`صواب`) or false (`خطأ`) values.

```baa
صحيح الرئيسية() {
    منطقي جاهز = صواب.
    
    إذا (جاهز) {
        اطبع "النظام جاهز!".
    }
    
    إرجع ٠.
}
```

---

## 5. Troubleshooting

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `Lexer Error: Unknown char` | File not saved as UTF-8 | In VS Code: Click encoding in status bar → "Save with Encoding" → "UTF-8" |
| `Parser Error: Expected ...` | Syntax error | Check for missing `.` (dot) at end of statements, missing `}`, or unmatched parentheses. Use Arabic semicolon `؛` in for loops. |
| `Undefined symbol` | Variable used before declaration | Declare variables before using them |
| `Cannot find الرئيسية` | Missing main function | Every program needs `صحيح الرئيسية() { ... }` |
| `Type mismatch` | Assigning wrong type | Cannot assign strings to integers or vice versa |
| `Cannot reassign constant` | Modifying a constant | Constants declared with `ثابت` cannot be changed after initialization |
| `Constant must be initialized` | Missing initial value | Constants require a value at declaration: `ثابت صحيح س = ١٠.` |
| `break/continue outside loop` | Control flow error | `توقف` and `استمر` must be inside loops or switches |

### Common Warnings (v0.2.8+)

| Warning | Cause | Solution |
|---------|-------|----------|
| `Variable 'x' is declared but never used` | Unused variable | Remove the variable or use it in your code |
| `Unreachable code after 'return/break'` | Dead code | Remove code after `إرجع` or `توقف` statements |
| `Local variable 'x' shadows global variable` | Variable shadowing | Rename the local variable to avoid confusion |

> **Note:** Warnings are non-fatal by default. Use `-Werror` to treat them as errors.

### Getting Help

- Check the [Language Specification](LANGUAGE.md) for complete syntax reference
- See [Compiler Internals](INTERNALS.md) for technical details
- Open an issue on GitHub for bugs

---

*Next: [Language Specification →](LANGUAGE.md)*