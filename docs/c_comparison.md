# K&R C and B (باء) Comparison

## Overview

This document compares K&R C features with their B (باء) equivalents and outlines the implementation roadmap for achieving full K&R C compatibility.

## Language Features

### Basic Types

| K&R C Feature | B (باء) Equivalent | Status      | Notes |
|---------------|--------------------|-------------|-------|
| `char`        | `حرف`             | Implemented | 16-bit (`wchar_t`) for Unicode support in Baa. |
| `int`         | `عدد_صحيح`        | Implemented | 32-bit integer (`int`). |
| `float`       | `عدد_حقيقي`      | Implemented | 32-bit floating point (`float`). |
| `void`        | `فراغ`            | Implemented | Type exists. |
| _Bool/bool    | `منطقي`           | Implemented | Boolean type exists; Baa uses `صحيح`/`خطأ` literals. |

### Derived Types

| K&R C Feature | B (باء) Equivalent | Status   | Notes |
|---------------|--------------------|----------|-------|
| Arrays        | `مصفوفة`          | Partial  | AST/Type support exists (`BAA_TYPE_ARRAY`, `BAA_EXPR_ARRAY`, `BAA_EXPR_INDEX`). Semantics/analysis likely incomplete. |
| Pointers      | `مؤشر`            | Planned  | Mentioned in docs, but no clear AST/Type support observed. |
| Structures    | `بنية`            | Planned  | Mentioned in docs, but no clear AST/Type support observed. |
| Unions        | `اتحاد`           | Planned  | Mentioned in docs, but no clear AST/Type support observed. |

### Storage Classes

| K&R C Feature | B (باء) Equivalent | Status   | Notes |
|---------------|--------------------|----------|-------|
| `auto`        | `تلقائي`          | N/A      | Baa uses type inference or explicit types; `auto` concept less relevant. Keyword not observed. |
| `static`      | `ثابت` (?)        | Planned  | `ثابت` keyword exists, likely maps to `const`. Static storage duration needs verification. |
| `extern`      | `خارجي`           | Planned  | Keyword exists (`is_extern` flag on `BaaFunction`). Full semantic handling needed. |
| `register`    | `سجل`             | Planned  | Keyword not observed; likely low priority. |

### Type Qualifiers

| K&R C Feature | B (باء) Equivalent | Status   | Notes |
|---------------|--------------------|----------|-------|
| `const`       | `ثابت`            | Partial  | Keyword `ثابت` exists and is parsed. Semantic enforcement needed. |
| `volatile`    | `متطاير`          | Planned  | Keyword not observed. |

### Literals

| K&R C Feature        | B (باء) Equivalent & Extensions                                     | Status      | Notes                                                                                                                               |
|----------------------|--------------------------------------------------------------------|-------------|-------------------------------------------------------------------------------------------------------------------------------------|
| Integer Literals     | Decimal (Western & Arabic-Indic), Binary (`0b`/`0B`), Hex (`0x`/`0X`). Underscores (`_`) for readability. | Implemented | K&R C had decimal, octal, hex. Baa does not explicitly support octal via `0` prefix (a leading `0` is just zero unless part of `0b`/`0x`). |
| Floating-Point Literals | Decimal point (`.` or `٫`), Scientific notation (`e`/`E`). Western & Arabic-Indic digits. Underscores for readability. | Implemented | K&R C had basic float forms. Baa extends with `٫` and full underscore/digit support.                                              |
| Character Literals   | `'c'` (Western/Arabic). Standard escapes (`\n`, `\t`, `\\`, `\'`, `\"`, `\r`, `\0`), Unicode (`\uXXXX`). | Implemented | K&R C had basic escapes and octal escapes. Baa focuses on standard and Unicode escapes.                                           |
| String Literals      | `"...\"` (Western/Arabic). Standard escapes, Unicode (`\uXXXX`).      | Implemented | K&R C had basic escapes. Baa supports richer character sets and Unicode escapes.                                                    |

### Control Flow

