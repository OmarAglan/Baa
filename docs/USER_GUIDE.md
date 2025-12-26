# Baa User Guide

Welcome to Baa (باء)! This guide will help you write your first Arabic computer program.

---

## Table of Contents

- [Installation](#1-installation)
- [Your First Program](#2-your-first-program)
- [Compiling](#3-compiling)
- [Running](#4-running)
- [Common Patterns](#5-common-patterns)
- [Troubleshooting](#6-troubleshooting)

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

After building, you will have `baa.exe` in your build directory.

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

## 3. Compiling

Open PowerShell in your build directory and run:

```powershell
.\baa.exe path\to\your\hello.b
```

This creates `out.exe` in the current directory.

### What Happens During Compilation

1. **Lexing** — Source code is tokenized
2. **Parsing** — Tokens are organized into an Abstract Syntax Tree
3. **Code Generation** — Assembly code is generated (`out.s`)
4. **Linking** — GCC compiles assembly to executable (`out.exe`)

---

## 4. Running

```powershell
.\out.exe
```

You should see:
```
مرحباً بالعالم!
```

---

## 5. Common Patterns

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

## 6. Troubleshooting

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `Lexer Error: Unknown char` | File not saved as UTF-8 | In VS Code: Click encoding in status bar → "Save with Encoding" → "UTF-8" |
| `Parser Error: Expected ...` | Syntax error | Check for missing `.` at end of statements |
| `Undefined symbol` | Variable used before declaration | Declare variables before using them |
| `Cannot find الرئيسية` | Missing main function | Every program needs `صحيح الرئيسية() { ... }` |

### Getting Help

- Check the [Language Specification](LANGUAGE.md) for complete syntax reference
- See [Compiler Internals](INTERNALS.md) for technical details
- Open an issue on GitHub for bugs

---

*Next: [Language Specification →](LANGUAGE.md)*