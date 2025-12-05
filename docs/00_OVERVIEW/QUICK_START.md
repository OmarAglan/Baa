# Baa Language Quick Start Guide

**Get started with Baa in 5 minutes!**

**Status:** ✅ Complete  
**Last Updated:** 2025-12-05  
**Version:** v0.1.27.0+

---

## 1. Prerequisites

- **CMake** (3.20 or higher)
- **C11 compliant compiler** (GCC, Clang, or MSVC)
- **Git**
- **(Optional)** LLVM development libraries

## 2. Clone and Build

```bash
# Clone the repository
git clone https://github.com/OmarAglan/baa.git
cd baa

# Create build directory
mkdir build
cd build

# Configure (Linux/macOS)
cmake -G "Ninja" ..

# Configure (Windows with LLVM/Clang)
cmake -G "Ninja" -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ..

# Build
cmake --build .
```

## 3. Test the Installation

```bash
# Test the lexer
./tools/baa_lexer_tester ../tests/resources/lexer_test_cases/lexer_test_suite.baa

# Test the preprocessor
./tools/baa_preprocessor_tester ../tests/resources/preprocessor_test_cases/preprocessor_test_all.baa

# Test the parser (if available)
./tools/baa_parser_tester ../tests/resources/parser_tests/valid_simple.baa
```

## 4. Your First Baa Program

Create a file called `hello.baa`:

```baa
// hello.baa - Your first Baa program
عدد_صحيح رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع ٠.
}
```

> **Note:** Baa uses `.` (period) as the statement terminator, not `;` (semicolon).

## 5. Language Features Overview

### Arabic Keywords

```baa
// Variables and constants
عدد_صحيح العدد = ١٠.
ثابت عدد_حقيقي PI = ٣٫١٤١٥٩.
منطقي النتيجة = صحيح.

// Functions
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}

// Control flow
إذا (العدد > ٥) {
    اطبع("العدد أكبر من خمسة").
} وإلا {
    اطبع("العدد خمسة أو أقل").
}

// Loops
طالما (العدد < ٢٠) {
    العدد = العدد + ١.
}

لكل (عدد_صحيح ي = ٠; ي < ١٠; ي++) {
    اطبع(ي).
}
```

### Arabic Numerals and Strings

```baa
// Arabic-Indic numerals
عدد_صحيح عدد_عربي = ١٢٣٤٥.
عدد_حقيقي رقم_عشري = ٣٫١٤.

// Scientific notation with Arabic exponent marker
عدد_حقيقي كبير = ١٫٢٣أ٦.  // 1.23 × 10^6

// Arabic escape sequences
حرف سطر_جديد = '\س'.     // Newline
حرف تاب = '\م'.           // Tab
نص رسالة = "مرحبا\سبالعالم".  // "Hello\nWorld"

// Unicode escape
حرف همزة = '\ي0623'.      // Arabic letter Alef with Hamza above
```

## 6. Key Syntax Differences from C

| Feature | C | Baa |
|---------|---|-----|
| Statement terminator | `;` | `.` |
| Keywords | English | Arabic (`إذا`, `طالما`, `إرجع`) |
| Struct member access | `.` | `::` |
| Exponent marker | `e`/`E` | `أ` |
| Escape sequences | `\n`, `\t` | `\س`, `\م` |

## 7. Next Steps

1. **Read the documentation**: Start with [Language Specification](../01_LANGUAGE_SPECIFICATION/LANGUAGE_OVERVIEW.md)
2. **Explore examples**: Check out test files in `tests/resources/`
3. **Learn the architecture**: Read [Architecture Overview](../02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)
4. **Contribute**: See [Development Guide](../03_DEVELOPMENT/CONTRIBUTING.md)

## 8. Getting Help

- **Documentation**: [Complete Documentation Index](../index.md)
- **Issues**: Create an issue on the project repository
- **Community**: Join discussions about Arabic programming languages

---

## Common Issues and Solutions

### Build Issues

**Problem**: `CMake Error: CMAKE_C_COMPILER not set`  
**Solution**: Install a C compiler or specify the compiler path:
```bash
cmake -DCMAKE_C_COMPILER=gcc ..
# or
cmake -DCMAKE_C_COMPILER=clang ..
```

### Character Encoding

**Problem**: Arabic text appears garbled  
**Solution**: Ensure your source files are saved as UTF-8 or UTF-16LE with BOM

### Tool Output

**Problem**: Lexer test output not visible  
**Solution**: Check the generated `lexer_test_output.txt` file in your current directory

---

## Feature Checklist

Use this checklist to verify your Baa installation is working correctly:

- [ ] ✅ **Preprocessor**: Can handle `#تضمين` and `#تعريف` directives
- [ ] ✅ **Lexer**: Tokenizes Arabic keywords and numerals correctly  
- [ ] ✅ **Parser**: Parses function definitions and calls
- [ ] ✅ **AST**: Builds complete abstract syntax trees
- [ ] 📋 **Semantic Analysis**: Symbol resolution *(Next Phase)*
- [ ] 📋 **Code Generation**: LLVM IR output *(Next Phase)*

---

**Ready to start programming in Arabic? Welcome to Baa!** 🎉