| K&R C Feature | B (باء) Equivalent | Status      | Notes |
|---------------|--------------------|-------------|-------|
| `if`          | `إذا`             | Implemented | AST/Parsing exists. |
| `else`        | `وإلا`            | Implemented | AST/Parsing exists. |
| `while`       | `طالما`           | Implemented | AST/Parsing exists. |
| `do`          | `افعل`            | Planned     | Keyword exists (`BAA_TOKEN_DO`), but parsing logic not implemented. |
| `for`         | `من_أجل`          | Implemented | AST/Parsing exists (`BaaForStmt`). Separators are ';'. |
| `switch`      | `اختر`            | Implemented | AST/Parsing exists (`BaaSwitchStmt`). |
| `case`        | `حالة`            | Implemented | AST/Parsing exists (`BaaCaseStmt`). |
| `break`       | `توقف`            | Implemented | Keyword exists (`BAA_TOKEN_BREAK`). AST/Parsing exists. Basic flow analysis check implemented. |
| `continue`    | `أكمل` (?)        | Implemented | Keyword exists (`BAA_TOKEN_CONTINUE`). AST/Parsing exists. Basic flow analysis check implemented. |
| `return`      | `إرجع`            | Implemented | Keyword exists (`BAA_TOKEN_RETURN`). AST/Parsing exists. |
| `goto`        | `اذهب`            | Planned     | Keyword not observed in lexer/parser implementation. |

### Operators

| K&R C Feature | B (باء) Equivalent | Status      | Notes |
|---------------|--------------------|-------------|-------|
| Arithmetic    | Same symbols       | Implemented | +, -, *, /, %. Basic type validation exists. |
| Comparison    | Same symbols       | Implemented | ==, !=, <, >, <=, >=. Basic type validation exists. |
| Logical       | Same symbols       | Implemented | &&, ||, !. Basic type validation for `!` exists. `&&`/`||` need validation logic. |
| Bitwise       | Same symbols       | Partial     | &, |, ^, <<, >>, ~. Keywords exist. Type validation logic missing. |
| Assignment    | Same symbols       | Implemented | =. Basic type validation exists. |
| Compound Assignment | Same symbols | Implemented | +=, -=, *=, /=, %=. AST/Parsing exists. Type validation logic missing. |
| Increment/Decrement | Same symbols | Implemented | ++, --. AST/Parsing exists. Type validation logic missing. |
| Address       | Same symbols       | Planned     | &, *. Requires pointer implementation. |
| Sizeof        | `حجم`             | Planned     | Keyword not observed in lexer/parser implementation. |

### Boolean Literals

| K&R C Feature | B (باء) Equivalent | Status      | Notes |
|---------------|--------------------|-------------|-------|
| `1` (typically) | `صحيح`          | Implemented | Boolean true literal (`true` is C99+). |
| `0` (typically) | `خطأ`           | Implemented | Boolean false literal (`false` is C99+). |

### Function Parameters

| K&R C Feature | B (باء) Extension | Status      | Notes |
|---------------|-------------------|-------------|-------|
| Basic parameters | Basic parameters | Implemented | Standard function parameters. |
| Default values | Optional parameters | Planned     | AST (`BaaParameter`) supports feature, but parser implementation is missing. |
| Variadic functions | Rest parameters | Planned     | AST (`BaaParameter`) supports feature, but parser implementation is missing. |
| - | Named arguments | Planned     | AST (`BaaCallExpr`) supports feature, but parser implementation is missing. |
| - | Method distinction | Partial?    | AST (`BaaFunction`) supports `is_method`. Usage/semantics unclear. Parser does not set this. |

### Preprocessor

| K&R C Feature | B (باء) Equivalent | Status      | Notes |
|---------------|--------------------|-------------|-------|
| `#include`    | `#تضمين`          | Implemented | Handled by the external `baa_preprocess` function. |
| `#define`     | `#تعريف`          | Implemented | Parameterless macros handled by `baa_preprocess`. |
| `#ifdef`      | `#إذا_عرف`        | Implemented | Handled by `baa_preprocess`. |
| `#ifndef`     | `#إذا_لم_يعرف`    | Implemented | Handled by `baa_preprocess`. |
| `#else`       | `#إلا`            | Implemented | Handled by `baa_preprocess`. |
| `#endif`      | `#نهاية_إذا`      | Implemented | Handled by `baa_preprocess`. |
| `#undef`      | `#الغاء_تعريف`    | Implemented | Handled by `baa_preprocess`. |

