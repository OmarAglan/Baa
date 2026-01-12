<div dir="rtl">

<div align="center">
<img width="260" height="260" alt="gpt-image-1 5_b_A_professional_logo_ (1)" src="https://github.com/user-attachments/assets/9b1c29f2-5dd7-4a38-a1be-19be6afe64f6" />


![Version](https://img.shields.io/badge/version-0.2.7-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**The first Arabic-syntax compiled systems programming language**

*Write native Windows applications using Arabic keywords, numerals, and punctuation*

</div>

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 🖥️ **Native Compilation** | Compiles to x86-64 Assembly → Native Windows Executables |
| 🌍 **Full Arabic Syntax** | Arabic keywords, numerals (٠-٩), and punctuation (`.` `؛`) |
| 🧩 **Modular Code** | `#تضمين` (Include), multi-file compilation, `.baahd` headers |
| 🔧 **Preprocessor** | `#تعريف` (Define), `#إذا_عرف` (Ifdef), `#الغاء_تعريف` (Undefine) |
| ⚡ **Functions** | Define and call functions with parameters and return values |
| 📦 **Arrays** | Fixed-size stack arrays (`صحيح قائمة[١٠]`) |
| 🔄 **Control Flow** | `إذا`/`وإلا` (If/Else), `طالما` (While), `لكل` (For) |
| 🎯 **Advanced Control** | `اختر` (Switch), `حالة` (Case), `افتراضي` (Default), `توقف` (Break), `استمر` (Continue) |
| ➕ **Full Operators** | Arithmetic, comparison, and logical operators with short-circuit evaluation |
| 📝 **Text Support** | String (`"..."`) and character (`'...'`) literals |
| ✅ **Type Safety** | Static type checking (v0.2.4+) with semantic analysis |
| 🔄 **Self-Updating** | Built-in updater (`baa update`) |

---

## 🚀 Quick Start

### 1. Build the Compiler

**Prerequisites:** Windows, PowerShell, [CMake](https://cmake.org/) 3.10+, [MinGW-w64](https://www.mingw-w64.org/) with GCC

```powershell
git clone https://github.com/YourUsername/Baa.git
cd Baa
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. Write Your First Program

Create `hello.baa` (⚠️ **IMPORTANT:** Save as **UTF-8** encoding):

```baa
صحيح الرئيسية() {
    اطبع "مرحباً بالعالم!".
    إرجع ٠.
}
```

### 3. Compile & Run

```powershell
# Compile
.\baa.exe ..\hello.baa

# Run
.\out.exe
```
**Output:** `مرحباً بالعالم!`

---

## 📖 Example: Array Sum

```baa
// حساب مجموع مصفوفة
صحيح الرئيسية() {
    // Declare array of 5 integers
    صحيح قائمة[٥].
    صحيح مجموع = ٠.

    // Fill array with values 0, 10, 20, 30, 40
    لكل (صحيح س = ٠؛ س < ٥؛ س++) {
        قائمة[س] = س * ١٠.
    }

    // Sum all values
    لكل (صحيح س = ٠؛ س < ٥؛ س++) {
        مجموع = مجموع + قائمة[س].
    }

    اطبع "المجموع هو: ".
    اطبع مجموع.
    
    إرجع ٠.
}
```
**Output:** `المجموع هو: 100` (0 + 10 + 20 + 30 + 40)

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/USER_GUIDE.md) | Getting started and basic usage |
| [Language Specification](docs/LANGUAGE.md) | Complete syntax and features reference |
| [Compiler Internals](docs/INTERNALS.md) | Architecture and implementation details |
| [API Reference](docs/API_REFERENCE.md) | Internal C API documentation |
| [Roadmap](ROADMAP.md) | Future development plans |
| [Changelog](CHANGELOG.md) | Version history |

---

## 🛠️ Building from Source

### Prerequisites

- **CMake** 3.10+
- **MinGW-w64** with GCC
- **PowerShell** (Windows)
- **Git** (for cloning)

### Build Steps

```powershell
# Clone the repository
git clone https://github.com/OmarAglan/Baa.git
cd Baa

# Create build directory
mkdir build
cd build

# Generate and build
cmake ..
cmake --build .

# The compiler is now at: build/baa.exe
```

### Running Tests

```powershell
# Generate test file
gcc ../make_test.c -o make_test
./make_test

# Compile and run
.\baa.exe ..\test.baa
.\out.exe
```

**Expected Output:**
```
100
1
2
```
</div>
