# Baa User Guide

> **Version:** 0.5.4 | [← README](../README.md) | [Language Spec →](LANGUAGE.md)

Welcome to Baa (باء)! This guide will help you write your first Arabic computer program and use the Baa compiler toolchain.

---

## Table of Contents

- [1. Installation](#1-installation)
- [2. Your First Program](#2-your-first-program)
- [3. Command Line Interface](#3-command-line-interface)
- [4. Linux Packaging (TGZ + DEB)](#4-linux-packaging-tgz--deb)
- [5. File Types](#5-file-types)
- [6. Deployment Notes](#6-deployment-notes)
- [7. Organizing Code (Multi-File Projects)](#7-organizing-code-multi-file-projects)
- [8. Common Patterns](#8-common-patterns)
- [9. Testing and QA](#9-testing-and-qa)
- [10. Troubleshooting](#10-troubleshooting)

---

## 1. Installation

### Platform Support

- **Supported:** Windows (x86-64) and Linux (x86-64)
- **macOS:** Not supported yet

### Installation Methods

#### Windows: Installer with GCC Bundle (Recommended)

The easiest way to install Baa on Windows is using the installer which includes a GCC bundle:

1. Download the latest `Baa-Setup.exe` from the [releases page](https://github.com/OmarAglan/Baa/releases)
2. Run the installer and follow the prompts
3. The installer will automatically:
   - Install `baa.exe` to the selected directory
   - Set up the bundled MinGW-w64 GCC toolchain
   - Install stdlib + examples + docs
   - Offer optional integration tasks:
     - add `baa` and bundled GCC to `PATH`
     - set `BAA_HOME` and `BAA_STDLIB`
     - associate `.baa/.baahd` files

After installation, open a new terminal and verify:

```powershell
baa --version
```

#### Windows: Build from Source

If you prefer to build from source:

**Prerequisites:**
- **CMake** 3.10 or higher
- **MinGW-w64** (GCC compiler)
- **PowerShell**

**Build Steps:**

```powershell
# Clone the repository
git clone https://github.com/OmarAglan/Baa.git
cd Baa

# Build
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

After building, you will have `baa.exe` in your `build` directory.

#### Linux: Build from Source

**Prerequisites:**
- **CMake** 3.10 or higher
- **GCC/Clang** toolchain

**Build Steps:**

```bash
git clone https://github.com/OmarAglan/Baa.git
cd Baa

cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j
./build-linux/baa --version
```

After building, you will have `baa` in your `build-linux` directory.

### Optional: Add `baa` to PATH

**Windows (PowerShell - current user):**

```powershell
$baaDir = (Resolve-Path .\build).Path
$current = [Environment]::GetEnvironmentVariable("Path", "User")
[Environment]::SetEnvironmentVariable("Path", "$current;$baaDir", "User")
```

**Linux:**

```bash
sudo cp ./build/baa /usr/local/bin/
```

Open a new terminal and verify:

```bash
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

### Compile and Run

**Windows:**

```powershell
# Using default output (out.exe)
.\build\baa.exe hello.baa
.\out.exe

# Or specify output name
.\build\baa.exe hello.baa -o hello.exe
.\hello.exe
```

**Linux:**

```bash
# Using default output (out)
./build/baa hello.baa
./out

# Or specify output name
./build/baa hello.baa -o hello
./hello
```

---

## 3. Command Line Interface

The Baa compiler is a full-featured command-line tool (since v0.2.0):

- Windows: `baa.exe`
- Linux: `baa`
- In examples below, use `baa`/`baa.exe` if it's on `PATH`; otherwise use `build\baa.exe` (Windows) or `./build/baa` (Linux).

### Basic Usage

```bash
baa [options] <source.baa> [-o <output>]
```

### Command-Line Flags

| Flag | Description | Example |
|------|-------------|---------|
| `<input.baa>` | Source file(s) to compile. | `.\baa.exe main.baa lib.baa` |
| `update` | **Self Update.** Windows-only currently. | `.\baa.exe update` |
| `-o <file>` | Specify the output filename (e.g., `myapp.exe`, `mylib.o`, `output.s`). | `.\baa.exe main.baa -o myapp.exe` |
| `-I <dir>` / `-I<dir>` | Add include search directory for `#تضمين` (can be repeated; order preserved). | `.\baa.exe -I include -I third_party\hdr main.baa` |
| `-S`, `-s` | **Compile only to Assembly.** Produces `.s` file, does not invoke assembler/linker. | `.\baa.exe -S main.baa` (creates `main.s`) |
| `-c` | **Compile and Assemble.** Produces object file (`.o`), does not link. | `.\baa.exe -c main.baa` (creates `main.o`) |
| `-v` | Enable verbose output (shows all compilation steps with timing). | `.\baa.exe -v main.baa` |
| `--time-phases` | Print per-phase timing and memory statistics. | `.\baa.exe --time-phases -O2 main.baa` |
| `--emit-build-manifest <file>` | Write a deterministic JSON dependency/build manifest. | `.\baa.exe --emit-build-manifest build.json main.baa` |
| `--incremental` | Reuse cached object files when source/header content hashes and flags match. | `.\baa.exe --incremental main.baa lib.baa` |
| `--cache-dir <dir>` | Override the incremental cache directory (default: `.baa_build/cache`). | `.\baa.exe --incremental --cache-dir .cache/baa main.baa` |
| `--debug-info` | Emit debug line info and pass `-g` to toolchain. | `.\baa.exe --debug-info main.baa` |
| `--asm-comments` | Emit explanatory comments in generated assembly (`-S`). | `.\baa.exe -S --asm-comments main.baa` |
| `--help`, `-h` | Display help message and usage. | `.\baa.exe --help` |
| `--version` | Display compiler version. | `.\baa.exe --version` |
| `-O0` / `-O1` / `-O2` | Optimization levels (default: `-O1`). | `.\baa.exe -O2 main.baa` |
| `--target=<t>` | Select backend target (`x86_64-windows` or `x86_64-linux`). | `.\baa.exe --target=x86_64-linux main.baa` |
| `--startup=custom` | Use custom entrypoint symbol `__baa_start` (via linker `-e`) while keeping CRT/libc initialization. | `.\baa.exe --startup=custom main.baa -o app.exe` |
| `--dump-ir` | Print Baa IR (Arabic) after semantic analysis. | `.\baa.exe --dump-ir main.baa` |
| `--emit-ir` | Write Baa IR (Arabic) to `<input>.ir` after semantic analysis. | `.\baa.exe --emit-ir main.baa` |
| `--dump-ir-opt` | Print optimized Baa IR (Arabic) after the optimizer. | `.\baa.exe --dump-ir-opt -O2 main.baa` |
| `--verify` | Run all verifiers (`--verify-ir` + `--verify-ssa`; **requires `-O1`/`-O2`**). | `.\baa.exe --verify -O2 main.baa` |
| `--verify-ir` | Verify IR well-formedness (operands/types/terminators/phi/calls). | `.\baa.exe --verify-ir -O2 main.baa` |
| `--verify-ssa` | Verify SSA invariants after Mem2Reg and before Out-of-SSA (**requires `-O1`/`-O2`**). | `.\baa.exe --verify-ssa -O2 main.baa` |
| `--verify-gate` | Debug: run `--verify-ir`/`--verify-ssa` after each optimizer iteration (**requires `-O1`/`-O2`**). | `.\baa.exe --verify-gate -O2 main.baa` |
| `-funroll-loops` | Unroll small constant-count loops (conservative). | `.\baa.exe -funroll-loops -O2 main.baa` |
| `-fPIC` | PIC-friendly emission (Linux/ELF). | `./baa -fPIC main.baa` |
| `-fPIE` | PIE build (Linux/ELF; adds `-pie` at link). | `./baa -fPIE main.baa` |
| `-fno-pic` / `-fno-pie` | Disable PIC/PIE modes. | `./baa -fno-pie main.baa` |
| `-mcmodel=small` | Select code model (only `small` is supported). | `./baa -mcmodel=small main.baa` |
| `-fstack-protector` | Enable stack canary (Linux/ELF). | `./baa -fstack-protector main.baa` |
| `-fstack-protector-all` | Enable canary for all functions (Linux/ELF). | `./baa -fstack-protector-all main.baa` |
| `-fno-stack-protector` | Disable stack canary. | `./baa -fno-stack-protector main.baa` |

> **Note on Inlining:** At `-O2`, small internal functions with a single call site may be inlined automatically for better performance.

### Warning Flags (v0.2.8+)

| Flag | Description | Example |
|------|-------------|---------|
| `-Wall` | Enable all warnings. | `.\baa.exe -Wall main.baa` |
| `-Werror` | Treat warnings as errors (compilation fails). | `.\baa.exe -Wall -Werror main.baa` |
| `-Wunused-variable` | Warn about unused variables. | `.\baa.exe -Wunused-variable main.baa` |
| `-Wno-unused-variable` | Disable unused variable warnings. | `.\baa.exe -Wall -Wno-unused-variable main.baa` |
| `-Wdead-code` | Warn about unreachable code after `إرجع`/`توقف`. | `.\baa.exe -Wdead-code main.baa` |
| `-Wno-dead-code` | Disable dead code warnings. | `.\baa.exe -Wall -Wno-dead-code main.baa` |
| `-Wimplicit-narrowing` | Warn on potentially lossy implicit numeric conversions. | `.\baa.exe -Wimplicit-narrowing main.baa` |
| `-Wno-implicit-narrowing` | Disable implicit narrowing warnings. | `.\baa.exe -Wall -Wno-implicit-narrowing main.baa` |
| `-Wsigned-unsigned-compare` | Warn on mixed signed/unsigned comparisons. | `.\baa.exe -Wsigned-unsigned-compare main.baa` |
| `-Wno-signed-unsigned-compare` | Disable signed/unsigned comparison warnings. | `.\baa.exe -Wall -Wno-signed-unsigned-compare main.baa` |
| `-Wcolor` | Force colored output. | `.\baa.exe -Wall -Wcolor main.baa` |
| `-Wno-color` | Disable colored output. | `.\baa.exe -Wall -Wno-color main.baa` |

**Note:** Dedicated flags are available for `unused-variable`, `dead-code`, `implicit-narrowing`, and `signed-unsigned-compare`. `shadow-variable` is available through `-Wall` (no dedicated `-Wshadow-variable` switch yet).

### Output Naming Rules

- Default output executable is `out.exe` on Windows, and `out` on Linux (when linking).
- With `-S` or `-c`, output defaults to `<input>.s` / `<input>.o`.
- `-o <file>` is only applied for `-S` / `-c` when compiling a single input file.
- `update` must be used alone: `.\baa.exe update`

### Build Manifests and Incremental Builds (v0.5.3)

- `--emit-build-manifest <file>` writes JSON with compiler version, target, output mode, source files, canonical dependencies, content hashes, and cache status.
- `--incremental` is opt-in and only affects object-producing builds (`-c` and normal link mode).
- Cache hits are disabled for observable compiler/debug modes: IR dumps, `--emit-ir`, and verifier flags.
- Header invalidation is content-hash based: changing a `#تضمين` file rebuilds only source units that depended on it.

### CMake Build Profiles (v0.5.3)

Preset-capable CMake versions can use:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev
cmake --preset windows-verify
cmake --build --preset windows-verify
```

```bash
cmake --preset linux-dev
cmake --build --preset linux-dev
cmake --preset linux-verify
cmake --build --preset linux-verify
```

`verify` presets enable warnings as errors and release/verify presets set a reproducible build date.

### Cross-Target Notes (v0.3.2.8.4+)

- If the selected `--target` does not match the host toolchain/object format, the compiler currently supports **assembly-only** mode (`-S`) only. Cross-target `-c` and linking are deferred.

### Current Limitations (v0.3.10.6)

- SIMD and non-`عشري` floating types are not supported yet.

---

## 4. Linux Packaging (TGZ + DEB) (v0.3.6)

You can generate Linux installer artifacts from a Linux build:

```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j

# Generate TGZ + DEB in build-linux/
(cd build-linux && cpack -G TGZ)
(cd build-linux && cpack -G DEB)
```

Or run the helper script:

```bash
bash scripts/package_linux.sh
```

Build with strict warnings (quality gate):

```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release -DBAA_WARNINGS_AS_ERRORS=ON
cmake --build build-linux -j
```

Install (DEB):

```bash
sudo dpkg -i build-linux/baa-*-Linux-x86_64.deb
```

Install (TGZ):

```bash
tar -xzf build-linux/baa-*-Linux-x86_64.tar.gz
sudo cp -a usr/* /usr/
```

---

## 5. File Types

| Extension | Type | Description | Example |
|-----------|------|-------------|---------|
| `.baa` | Source File | Main Baa source code | `main.baa`, `lib.baa` |
| `.baahd` | Header File | Function prototypes and declarations | `math.baahd` |
| `.s` | Assembly | Generated assembly code (with `-S`) | `main.s` |
| `.o` | Object File | Compiled binary object (with `-c`) | `main.o` |
| `.exe` | Executable | Final linked program (Windows) | `out.exe` |
| *(no ext)* | Executable | Final linked program (Linux) | `out` |

**Note:** Header files (`.baahd`) are included using `#تضمين` directive and contain only declarations (prototypes), not implementations.

---

## 6. Deployment Notes

Baa's compiler driver invokes `gcc` to assemble and link output binaries, so deployment typically requires:

- `baa.exe`/`baa` available on the target machine (for example, on `PATH`)
- MinGW-w64 `gcc` (Windows) or GCC/Clang (Linux) available on `PATH`

If you distribute `baa.exe` to another Windows machine, ensure the MinGW-w64 toolchain is installed there as well (or use the installer with GCC bundle).

---

## 7. Organizing Code (Multi-File Projects)

As your program grows, you should split it into multiple files.

### Using Headers (`.baahd`)

Use header files for function prototypes and shared declarations.

**math.baahd** (Header):

```baa
// Function prototype (declaration only)
صحيح جمع(صحيح أ، صحيح ب).
```

**math.baa** (Implementation):

```baa
// Actual function implementation
صحيح جمع(صحيح أ، صحيح ب) {
    إرجع أ + ب.
}
```

**main.baa** (Main Program):

```baa
// Include the header
#تضمين "math.baahd"

صحيح الرئيسية() {
    اطبع جمع(١٠، ٢٠).
    إرجع ٠.
}
```

**Compilation:**

```powershell
.\baa.exe main.baa math.baa -o myapp.exe
```

> **Note:** `#تضمين` search order is: (1) current source-file directory, (2) exact path, (3) `{BAA_HOME}/<path>` for relative paths, (4) CLI `-I` paths in order; and for bare names (e.g. `baalib.baahd`) it also tries `<source_dir>/stdlib/`, `stdlib/`, `BAA_STDLIB`, then `{BAA_HOME}/stdlib`. Successful include paths are normalized before activation, so equivalent forms like `a.baahd` and `./a.baahd` collapse to one identity, and include cycles now produce an Arabic include-chain diagnostic.

**Visibility rules for multi-file code:**

- Top-level functions are externally visible by default and should be exposed through prototypes in `.baahd`.
- A header prototype is a declaration only; it does not create a second function body.
- Top-level `ساكن` variables and arrays are file-local and do not collide across files.
- Shared cross-file state via global variables is not a stable public pattern yet because the language still lacks a separate `extern`-style variable declaration.

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

1. **Source to Assembly**:

    ```powershell
    .\baa.exe -S program.baa -o program.s
    # This will create 'program.s'
    ```

2. **Assembly to Object File (via GCC)**:

    ```powershell
    # Requires 'gcc' in PATH
    gcc -c program.s -o program.o
    ```

3. **Link Object Files to Executable (via GCC)**:

    ```powershell
    # Requires 'gcc' in PATH
    gcc program.o -o program.exe
    ```

4. **Full Pipeline (default)**:

    ```powershell
    .\baa.exe program.baa -o program.exe
    # This is equivalent to steps 1-3 if gcc is installed
    ```

---

## 8. Common Patterns

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

### Control Flow

```baa
صحيح الرئيسية() {
    // If/Else
    صحيح عمر = ٢٥.
    إذا (عمر >= ١٨) {
        اطبع "بالغ".
    } وإلا {
        اطبع "قاصر".
    }
    
    // While loop
    صحيح عداد = ٠.
    طالما (عداد < ٥) {
        اطبع عداد.
        عداد = عداد + ١.
    }
    
    // For loop
    لكل (صحيح س = ٠؛ س < ٣؛ س = س + ١) {
        اطبع س.
    }
    
    // Switch
    صحيح خيار = ٢.
    اختر (خيار) {
        حالة ١:
            اطبع "واحد".
            توقف.
        حالة ٢:
            اطبع "اثنان".
            توقف.
        افتراضي:
            اطبع "آخر".
    }
    
    إرجع ٠.
}
```

### Arrays

```baa
صحيح الرئيسية() {
    // 1D array
    صحيح أرقام[٥] = {١، ٢، ٣، ٤، ٥}.
    اطبع أرقام[٢].  // يطبع ٣
    
    // 2D array
    صحيح مصفوفة[٢][٣] = {
        {١، ٢، ٣}،
        {٤، ٥، ٦}
    }.
    اطبع مصفوفة[١][٢].  // يطبع ٦
    
    // Calculate array length
    صحيح الطول = حجم(أرقام) / حجم(صحيح).
    اطبع الطول.  // يطبع ٥
    
    إرجع ٠.
}
```

### Strings and Standard Library

To manipulate dynamically allocated strings, include the standard library header. Remember to free memory allocated by `دمج_نص` or `نسخ_نص`.

```baa
#تضمين "stdlib/baalib.baahd"

صحيح الرئيسية() {
    نص جزء١ = "مرحباً ".
    نص جزء٢ = "باء!".
    
    // Concatenate strings (allocates memory)
    نص رسالة = دمج_نص(جزء١، جزء٢).
    اطبع رسالة.
    
    // Get string length
    صحيح الطول = طول_نص(رسالة).
    اطبع الطول. // يطبع ١١
    
    // Free allocated memory
حرر_نص(رسالة).
    
    إرجع ٠.
}
```

### Formatted Console I/O (الإخراج/الإدخال المنسّق) (v0.4.0.0)

Baa v0.4.0.0 adds formatted I/O builtins using **Arabic format specifiers** (مثل `%ص/%ن/%ع`) مع دعم `width` و `precision` على نمط C.

> ملاحظة: تسلسلات الهروب في باء عربية، استخدم `\س` للسطر الجديد بدلاً من `\n`.

```baa
#تضمين "stdlib/baalib.baahd"

صحيح الرئيسية() {
    نص اسم = "علي".
    صحيح عمر = ٢٥.

    اطبع_منسق("الاسم: %ن، العمر: %ص\س", اسم, عمر).

    نص رسالة = نسق("x=%س f=%.2ع", كـ<ط٦٤>(42), 3.5).
    اطبع رسالة.
    حرر_نص(رسالة).

    صحيح رقم = اقرأ_رقم().
    نص سطر = اقرأ_سطر().
    إذا (سطر != عدم) { حرر_نص(سطر). }

    إرجع ٠.
}
```

### Dynamic Memory (الذاكرة الديناميكية)

Use the v0.3.11 memory APIs for heap allocation and raw memory operations:

- `حجز_ذاكرة` / `تحرير_ذاكرة`
- `إعادة_حجز` (realloc)
- `نسخ_ذاكرة` / `تعيين_ذاكرة`

```baa
#تضمين "stdlib/baalib.baahd"

صحيح الرئيسية() {
    صحيح عدد = ٤.
    صحيح* أرقام = حجز_ذاكرة(عدد * حجم(صحيح)).
    إذا (أرقام == عدم) { إرجع ١. }

    تعيين_ذاكرة(أرقام، ٠، عدد * حجم(صحيح)).
    *(أرقام + ٠) = ١٠.
    *(أرقام + ١) = ٢٠.

    // نمط إعادة_حجز الآمن: لا تفقد المؤشر الأصلي إذا فشلت العملية.
    صحيح* مؤقت = إعادة_حجز(أرقام، ٨ * حجم(صحيح)).
    إذا (مؤقت != عدم) {
        أرقام = مؤقت.
    }

    تحرير_ذاكرة(أرقام).
    إرجع ٠.
}
```

### File I/O (إدخال/إخراج الملفات)

يدعم باء (v0.3.12) عمليات الملفات عبر واجهات رقيقة فوق `stdio` في C.

**ملاحظات:**

- نوع مقبض الملف هو `عدم*` (يمثل `FILE*` بشكل معتم).
- `فتح_ملف` يعيد `عدم` عند الفشل.
- `اقرأ_سطر` يعيد `عدم` عند EOF قبل قراءة أي بايت.
- السطر المعاد من `اقرأ_سطر` يجب تحريره عبر `حرر_نص`.

```baa
#تضمين "stdlib/baalib.baahd"

صحيح الرئيسية() {
    عدم* ف = فتح_ملف("بيانات.txt", "قراءة").
    إذا (ف == عدم) { إرجع ١. }

    طالما (صواب) {
        نص س = اقرأ_سطر(ف).
        إذا (س == عدم) { توقف. }
        اطبع س.
        حرر_نص(س).
    }

    اغلق_ملف(ف).
    إرجع ٠.
}
```

### Compound Types (Structs, Enums, Unions)

Group related data using `هيكل` (structs), `تعداد` (enums), and `اتحاد` (unions). Use `:` to access members.

```baa
تعداد حالة_النظام {
    مغلق،
    يعمل
}

اتحاد قيمة {
    صحيح رقم.
    نص نص_قيمة.
}

هيكل خادم {
    صحيح رقم.
    تعداد حالة_النظام حالة.
}

صحيح الرئيسية() {
    هيكل خادم خ.
    خ:رقم = ١٠٤.
    خ:حالة = حالة_النظام:يعمل.
    
    إذا (خ:حالة == حالة_النظام:يعمل) {
        اطبع "الخادم يعمل!".
        اطبع خ:رقم.
    }
    
    إرجع ٠.
}
```

### Static Variables

Use `ساكن` to declare variables that persist between function calls.

```baa
صحيح عداد() {
    ساكن صحيح ع = ٠.
    ع = ع + ١.
    إرجع ع.
}

صحيح الرئيسية() {
    اطبع عداد(). // ١
    اطبع عداد(). // ٢
    إرجع ٠.
}
```

### Pointers and References

Use `&` to get the address of a variable, and `*` to dereference it. `عدم` represents a null pointer.

```baa
صحيح الرئيسية() {
    صحيح س = ٤٢.
    صحيح* مؤشر = &س.
    
    إذا (مؤشر != عدم) {
        *مؤشر = ١٠٠. // يغير قيمة س إلى ١٠٠
    }
    
    // Pointer arithmetic
    صحيح مصفوفة[٥] = {١٠، ٢٠، ٣٠، ٤٠، ٥٠}.
    صحيح* م = &مصفوفة[٠].
    م = م + ٢.  // Points to مصفوفة[٢]
    اطبع *م.    // يطبع ٣٠
    
    اطبع س. // يطبع ١٠٠
    إرجع ٠.
}
```

### Type Casting

Use `كـ<النوع>(...)` for explicit conversions between numeric types or pointers.

```baa
صحيح الرئيسية() {
    // Convert integer to character
    صحيح رقم = ٦٥.
    حرف ح = كـ<حرف>(رقم).
    اطبع ح. // يطبع 'A'
    
    // Pointer arithmetic using casts
    صحيح* م = عدم.
    ط٦٤ عنوان = كـ<ط٦٤>(م).
    عنوان = عنوان + ٨.
    
    // Cast pointer to different type
    صحيح* مؤشر_صحيح = &رقم.
    ط٨* مؤشر_بايت = كـ<ط٨*>(مؤشر_صحيح).
    
    إرجع ٠.
}
```

### Function Pointers (v0.3.10.6)

Pass functions as values or store them in variables using the `دالة(...) -> ...` type syntax.

```baa
صحيح جمع(صحيح أ، صحيح ب) {
    إرجع أ + ب.
}

صحيح ضرب(صحيح أ، صحيح ب) {
    إرجع أ * ب.
}

// Function that takes a function pointer as parameter
صحيح طبق(دالة(صحيح، صحيح) -> صحيح ف، صحيح أ، صحيح ب) {
    إرجع ف(أ، ب).
}

صحيح الرئيسية() {
    // Declare a function pointer variable
    دالة(صحيح، صحيح) -> صحيح عملية = جمع.
    
    // Call through the pointer
    صحيح نتيجة = عملية(٥، ٤).
    اطبع نتيجة. // يطبع ٩
    
    // Assign different function
    عملية = ضرب.
    نتيجة = عملية(٥، ٤).
    اطبع نتيجة. // يطبع ٢٠
    
    // Pass function pointer as argument
    نتيجة = طبق(جمع، ١٠، ٢٠).
    اطبع نتيجة. // يطبع ٣٠
    
    // Null function pointer
    عملية = عدم.
    إذا (عملية == عدم) {
        اطبع "المؤشر فارغ".
    }
    
    إرجع ٠.
}
```

### Boolean Logic

```baa
صحيح الرئيسية() {
    منطقي جاهز = صواب.
    منطقي مغلق = خطأ.
    
    إذا (جاهز && !مغلق) {
        اطبع "النظام جاهز!".
    }
    
    إرجع ٠.
}
```

### Low-Level Operations

Bitwise operators, shifts, `حجم(...)`, and `عدم` are available for systems-level code.

```baa
عدم اطبع_رسالة() {
    اطبع "مرحباً".
    إرجع.
}

صحيح الرئيسية() {
    صحيح أ = ٥ & ٣.      // Bitwise AND: ١
    صحيح ب = ١ << ٤.     // Left shift: ١٦
    صحيح ج = ~٠.         // Bitwise NOT: -١
    صحيح د = ٥ \| ٣.      // Bitwise OR: ٧
    صحيح هـ = ٥ ^ ٣.      // Bitwise XOR: ٦
    صحيح و = ١٦ >> ٢.    // Right shift: ٤
    صحيح ح = حجم(صحيح).  // Size of integer
    اطبع_رسالة().
    إرجع أ + ب + ح.
}
```

### Integer Types with Specific Sizes

Baa supports fixed-width integer types:

```baa
صحيح الرئيسية() {
    // Signed integers
    ص٨  صغير = ١٢٧.        // 8-bit signed
    ص١٦ متوسط = ١٠٠٠٠.    // 16-bit signed
    ص٣٢ كبير = ١٠٠٠٠٠٠.   // 32-bit signed
    ص٦٤ ضخم = ١٠٠٠٠٠٠٠٠٠٠٠٠٠. // 64-bit signed
    
    // Unsigned integers
    ط٨  طبيعي_صغير = ٢٥٥.       // 8-bit unsigned
    ط١٦ طبيعي_متوسط = ٦٥٥٣٥.   // 16-bit unsigned
    ط٣٢ طبيعي_كبير = ٤٠٠٠٠٠٠٠٠٠. // 32-bit unsigned
    ط٦٤ طبيعي_ضخم = ١٠٠٠٠٠٠٠٠٠٠٠٠٠. // 64-bit unsigned
    
    // Regular صحيح is platform-dependent (usually 32-bit)
    صحيح عادي = ١٠٠.
    
    إرجع ٠.
}
```

### Reading User Input

Use `اقرأ` to read integers from the user:

```baa
صحيح الرئيسية() {
    اطبع "أدخل عمرك: ".
    صحيح عمر.
    اقرأ عمر.
    اطبع "عمرك هو:".
    اطبع عمر.
    إرجع ٠.
}
```

---

## 9. Testing and QA

Baa includes a comprehensive test suite to ensure compiler correctness.

### Running Tests

From the repo root:

```powershell
# Recommended quick path (integration tests only)
python .\scripts\qa_run.py --mode quick

# Recommended full QA gate (includes regression + negative tests)
python .\scripts\qa_run.py --mode full

# Stress testing (full + stress suite + fuzz-lite)
python .\scripts\qa_run.py --mode stress

# Direct integration test runner
python tests/test.py

# Regression test runner
python tests/regress.py
```

### Test Organization

```
tests/
├── integration/
│   ├── backend/   # Integration tests that compile and run
│   └── ir/        # IR-specific integration tests
├── neg/           # Negative tests (expect compiler to fail)
├── stress/        # Stress tests for performance
├── fixtures/      # Helper files for tests
├── test.py        # Integration test runner
└── regress.py     # Regression test runner
```

### Running a Single Test (Manual)

**Integration test:**

```powershell
# Windows
build\baa.exe -O2 --verify tests\integration\backend\backend_test.baa -o build\backend_test.exe
build\backend_test.exe

# Linux
./build-linux/baa -O2 --verify tests/integration/backend/backend_test.baa -o build-linux/backend_test
./build-linux/backend_test
```

**Compile-only integration test:**

```bash
# Linux
./build-linux/baa -O2 --verify tests/integration/ir/ir_test.baa -o build-linux/ir_test

# Windows
build\baa.exe -O2 --verify tests\integration\ir\ir_test.baa -o build\ir_test.exe
```

**Negative test (expect compiler failure):**

```powershell
# Windows
build\baa.exe -O1 tests\neg\semantic_deref_non_pointer.baa -o build\neg_tmp.exe
# Should produce error about dereferencing non-pointer
# Confirm stderr includes the test // EXPECT: marker text

# Linux
./build-linux/baa -O1 tests/neg/semantic_deref_non_pointer.baa -o build-linux/neg_tmp
# Confirm stderr includes the test // EXPECT: marker text
```

### Test Metadata Conventions

Test files can include special comments:

- `// RUN:` - Test contract: `expect-pass`, `expect-fail`, `runtime`, `compile-only`, `skip`
- `// EXPECT:` - Required diagnostic substring(s) for negative tests
- `// EXPECT-NOT:` - Forbidden diagnostic substring(s) for negative tests
- `// EXPECT-DIAG-COUNT:` - Required number of emitted `[Error]`/`[Warning]` diagnostics
- `// FLAGS:` - Extra compiler flags for that specific test file

---

## 10. Troubleshooting

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `خطأ لفظي: ...` | Unsupported byte, malformed UTF-8, or malformed literal | Ensure the file is UTF-8 and literals are closed correctly |
| `خطأ نحوي: ...` | Missing token (`.`/`}`/`)`), or malformed statement/declaration | Fix the reported token location; `مساعدة:` lines may suggest the common fix |
| `[Error] <file>:<line>:<col>: ...` | Syntax error (parser) | Check for missing `.` at statement end, missing `}`, or unmatched parentheses |
| `[Error] ... خطأ دلالي: ... متغير غير معرّف ...` | Variable used before declaration | Declare variables before using them or correct the name |
| `[Semantic Error] ... عدم تطابق أنواع ...` | Assigning wrong type | Keep assignment/call/return types compatible |
| `[Error] ... خطأ دلالي: ... ثابت ...` | Modifying a constant | Constants declared with `ثابت` cannot be changed after initialization |
| `Aborting <file> due to syntax errors.` | Parser reported one or more errors | Fix reported `[Error]` diagnostics and recompile |
| `Aborting <file> due to semantic errors.` | Analyzer reported one or more errors | Fix reported `[Semantic Error]` messages and recompile |

### Common Warnings (v0.2.8+)

| Warning | Cause | Solution |
|---------|-------|----------|
| `[-Wunused-variable] Variable 'x' is declared but never used.` | Unused variable | Remove the variable or use it |
| `[-Wunused-variable] Global variable 'x' is declared but never used.` | Unused global | Remove it or reference it |
| `[-Wdead-code] Unreachable code after 'return/break' statement.` | Dead code after terminator | Remove code after `إرجع`/`توقف`/`استمر` |
| `[-Wshadow-variable] Local variable 'x' shadows global variable.` | Variable shadowing | Rename the local variable |

**Note:** Semantic analysis generates precise line and column numbers.

> **Note:** Warnings are non-fatal by default. Use `-Werror` to treat them as errors.

### Getting Help

- Check the [Language Specification](LANGUAGE.md) for complete syntax reference
- See [Compiler Internals](INTERNALS.md) for technical details
- Open an issue on GitHub for bugs

---

*[← README](../README.md) | [Language Specification →](LANGUAGE.md)*
