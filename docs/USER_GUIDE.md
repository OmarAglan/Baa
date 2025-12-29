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
- **CMake** 3.10 or later
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

Create a file named `hello.b` using any text editor. **Important:** Save the file as **UTF-8** encoding.

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
| `الرئيسية` | Main function (program entry point) |
| `اطبع` | Print statement |
| `إرجع` | Return statement |
| `٠` | Arabic numeral zero |
| `.` | Statement terminator (like `;` in C) |

---

## 3. Command Line Interface

The Baa compiler `baa.exe` is now a robust command-line tool.

### Basic Compilation and Execution

```powershell
# Compile and link to default output (out.exe)
.\baa.exe hello.b

# Run the generated executable
.\out.exe```

### Command-Line Flags

| Flag | Description | Example |
|------|-------------|---------|
| `<input.b>` | Source file to compile. Multiple files can be provided (future: requires import system). | `.\baa.exe main.b` |
| `-o <file>` | Specify the output filename (e.g., `myapp.exe`, `mylib.o`, `output.s`). | `.\baa.exe -o myprog.exe hello.b` |
| `-S` | **Compile only to Assembly.** Produces `.s` file, does not invoke assembler/linker. | `.\baa.exe -S hello.b` (creates `hello.s`) |
| `-c` | **Compile and Assemble.** Produces object file (`.o`), does not link. (Future: for multi-file linking). | `.\baa.exe -c hello.b` (creates `hello.o`) |
| `--help`, `-h` | Display help message and usage. | `.\baa.exe --help` |
| `--version` | Display compiler version. | `.\baa.exe --version` |

### Compilation Workflow (`-o`, `-S`, `-c`)

You can control the compilation stages:

1.  **Source to Assembly**:
    ```powershell
    .\baa.exe -S program.b -o program.s
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
    .\baa.exe program.b -o program.exe
    # This is equivalent to steps 1-3 if gcc is installed
    ```

---

## 4. Common Patterns

### Variables and Math

```baa
صحيح الرئيسية() {
    صحيح س = ١٠.
    صحيح ص = ٢٠.
    صحيح مجموع = س + ص.
    
    اطبع "المجموع: ".
    اطبع مجموع.
    
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
    صحيح نتيجة = مربع(٥).
    اطبع نتيجة.  // يطبع ٢٥
    إرجع ٠.
}
```

---

## 5. Troubleshooting

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `Lexer Error: Unknown char` | File not saved as UTF-8 | In VS Code: Click encoding in status bar → "Save with Encoding" → "UTF-8" |
| `Parser Error: Expected ...` | Syntax error | Check for missing `.` at end of statements, `}` or parentheses. |
| `Undefined symbol` | Variable used before declaration | Declare variables before using them |
| `Cannot find الرئيسية` | Missing main function | Every program needs `صحيح الرئيسية() { ... }` |

### Getting Help

- Check the [Language Specification](LANGUAGE.md) for complete syntax reference
- See [Compiler Internals](INTERNALS.md) for technical details
- Open an issue on GitHub for bugs

---

*Next: [Language Specification →](LANGUAGE.md)*