### Standard Library

**(Note: Status reflects availability of Baa function names/bindings, not necessarily full K&R C implementation completeness/correctness)**

| K&R C Feature | B (باء) Equivalent | Status   | Notes |
|---------------|--------------------|----------|-------|
| `printf`      | `اطبع`            | Unknown  | Baa function likely exists, implementation details unknown. |
| `scanf`       | `اقرأ`            | Unknown  | Baa function likely exists, implementation details unknown. |
| `fopen`       | `افتح_ملف`        | Unknown  | Baa function likely exists, implementation details unknown. |
| `fclose`      | `اغلق_ملف`        | Unknown  | Baa function likely exists, implementation details unknown. |
| `malloc`      | `حجز_ذاكرة`       | Unknown  | Baa function likely exists (maybe `baa_malloc` internally), implementation details unknown. |
| `free`        | `حرر_ذاكرة`       | Unknown  | Baa function likely exists (maybe `baa_free` internally), implementation details unknown. |
| `strcpy`      | `نسخ_نص`          | Unknown  | Baa function likely exists, implementation details unknown. |
| `strlen`      | `طول_نص`          | Unknown  | Baa function likely exists, implementation details unknown. |
| `strcmp`      | `مقارنة_نص`       | Unknown  | Baa function likely exists, implementation details unknown. |

## Implementation Roadmap

**(This section has been removed as it was inconsistent with detailed component status. Please refer to `docs/roadmap.md` and component-specific roadmaps like `docs/LEXER_ROADMAP.md`, `docs/PARSER_ROADMAP.md`, etc. for detailed status and future plans.)**

## Extensions Beyond K&R C

### Unicode Support

- 16-bit character type (`wchar_t`) - `Implemented`
- UTF-16LE string literals - `Implemented` (Lexer assumes UTF-16LE input)
- Arabic identifier support - `Implemented`
- Bidirectional text handling - `N/A` (Considered an editor/terminal concern)

### Error Messages

- Arabic language error messages - `Partial` (Number parsing only; general errors planned)
- Enhanced error recovery - `Planned` (Basic parser synchronization planned)
- Detailed diagnostic information - `Planned`
- Context-aware suggestions - `Planned`

### Development Tools

- Interactive debugger - `Planned`
- Performance profiler - `Planned`
- Memory analyzer - `Planned`
- Documentation generator - `Planned`

### Modern Features

- Source code formatting - `Planned`
- Static analysis - `Planned`
- Code completion - `Planned`
- Refactoring tools - `Planned`

## Compatibility Notes

### Source Code

- K&R C source code requires syntax changes (keywords, statement terminator `.`), type adjustments (e.g., `char` size), and UTF-16LE encoding conversion for Baa compilation.
- Arabic keywords can be used alongside English identifiers - `Implemented` (Lexer/Parser support this).
- UTF-16LE encoding required for source files - `Implemented` (Lexer assumes this).
- Bidirectional text support in string literals - `Implemented` (Supported via `wchar_t`).

### Binary Compatibility

- Generated object files are compatible with C linkers - `N/A` (Code generation not implemented).
- Standard C calling convention - `N/A` (Code generation not implemented).
- Standard C ABI compliance - `N/A` (Code generation not implemented).
- Platform-specific alignment rules - `N/A` (Code generation not implemented).

### Standard Library

- K&R C standard library functions available - `Unknown` / `Planned`.
- Arabic function names provided as aliases - `Unknown` / `Planned`.
- Extended Unicode support functions - `Unknown` / `Planned`.
- Additional helper functions for Arabic text - `Unknown` / `Planned`.

### Tool Integration

- Compatible with standard C debuggers - `N/A` (Code generation not implemented).
- Works with common build systems - `Implemented` (CMake).
- Supports standard profiling tools - `N/A` (Code generation not implemented).
- Integrates with popular IDEs - `Planned`.

## Syntax Comparison (مقارنة التركيب)

### Basic Program Structure

```c
// C Version
int main() {
    printf("Hello, World!\n");
    return 0;
}
```

```baa
// Baa Version
دالة رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع 0.
}
```

### Variable Declaration

