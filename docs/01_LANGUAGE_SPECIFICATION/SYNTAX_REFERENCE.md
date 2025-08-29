# Baa Language Syntax Reference

**Available in:** [English](#) | [العربية](../01_مواصفات_اللغة/مرجع_النحو.md)

**Status:** 📋 Planned  
**Last Updated:** 2025-01-09  
**Version Compatibility:** v1.0.0+  

## Overview

This document provides a comprehensive syntax reference for the Baa programming language. It covers all language constructs, operators, keywords, and syntax rules in detail.

> **Note:** This document is currently planned for development. The syntax information is distributed across other documentation files until this comprehensive reference is completed.

## Planned Contents

### 1. Language Fundamentals
- Character sets and encoding (UTF-8/UTF-16)
- Identifiers and naming rules
- Keywords and reserved words
- Comments and documentation

### 2. Data Types
- Primitive types (`عدد_صحيح`, `عدد_حقيقي`, `حرف`, `منطقي`)
- Array types
- Pointer types (planned)
- User-defined types (planned)

### 3. Operators
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`)
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)
- Logical operators (`&&`, `||`, `!`)
- Bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`)
- Assignment operators (`=`, `+=`, `-=`, `*=`, `/=`)
- Increment/decrement (`++`, `--`)

### 4. Control Flow
- Conditional statements (`إذا`, `وإلا`)
- Loop statements (`طالما`, `لكل`, `افعل`)
- Jump statements (`إرجع`, `توقف`, `استمر`)
- Switch statements (`اختر`, `حالة`) (planned)

### 5. Functions
- Function declarations and definitions
- Parameter passing
- Return values
- Function overloading (planned)

### 6. Variables and Constants
- Variable declarations
- Constant declarations (`ثابت`)
- Scope and lifetime
- Storage classes (`ساكن`, `خارجي`, `تلقائي`)

### 7. Arrays and Collections
- Array declaration and initialization
- Multi-dimensional arrays
- Dynamic arrays (planned)

### 8. Preprocessor Directives
- File inclusion (`#تضمين`)
- Macro definitions (`#تعريف`)
- Conditional compilation (`#إذا`, `#وإلا`, `#نهاية_إذا`)

### 9. Error Handling
- Error reporting mechanisms (planned)
- Exception handling (planned)

### 10. Memory Management
- Automatic memory management
- Manual memory management (planned)
- Garbage collection (planned)

## Current Status

The syntax information is currently available in the following documents:

### Core Language Features
- [Language Overview](LANGUAGE_OVERVIEW.md) - Basic syntax and features
- [Arabic Features](ARABIC_FEATURES.md) - Arabic-specific syntax elements
- [C Comparison](C_COMPARISON.md) - Syntax differences from C

### Architecture Documentation
- [Lexer Documentation](../02_COMPILER_ARCHITECTURE/LEXER.md) - Token definitions and lexical syntax
- [Parser Documentation](../02_COMPILER_ARCHITECTURE/PARSER.md) - Grammar rules and parsing
- [AST Documentation](../02_COMPILER_ARCHITECTURE/AST.md) - Abstract syntax tree structure

## Quick Syntax Examples

### Basic Program Structure
```baa
// A simple Baa program
عدد_صحيح الرئيسية() {
    طباعة("مرحبا بالعالم!");
    إرجع ٠;
}
```

### Variable Declarations
```baa
// Variable declarations with Arabic types
عدد_صحيح العدد = ١٠;
عدد_حقيقي السعر = ٢٥٫٥٠;
حرف الحرف = 'أ';
منطقي صحيح_أم_خطأ = صحيح;
```

### Control Flow
```baa
// Conditional statements
إذا (العدد > ٠) {
    طباعة("العدد موجب");
} وإلا {
    طباعة("العدد سالب أو صفر");
}

// Loop statements
طالما (العدد > ٠) {
    طباعة(العدد);
    العدد = العدد - ١;
}
```

### Functions
```baa
// Function definition
عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب;
}

// Function call
عدد_صحيح النتيجة = جمع(٥، ٣);
```

## Grammar Notation

When this document is completed, it will use the following notation for grammar rules:

- `Terminal` - Literal tokens in the language
- `<non-terminal>` - Grammar rules
- `|` - Alternatives
- `[]` - Optional elements
- `{}` - Zero or more repetitions
- `()` - Grouping

## Contributing

To help complete this syntax reference:

1. Review existing syntax information in other documents
2. Identify gaps in syntax coverage
3. Create detailed grammar rules
4. Add comprehensive examples
5. Test all syntax examples with the compiler

## See Also

- [Language Overview](LANGUAGE_OVERVIEW.md) - High-level language introduction
- [Arabic Features](ARABIC_FEATURES.md) - Arabic language capabilities
- [Parser Documentation](../02_COMPILER_ARCHITECTURE/PARSER.md) - Grammar implementation
- [Lexer Documentation](../02_COMPILER_ARCHITECTURE/LEXER.md) - Token definitions

---

*This document is part of the [Baa Language Documentation](../NAVIGATION.md)*
