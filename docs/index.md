# Baa (باء) Programming Language

**The First Arabic-Native Systems Programming Language**

---

## ✨ Welcome

Baa (باء) is a modern programming language that brings Arabic syntax to systems programming. Write efficient, type-safe code in your native Arabic language.

```baa
// Hello World in Baa
عدد_صحيح الرئيسية() {
    طباعة("مرحباً بالعالم!").
    إرجع ٠.
}
```

---

## 🚀 Quick Start

| Step | Action |
|------|--------|
| **1. Prerequisites** | Install CMake 3.20+, C11 compiler (Clang/GCC/MSVC) |
| **2. Clone** | `git clone https://github.com/OmarAglan/Baa.git` |
| **3. Build** | `mkdir build && cd build && cmake .. && cmake --build .` |
| **4. Run** | `./baa program.ب` |

📖 **[Full Quick Start Guide →](00_OVERVIEW/QUICK_START.md)**

---

## 📊 Current Status

| Component | Status | Version |
|-----------|--------|---------|
| **Preprocessor** | ✅ Production Ready | v0.1.27.0 |
| **Lexer** | ✅ Production Ready | v0.1.27.0 |
| **Parser** | ✅ Production Ready | v0.1.27.0 |
| **AST** | ✅ Production Ready | v0.1.27.0 |
| **Semantic Analysis** | 📋 Next Phase | — |
| **Code Generation** | 🔄 Stub | — |

📖 **[Full Status Report →](00_OVERVIEW/CURRENT_STATUS.md)**

---

## 📚 Documentation

### For Users
| Document | Description |
|----------|-------------|
| [**Quick Start**](00_OVERVIEW/QUICK_START.md) | Get running in 5 minutes |
| [**Tutorial**](01_LANGUAGE_SPECIFICATION/TUTORIAL.md) | Step-by-step learning |
| [**Language Overview**](01_LANGUAGE_SPECIFICATION/LANGUAGE_OVERVIEW.md) | Complete language spec |
| [**Arabic Features**](01_LANGUAGE_SPECIFICATION/ARABIC_FEATURES.md) | Arabic programming guide |
| [**Syntax Reference**](01_LANGUAGE_SPECIFICATION/SYNTAX_REFERENCE.md) | Detailed syntax |

### For Contributors
| Document | Description |
|----------|-------------|
| [**Building**](03_DEVELOPMENT/BUILDING.md) | Build from source |
| [**Contributing**](03_DEVELOPMENT/CONTRIBUTING.md) | Contribution guidelines |
| [**Testing**](03_DEVELOPMENT/TESTING.md) | Test framework |
| [**Architecture**](02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md) | Compiler design |

### API Reference
| API | Description |
|-----|-------------|
| [**Preprocessor**](05_API_REFERENCE/PREPROCESSOR_API.md) | `#تضمين`, `#تعريف` handling |
| [**Lexer**](05_API_REFERENCE/LEXER_API.md) | Tokenization functions |
| [**Parser**](05_API_REFERENCE/PARSER_API.md) | Parsing functions |
| [**AST**](05_API_REFERENCE/AST_API.md) | Abstract Syntax Tree |
| [**CodeGen**](05_API_REFERENCE/CODEGEN_API.md) | Code generation |
| [**Utilities**](05_API_REFERENCE/UTILITIES_API.md) | Helper functions |

### Compiler Internals
| Document | Description |
|----------|-------------|
| [**Visual Overview**](02_COMPILER_ARCHITECTURE/VISUAL_OVERVIEW.md) | Diagrams |
| [**Preprocessor**](02_COMPILER_ARCHITECTURE/PREPROCESSOR.md) | Preprocessor internals |
| [**Lexer**](02_COMPILER_ARCHITECTURE/LEXER.md) | Lexer internals |
| [**Parser**](02_COMPILER_ARCHITECTURE/PARSER.md) | Parser internals |
| [**AST**](02_COMPILER_ARCHITECTURE/AST.md) | AST design |

---

## 🌟 Key Features

### Arabic-Native Syntax
```baa
// Arabic keywords, identifiers, and numerals
عدد_صحيح المستخدم = ٤٢.
إذا (المستخدم > ٠) {
    طباعة("موجب").
}
```

### C99-Compatible
```baa
// C-style syntax with Arabic keywords
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}
```

### Rich Error Messages
```
program.ب:15:8: خطأ: متغير غير معرف 'العدد'
                تلميح: هل قصدت 'العدد_الأول'؟
```

---

## 🔗 Quick Links

| Resource | Link |
|----------|------|
| **GitHub** | [github.com/OmarAglan/Baa](https://github.com/OmarAglan/Baa) |
| **Issues** | [Report bugs](https://github.com/OmarAglan/Baa/issues) |
| **Roadmap** | [Development plans](04_ROADMAP/ROADMAP_OVERVIEW.md) |

---

**Version:** v0.1.27.0 | **Updated:** 2025-12-05

**أهلاً بكم في لغة باء!** 🎉
