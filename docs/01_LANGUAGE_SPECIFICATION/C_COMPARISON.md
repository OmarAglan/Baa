# C99 and Baa (باء) Comparison

**Last Updated:** 2025-12-05  
**Version:** v0.1.27.0

## Overview

This document compares C99 language features with their Baa (باء) equivalents. Baa aims for conceptual alignment with many C features while introducing Arabic syntax and modern constructs.

**Status Legend:**
- **✅ Complete** - Fully implemented (Lexer, Parser, AST)
- **🔄 In Progress** - Parsing works, semantic analysis pending
- **📋 Planned** - Future work

## Language Features

### Basic Types

| C99 Feature     | Baa (باء) Equivalent        | Status                 | Notes                                                                 |
| ----------------- | ------------------------- | ---------------------- | --------------------------------------------------------------------- |
| `char`          | `حرف`                     | ✅ Complete | Baa `حرف` is 16-bit (`wchar_t`) for Unicode.                          |
| `int`           | `عدد_صحيح`                | ✅ Complete | Typically 32-bit signed integer.                                        |
| `float`         | `عدد_حقيقي`               | ✅ Complete | Typically 32-bit floating point. Literal suffix `ح`.                    |
| `double`        | `عدد_حقيقي` (modifier)    | 🔄 In Progress | Baa `عدد_حقيقي` might be `float` or `double` based on context/suffix. |
| `long double`   | (No direct equivalent yet)  | 📋 Planned       |                                                                       |
| `void`          | `فراغ`                    | ✅ Complete | Represents no value.                                                  |
| `_Bool`/`bool`  | `منطقي`                   | ✅ Complete | Literals: `صحيح` (true), `خطأ` (false).                             |
| `short int`     | `عدد_صحيح` (modifier)    | 🔄 In Progress |                                                                       |
| `long int`      | `عدد_صحيح` (modifier `ط`)  | 🔄 In Progress | Via suffix `ط` on literals. Type system needs to distinguish.       |
| `long long int` | `عدد_صحيح_طويل_جدا`       | 🔄 In Progress | Keyword planned. Suffix `طط` on literals. |
| `unsigned char` | `حرف` (modifier `غ`)      | 🔄 In Progress | Via suffix `غ` on char literals if applicable, or type system.     |
| `unsigned int`  | `عدد_صحيح` (modifier `غ`)  | 🔄 In Progress | Via suffix `غ` on int literals. Type system needs to distinguish.        |
| `enum`          | `تعداد`                    | 🔄 In Progress | Keyword `تعداد` exists, parser support pending.                        |

### Derived Types

| C99 Feature  | B (باء) Equivalent         | Status               | Notes                                                                           |
| -------------- | -------------------------- | -------------------- | ------------------------------------------------------------------------------- |
| Arrays         | `مصفوفة` (implicit type)   | Planned (Parser/AST) | Array syntax `type name[]` or `type name[size]`. `BAA_TYPE_ARRAY` exists.         |
| Pointers       | `مؤشر` (implicit type)      | Planned (Full)       | Full pointer arithmetic and dereferencing.                                      |
| Structures     | `بنية`                      | Planned (Parser/AST) | Member access: `::` (direct), `->` (pointer). C99 flexible array members.   |
| Unions         | `اتحاد`                     | Planned (Parser/AST) | Member access: `::` (direct), `->` (pointer).                                 |

### Storage Class Specifiers

| C99 Feature  | B (باء) Equivalent | Status               | Notes                                                                |
| -------------- | ------------------ | -------------------- | -------------------------------------------------------------------- |
| `auto`         | (No direct keyword)| N/A                  | Baa relies on lexical scope for automatic storage duration.           |
| `static`       | `مستقر`            | Planned (Parser/AST) | For static storage duration and internal linkage.                     |
| `extern`       | `خارجي`            | Planned (Parser/AST) | For external linkage.                                                |
| `register`     | `سجل` (tentative)  | Planned (Low Priority) | Hint to compiler, often ignored by modern compilers.                   |
| `typedef`      | `نوع_مستخدم`       | Planned (Parser/AST) | To create aliases for types.                                         |

### Type Qualifiers

| C99 Feature | B (باء) Equivalent | Status                     | Notes                                                              |
| ------------- | ------------------ | -------------------------- | ------------------------------------------------------------------ |
| `const`       | `ثابت`            | Implemented (Lexer/PP) / Planned (Semantic) | Keyword recognized. Semantic enforcement of const-correctness needed. |
| `volatile`    | `متطاير`           | Planned (Parser/AST)       | For memory that can change unexpectedly.                           |
| `restrict`    | `مقيد`             | Implemented (Lexer) / Planned (Semantic/CodeGen) | Hint for pointer optimization. Semantic validation needed.        |

