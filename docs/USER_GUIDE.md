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

### Platform Support (v0.2.9)

- **Supported:** Windows (x86-64) with MinGW-w64 toolchain
- **Not supported:** Linux/macOS (the compiler and updater are currently Windows-first)

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

### Optional: Add `baa.exe` to PATH

From PowerShell (current user):

```powershell
$baaDir = (Resolve-Path .\build).Path
$current = [Environment]::GetEnvironmentVariable("Path", "User")
[Environment]::SetEnvironmentVariable("Path", "$current;$baaDir", "User")
```

Open a new terminal and verify:

```powershell
baa --version
```

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
.\out.exe
```

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

### Output Naming Rules (v0.2.9)

- Default output executable is `out.exe` (when linking).
- With `-S` or `-c`, output defaults to `<input>.s` / `<input>.o`.
- `-o <file>` is only applied for `-S` / `-c` when compiling a single input file.
- `update` must be used alone: `.\baa.exe update`

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

**Note:** The compiler currently exposes flags for `unused-variable` and `dead-code`. The shadowing warning exists, but is only enabled via `-Wall` (there is no dedicated `-Wshadow-variable` flag yet).


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

## 4. Deployment Notes

Baa’s compiler driver invokes `gcc` to assemble and link output binaries, so deployment typically requires:

- `baa.exe` available on the target machine (for example, on `PATH`)
- MinGW-w64 `gcc` available on `PATH`

If you distribute `baa.exe` to another Windows machine, ensure the MinGW-w64 toolchain is installed there as well.

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
    صحيح العمر = ٠.
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
| `Lexer Error: Unknown byte 0x..` | Unsupported character/byte in source | Ensure the file is UTF-8 and only uses supported punctuation and operators |
| `Lexer Error: Unterminated string ...` | Missing closing `"` | Close the string literal before the statement terminator `.` |
| `[Error] <file>:<line>:<col>: ...` | Syntax error (parser) | Check for missing `.` at statement end, missing `}`, or unmatched parentheses |
| `[Semantic Error] Assignment to undefined variable '...'` | Variable used before declaration | Declare variables before using them |
| `[Semantic Error] Type mismatch in assignment to '...'` | Assigning wrong type | Do not assign `نص` to `صحيح` (or vice versa) |
| `[Semantic Error] Cannot reassign constant '...'` | Modifying a constant | Constants declared with `ثابت` cannot be changed after initialization |
| `Aborting <file> due to syntax errors.` | Parser reported one or more errors | Fix reported `[Error]` diagnostics and recompile |
| `Aborting <file> due to semantic errors.` | Analyzer reported one or more errors | Fix reported `[Semantic Error]` messages and recompile |

### Common Warnings (v0.2.8+)

| Warning | Cause | Solution |
|---------|-------|----------|
| `[-Wunused-variable] Variable 'x' is declared but never used.` | Unused variable | Remove the variable or use it |
| `[-Wunused-variable] Global variable 'x' is declared but never used.` | Unused global | Remove it or reference it |
| `[-Wdead-code] Unreachable code after 'return/break' statement.` | Dead code after terminator | Remove code after `إرجع`/`توقف`/`استمر` |
| `[-Wshadow-variable] Local variable 'x' shadows global variable.` | Variable shadowing | Rename the local variable |

**Note:** Some warnings may currently show line number `0` due to missing AST line tracking in semantic analysis.

> **Note:** Warnings are non-fatal by default. Use `-Werror` to treat them as errors.

### Getting Help

- Check the [Language Specification](LANGUAGE.md) for complete syntax reference
- See [Compiler Internals](INTERNALS.md) for technical details
- Open an issue on GitHub for bugs

---

*Next: [Language Specification →](LANGUAGE.md)*