```c
// C Version
int count = 0;
float price = 10.5;
char letter = 'A';
```

```baa
// Baa Version
عدد_صحيح عداد = 0.
عدد_حقيقي سعر = 10.5.
حرف حرف = 'أ'.
```

### Control Structures

```c
// C Version
if (age >= 18) {
    printf("Adult\n");
} else {
    printf("Minor\n");
}

while (count < 10) {
    count++;
}
```

```baa
// Baa Version
إذا (عمر >= 18) {
    اطبع("بالغ").
} وإلا {
    اطبع("قاصر").
}

طالما (عداد < 10) {
    عداد++.
}
```

## Feature Comparison (مقارنة الميزات)

### 1. Types (الأنواع)

| C Type | Baa Type | Size | Notes |
|--------|----------|------|-------|
| int | عدد_صحيح | 32-bit | Same range |
| float | عدد_حقيقي | 32-bit | Same precision |
| char | حرف | 8-bit | UTF-8 in Baa |
| void | فراغ | - | Same usage |
| struct | بنية | varies | Same layout |
| union | اتحاد | varies | Same usage |

### 2. Operators (العمليات)

| Category | C | Baa | Notes |
|----------|---|-----|-------|
| Arithmetic | +, -, *, /, % | جمع, طرح, ضرب, قسمة, باقي | Same precedence |
| Comparison | ==, !=, <, > | يساوي, لا_يساوي, أصغر_من, أكبر_من | Same behavior |
| Logical | &&, \|\|, ! | و, أو, ليس | Same rules |
| Bitwise | &, \|, ^, ~, <<, >> | Same | Not localized |

### 3. Keywords (الكلمات المفتاحية)

| C | Baa | Notes |
|---|-----|-------|
| if | إذا | Same semantics |
| else | وإلا | Same usage |
| while | طالما | Same behavior |
| for | من_أجل | Same structure |
| return | إرجع | Same purpose |
| struct | بنية | Same memory layout |
| typedef | نوع_مستخدم | Same functionality |

## Memory Model (نموذج الذاكرة)

- Same stack and heap organization
- Identical pointer arithmetic
- Compatible struct padding
- Similar alignment rules

## Standard Library (المكتبة القياسية)

| C Function | Baa Function | Purpose |
|------------|-------------|----------|
| printf | اطبع | Output text |
| scanf | اقرأ | Input text |
| malloc | احجز | Allocate memory |
| free | حرر | Free memory |
| strlen | طول_نص | String length |
| strcpy | انسخ_نص | String copy |

## Compilation Process (عملية الترجمة)

1. **Preprocessing**
   - C: #include, #define
   - Baa: تضمين#, تعريف#

2. **Compilation**
   - Both generate similar intermediate code
   - Baa adds Arabic symbol handling

3. **Linking**
   - Compatible object file format
   - Same linking process

## Key Differences (الفروق الرئيسية)

### 1. Text Handling

- Baa uses UTF-8 by default
- Better support for Arabic text
- RTL text rendering
- Arabic string literals

### 2. Error Messages

- Baa provides Arabic error messages
- More detailed error descriptions
- Cultural context in messages
- Bilingual support

### 3. Development Tools

- Arabic-aware debugger
- RTL-compatible editors
- Arabic documentation
- Localized tooling

### 4. Extensions

- Arabic identifier support
- RTL code formatting
- Arabic documentation comments
- Cultural adaptations

## Migration Guide (دليل الترحيل)

### From C to Baa

1. Convert keywords to Arabic
2. Update type names
3. Translate identifiers
4. Adjust string encoding
5. Update comments

### From Baa to C

1. Convert keywords to English
2. Restore C type names
3. Transliterate identifiers
4. Convert UTF-8 strings
5. Translate comments

## Best Practices (أفضل الممارسات)

### 1. Code Style

- Consistent language choice
- Clear naming conventions
- Proper text direction
- Cultural considerations

### 2. Interoperability

- Use compatible types
- Maintain C ABI
- Document translations
- Handle encodings

## Version Compatibility (توافق الإصدارات)

- Version 0.1.7: Basic C compatibility
- Version 0.1.8: Enhanced features
  - Better error messages
  - Improved type system
  - Extended standard library
  - More Arabic keywords