### Literals

| C99 Feature               | B (باء) Equivalent & Extensions                                                                  | Status (Lexer/PP)   | Notes                                                                                                                                                                 |
| ------------------------- | ------------------------------------------------------------------------------------------------ | ------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Integer (dec, oct, hex)   | Dec (Western & Arabic-Indic `٠-٩`), Bin (`0b`), Hex (`0x`). Underscores (`_`). Suffixes `غ`,`ط`,`طط`. | Implemented         | Baa does not explicitly support C-style octal literals (e.g., `012`). Arabic suffixes `غ`(U), `ط`(L), `طط`(LL) are tokenized. Interpretation by Parser/Semantics. |
| Floating-Point            | Dec point (`.` or `٫`), Sci-notation (`أ`). Suffix `ح`. Underscores. Arabic-Indic digits.       | Implemented (Lexer with `أ`, `ح`) | Arabic decimal `٫` supported. `e`/`E` for exponent not supported.                                                                                                     |
| Character (`'c'`, `L'c'`) | `'حرف'` (16-bit `wchar_t`). Baa Arabic escapes only (e.g., `\س`, `\م`, `\يXXXX`, `\هـHH`).       | Implemented (Lexer) | C-style `\n`, `\t`, `\uXXXX`, `\xHH` are **not** supported. Uses `\` as escape char.                                                                                 |
| String (`"s"`, `L"s"`)    | `"نص"`. Multiline `"""..."""`. Raw `خ"..."`. Baa Arabic escapes only.                             | Implemented (Lexer) | C-style `\n`, `\t`, `\uXXXX`, `\xHH` are **not** supported. Internally UTF-16LE after preprocessing.                                                              |
| Boolean (`true`, `false` C++) | `صحيح`, `خطأ`                                                                                   | Implemented         | Baa has `منطقي` type and these literals.                                                                                                                               |
| Compound Literals (C99)   | `(اسم_النوع){قائمة_التهيئة}`                                                                      | Planned (Parser/AST) | e.g., `(عدد_صحيح[]){1,2,3}`.                                                                                                                            |

### Control Flow Statements

| C99 Feature  | B (باء) Equivalent    | Status (Lexer/PP) | Notes                                                                                                   |
| -------------- | --------------------- | ----------------- | ------------------------------------------------------------------------------------------------------- |
| `if`           | `إذا`                | Implemented       | Parsing/AST/Semantics planned.                                                                            |
| `else`         | `وإلا`               | Implemented       | Parsing/AST/Semantics planned. (Bug: Currently lexed as IDENTIFIER)                                      |
| `while`        | `طالما`              | Implemented       | Parsing/AST/Semantics planned.                                                                            |
| `do ... while` | `افعل ... طالما`      | Implemented (Lexer `افعل`) / Planned (Parser/AST) | Keyword `افعل` tokenized.                                                                             |
| `for`          | `لكل`                | Implemented       | C-style `for(init; cond; iter)`. Semicolons used. Parsing/AST/Semantics planned. Declaration in init planned. |
| `switch`       | `اختر`               | Implemented       | Parsing/AST/Semantics planned.                                                                            |
| `case`         | `حالة`               | Implemented       | Parsing/AST/Semantics planned.                                                                            |
| `default`      | `افتراضي` (assumed)   | Planned (Lexer/Parser/AST) | Keyword `افتراضي` not yet in lexer; assumed for C parity.                                                 |
| `break`        | `توقف`               | Implemented       | Parsing/AST/Semantics planned.                                                                            |
| `continue`     | `استمر`              | Implemented | Parsing/AST/Semantics planned.                                                                          |
| `return`       | `إرجع`               | Implemented       | Parsing/AST/Semantics planned.                                                                            |
| `goto`         | `اذهب` (tentative)   | Planned (Lexer/Parser/AST) | Not yet in lexer.                                                                                       |
| Labeled Stmts  | `معرف:` (tentative)   | Planned (Parser/AST) |                                                                                                         |

### Operators

(Baa uses standard C symbols for most operators)

| C99 Category / Op | B (باء) Symbol / Keyword | Status (Lexer/Core) | Notes                                                               |
| ----------------- | ------------------------ | ------------------- | ------------------------------------------------------------------- |
| Postfix `++` `--` | `++` `--`                | Implemented         | Parsing/AST/Semantics planned.                                        |
| Function Call `()`| `()`                     | Implemented         | Parsing/AST/Semantics planned.                                        |
| Array Subscript`[]`| `[]`                     | Implemented         | Parsing/AST/Semantics planned.                                        |
| Member `.` `->`   | `::` (direct), `->` (ptr)| Implemented (`->`) / Planned (`::`) | Baa uses `::` for direct struct/union member access.                |
| Unary `++` `--`   | `++` `--`                | Implemented         | Prefix. Parsing/AST/Semantics planned.                               |
| Unary `+` `-`     | `+` `-`                  | Implemented         | Parsing/AST/Semantics planned.                                        |
| Unary `!` `~`     | `!` `~`                  | Implemented         | Logical NOT `!`, Bitwise NOT `~`. Parsing/AST/Semantics planned.      |
| Unary `*` (deref) | `*`                      | Planned             | Requires pointer implementation.                                    |
| Unary `&` (addr)  | `&`                      | Planned             | Requires pointer implementation.                                    |
| `sizeof`          | `حجم`                    | Implemented (Lexer) / Planned (Semantic/CodeGen) |                                                                     |
| Cast `(type)`     | `(نوع)`                  | Planned (Parser/AST) |                                                                     |
| Multiplicative    | `*` `/` `%`              | Implemented         | Parsing/AST/Semantics planned.                                        |
| Additive          | `+` `-`                  | Implemented         | Parsing/AST/Semantics planned.                                        |
| Bitwise Shift     | `<<` `>>`                | Implemented (PP) / Planned (Parser/AST) | In preprocessor expressions. Parsing for general code planned.   |
| Relational        | `<` `>` `<=` `>=`        | Implemented         | Parsing/AST/Semantics planned.                                        |
| Equality          | `==` `!=`                | Implemented         | Parsing/AST/Semantics planned.                                        |
| Bitwise AND       | `&`                      | Implemented (PP) / Planned (Parser/AST) | In preprocessor expressions. Parsing for general code planned.   |
| Bitwise XOR       | `^`                      | Implemented (PP) / Planned (Parser/AST) | In preprocessor expressions. Parsing for general code planned.   |
| Bitwise OR        | `|`                      | Implemented (PP) / Planned (Parser/AST) | In preprocessor expressions. Parsing for general code planned.   |
| Logical AND       | `&&`                     | Implemented         | Parsing/AST/Semantics planned.                                        |
| Logical OR        | `||`                     | Implemented         | Parsing/AST/Semantics planned.                                        |
| Conditional `?:`  | `?:`                     | Planned (Parser/AST) |                                                                     |
| Assignment        | `=` `+=` `-=` etc.       | Implemented         | Parsing/AST/Semantics planned.                                        |
| Comma `,`         | `,`                      | Implemented         | Parsing/AST/Semantics planned (e.g., in function calls, for loops).   |

### Functions

| C99 Feature         | B (باء) Equivalent        | Status                      | Notes                                                                    |
| --------------------- | ------------------------- | --------------------------- | ------------------------------------------------------------------------ |
| Declaration/Definition| C-style: `type name(params)` | Planned (Parser/AST)        | `دالة` keyword removed from language spec.                               |
| `inline` specifier  | `مضمن`                    | Implemented (Lexer) / Planned (Semantic/CodeGen) | Hint to compiler.                                                        |
| Variadic Functions (`...`) | `...` (in C style decl) | Planned (Parser/AST/Codegen) | Standard C variadic functions. Not to be confused with preprocessor variadic macros. |

### Preprocessor

Baa's preprocessor aims for C99 compliance with Arabic keywords.

| C99 Feature           | B (باء) Equivalent             | Status (Preprocessor)       | Notes                                                                               |
| --------------------- | ------------------------------ | --------------------------- | ----------------------------------------------------------------------------------- |
| `#include`            | `#تضمين`                       | Implemented                 |                                                                                     |
| `#define` (object-like) | `#تعريف`                       | Implemented                 |                                                                                     |
| `#define` (func-like)   | `#تعريف(params)`               | Implemented                 |                                                                                     |
| `#undef`              | `#الغاء_تعريف`                 | Implemented                 |                                                                                     |
| `#ifdef`              | `#إذا_عرف`                     | Implemented                 | Uses `معرف NAME`.                                                                    |
| `#ifndef`             | `#إذا_لم_يعرف`                  | Implemented                 | Uses `!معرف NAME`.                                                                   |
| `#if`                 | `#إذا`                         | Implemented                 | Evaluates constant integer expressions. Undefined macros are 0.                       |
| `#elif`               | `#وإلا_إذا`                     | Implemented                 |                                                                                     |
| `#else`               | `#إلا`                         | Implemented                 |                                                                                     |
| `#endif`              | `#نهاية_إذا`                   | Implemented                 |                                                                                     |
| `defined` operator    | `معرف` operator                | Implemented                 | `معرف MACRO` or `معرف(MACRO)`.                                                       |
| `#` (stringification) | `#`                            | Implemented                 |                                                                                     |
| `##` (token pasting)  | `##`                           | Implemented                 |                                                                                     |
| `__FILE__`            | `__الملف__`                   | Implemented                 |                                                                                     |
| `__LINE__`            | `__السطر__`                   | Implemented                 | Expands to integer.                                                                 |
| `__DATE__`            | `__التاريخ__`                 | Implemented                 |                                                                                     |
| `__TIME__`            | `__الوقت__`                   | Implemented                 |                                                                                     |
| `__func__` (C99)      | `__الدالة__`                  | Implemented (PP Placeholder)| Preprocessor expands to placeholder; final value by later stages.                   |
| `__STDC_VERSION__`    | `__إصدار_المعيار_باء__`        | Implemented                 | e.g., `10210L`. (Value updated based on hypothetical v0.1.21.0)                      |
| Variadic Macros       | `وسائط_إضافية`, `__وسائط_متغيرة__` | Implemented                 | `...` becomes `وسائط_إضافية`, `__VA_ARGS__` becomes `__وسائط_متغيرة__`.                |
| `#error`              | `#خطأ`                         | Implemented                 |                                                                                     |
| `#warning`            | `#تحذير`                       | Implemented                 |                                                                                     |
| `#line`               | `#سطر`                         | Planned                     |                                                                                     |
| `_Pragma` operator    | `أمر_براغما`                   | Planned                     |                                                                                     |
| `#pragma`             | `#براغما`                      | Planned                     | e.g., `#براغما مرة_واحدة` for `#pragma once`.                                         |

