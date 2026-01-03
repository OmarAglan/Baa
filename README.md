# Baa (باء) Programming Language

<div align="center">

![Version](https://img.shields.io/badge/version-0.2.4-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**The first Arabic-syntax compiled systems programming language**

*Write native Windows applications using Arabic keywords, numerals, and punctuation*

</div>

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 🖥️ **Native Compilation** | Compiles directly to x86_64 Assembly → Windows Executable |
| 🌍 **Full Arabic Syntax** | Arabic keywords, numerals (٠-٩), and punctuation (`.` `؛`) |
| ⚡ **Functions** | Define and call functions with parameters and return values |
| 📦 **Arrays** | Fixed-size stack arrays (`صحيح قائمة[١٠]`) |
| 🔄 **Control Flow** | `إذا` (If), `طالما` (While), `لكل` (For), `توقف` (Break), `استمر` (Continue) | `اختر` (Switch), `حالة` (Case), `افتراضي` (Default) |
| ➕ **Full Operators** | Arithmetic, comparison, and logical operators with short-circuit evaluation |
| 📝 **Text Support** | String (`"..."`) and character (`'...'`) literals |

---

## 🚀 Quick Start

### 1. Build the Compiler

**Prerequisites:** PowerShell, [CMake](https://cmake.org/), [MinGW-w64](https://www.mingw-w64.org/) (GCC)

```powershell
git clone https://github.com/YourUsername/Baa.git
cd Baa
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. Write Your First Program

Create `hello.baa` (save as **UTF-8**):

```baa
صحيح الرئيسية() {
    اطبع "مرحباً بالعالم!".
    إرجع ٠.
}
```

### 3. Compile & Run

```powershell
.\baa.exe ..\hello.baa
.\out.exe
```

---

## 📖 Example: Array Sum

```baa
// حساب مجموع مصفوفة
صحيح الرئيسية() {
    صحيح قائمة[٥].
    صحيح مجموع = ٠.

    // ملء المصفوفة بالقيم ٠، ١٠، ٢٠، ٣٠، ٤٠
    لكل (صحيح س = ٠؛ س < ٥؛ س++) {
        قائمة[س] = س * ١٠.
    }

    // جمع كل القيم
    لكل (صحيح س = ٠؛ س < ٥؛ س++) {
        مجموع = مجموع + قائمة[س].
    }

    اطبع "المجموع هو: ".
    اطبع مجموع.
    
    إرجع ٠.
}
```

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/USER_GUIDE.md) | Getting started and basic usage |
| [Language Specification](docs/LANGUAGE.md) | Complete syntax reference |
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
```

### Running Tests

```powershell
# From the build directory
.\baa.exe ..\test_suite.baa
.\out.exe
```