### Miscellaneous C99 Features

| C99 Feature                                | B (باء) Equivalent / Status                                   | Notes                                                                 |
| ------------------------------------------ | ------------------------------------------------------------- | --------------------------------------------------------------------- |
| Variable length arrays (VLAs)              | Planned (Future)                                              | Stack-allocated arrays whose size is determined at runtime.           |
| Flexible array members (in structs)        | Planned (Parser/AST)                                          | e.g., `type member[];` as last member of a struct.                    |
| Designated initializers                    | Planned (Parser/AST)                                          | For arrays `[index] = value`, for structs/unions `::member = value` (using `::` instead of `.`) |
| Mixing declarations and code               | Planned (Parser/AST)                                          | Allowed in C99 within blocks.                                         |
| `//` comments                              | Implemented (Lexer/PP)                                        |                                                                       |
| Universal Character Names (`\uXXXX`, `\UXXXXXXXX`) | Baa uses `\يXXXX` only. `\U` not planned. `\u` removed. | Implemented (Lexer `\يXXXX`)                                         |
| `snprintf`, `vsnprintf` family             | Standard Library: `اطبع_نص_مُنسق_مُقيد` (tentative)            | Planned (Standard Library)                                            |
| `<stdbool.h>`, `<stdint.h>`, `<inttypes.h>`| Baa types map to these concepts. Specific headers not directly included by user. | Implemented (Core Types)                                             |
| `<complex.h>`, `<fenv.h>`, `<tgmath.h>`    | Planned (Future - Standard Library Extensions)                | Low priority.                                                         |

## Key Syntactic Differences from C99

* **Statement Terminator:** Baa uses `.` (dot) instead of C's `;`.
* **Keywords:** Baa uses Arabic keywords.
* **Literals:** Baa extends support for Arabic-Indic digits, Arabic decimal separator (`٫`), uses Arabic exponent marker `أ` (instead of `e`/`E`), and Arabic literal suffixes (`غ`, `ط`, `طط`, `ح`). Baa uses its own set of Arabic escape sequences (e.g., `\س` for newline, `\يXXXX` for Unicode) and does not support C's `\n`, `\t`, `\uXXXX`, `\xHH`.
* **Struct/Union Member Access (Direct):** Baa uses `::` for direct member access (e.g., `my_struct::member`) because `.` is the statement terminator. Pointer member access remains `->`.
* **Function Declaration Keywords:** Baa has removed `دالة` (function) and `متغير` (variable) as explicit declaration keywords, favoring C-style declarations (`type name(...){}`).

## Standard Library

Baa plans to provide a standard library with Arabic function names that correspond to common C standard library functions (e.g., `اطبع` for `printf`). The extent and direct C99 compatibility of this library is part of ongoing development.

*This comparison is based on the Baa language specification documents and current implementation status. It will evolve as the Baa compiler develops.*
