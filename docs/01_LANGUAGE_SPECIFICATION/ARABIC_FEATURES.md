# Arabic Language Support

**Status:** ✅ Core Implementation Complete  
**Last Updated:** 2025-12-05  
**Version:** v0.1.27.0

## Overview

Baa (باء) provides comprehensive support for Arabic programming through:

- Full UTF-8 encoding support for source files (preprocessor auto-detects UTF-8 or UTF-16LE, internal processing is UTF-16LE for lexer).
- Right-to-left (RTL) text handling considerations in tools and documentation.
- Arabic identifiers and keywords.
- Bilingual error messages (planned for comprehensive coverage).
- Arabic documentation.

## Language Features (ميزات اللغة)

### 1. Keywords (الكلمات المفتاحية)

| Arabic             | English         | Description                                    |
| ------------------ | --------------- | ---------------------------------------------- |
| `إذا`               | `if`            | Conditional statement                          |
| `وإلا`              | `else`          | Alternative branch                             |
| `طالما`             | `while`         | While loop                                     |
| `لكل`               | `for`           | For loop (C-style: init; condition; increment) |
| `افعل`              | `do`            | Do-while loop                                  |
| `اختر`              | `switch`        | Switch statement                               |
| `حالة`              | `case`          | Case label                                     |
| `توقف`              | `break`         | Break statement                                |
| `استمر`             | `continue`      | Continue statement       |
| `إرجع`              | `return`        | Return statement                               |
| `بنية`              | `struct`        | Structure definition                           |
| `ثابت`              | `const`         | Constant keyword                               |
| `اتحاد`             | `union`         | Union definition                               |
| `مستقر`             | `static`        | Static storage duration / internal linkage     |
| `خارجي`             | `extern`        | External linkage                               |
| `مضمن`              | `inline`        | Inline function hint (C99)                     |
| `مقيد`              | `restrict`      | Pointer optimization hint (C99)                |
| `نوع_مستخدم`        | `typedef`       | Type alias definition                          |
| `حجم`               | `sizeof`        | Size of type/object operator                   |
| `متطاير`            | `volatile`      | Volatile memory access qualifier (C99)         |
| `تعداد`             | `enum`          | Enumeration type definition                    |

### 2. Types (الأنواع)

| Arabic             | English         | Description                    | Size   |
| ------------------ | --------------- | ------------------------------ | ------ |
| `عدد_صحيح`         | `int`           | Integer                        | 32-bit |
| `عدد_حقيقي`        | `float`         | Floating-point                 | 32-bit |
| `عدد_صحيح_طويل_جدا` | `long long int` | Signed Long Long Integer       | 64-bit |
| `حرف`              | `char`          | Character (UTF-16LE `wchar_t`) | 16-bit |
| `فراغ`             | `void`          | No type                        | -      |
| `منطقي`            | `bool`          | Boolean                        | 8-bit  |
| `مصفوفة`           | array           | Array type                     | -      |
| `مؤشر`             | pointer         | Pointer type                   | -      |
| `بنية`             | struct          | Structure type                 | -      |
| `اتحاد`            | union           | Union type                     | -      |
| `تعداد`            | enum            | Enumeration type               | -      |

### 3. Operators (العمليات)

| Category        | Arabic        | Symbol | Description             |
| --------------- | ------------- | ------ | ----------------------- |
| Arithmetic      | جمع          | `+`    | Addition                |
|                 | طرح          | `-`    | Subtraction             |
|                 | ضرب          | `*`    | Multiplication          |
|                 | قسمة         | `/`    | Division                |
|                 | باقي          | `%`    | Modulo                  |
|                 | زيادة        | `++`   | Increment               |
|                 | إنقاص        | `--`   | Decrement               |
| Compound        | جمع_وتعيين   | `+=`   | Add and assign          |
|                 | طرح_وتعيين   | `-=`   | Subtract and assign     |
|                 | ضرب_وتعيين   | `*=`   | Multiply and assign     |
|                 | قسمة_وتعيين  | `/=`   | Divide and assign       |
|                 | باقي_وتعيين  | `%=`   | Modulo and assign       |
| Comparison      | يساوي       | `==`   | Equal to                |
|                 | لا_يساوي    | `!=`   | Not equal to            |
|                 | أكبر_من     | `>`    | Greater than            |
|                 | أصغر_من     | `<`    | Less than               |
|                 | أكبر_أو_يساوي | `>=`   | Greater than or equal   |
|                 | أصغر_أو_يساوي | `<=`   | Less than or equal    |
| Logical         | و             | `&&`   | Logical AND             |
|                 | أو            | `||`   | Logical OR              |
|                 | ليس          | `!`    | Logical NOT             |
| Member Access   | وصول_مباشر  | `::`   | Direct member access    |
|                 | وصول_مؤشر   | `->`   | Pointer member access   |

### 4. Boolean Literals (القيم المنطقية)

| Arabic | English | Description        |
| ------ | ------- | ------------------ |
| `صحيح` | `true`  | Boolean true value |
| `خطأ`  | `false` | Boolean false value|

### 5. Preprocessor Directives (توجيهات المعالج المسبق)

Baa uses Arabic keywords for its preprocessor directives, aligning with C99 functionality:

| Baa Directive             | C99 Equivalent     | Description                                                                 |
| ------------------------- | ------------------ | --------------------------------------------------------------------------- |
| `#تضمين`                  | `#include`         | Includes another source file.                                               |
| `#تعريف`                  | `#define`          | Defines a macro. Supports object-like and function-like macros.             |
| `وسائط_إضافية` (in def)   | `...` (in def)     | Indicates variadic arguments in a function-like macro definition.           |
| `__وسائط_متغيرة__` (in body) | `__VA_ARGS__` (in body) | Accesses variadic arguments within a macro expansion.                       |
| `#الغاء_تعريف`            | `#undef`           | Removes a macro definition.                                                 |
| `#إذا`                    | `#if`              | Conditional compilation based on an expression. Uses `معرف` for `defined`. |
| `#إذا_عرف`                | `#ifdef`           | Conditional compilation if a macro is defined. (Uses `معرف`)                |
| `#إذا_لم_يعرف`            | `#ifndef`          | Conditional compilation if a macro is not defined. (Uses `!معرف`)         |
| `#وإلا_إذا`                | `#elif`            | Else-if condition for conditional compilation.                              |
| `#إلا`                    | `#else`            | Alternative branch for conditional compilation.                             |
| `#نهاية_إذا`              | `#endif`           | Ends a conditional compilation block.                                       |
| `#خطأ`                    | `#error`           | Reports a fatal error during preprocessing.                                 |
| `#تحذير`                  | `#warning`         | Reports a warning message during preprocessing.                             |
| `#سطر`                    | `#line`            | Modifies the reported line number and filename.                             |
| `#براغما`                  | `#pragma`          | Implementation-defined directives.                                          |
| `أمر_براغما("...")`       | `_Pragma("...")`   | Operator to generate a pragma directive from a string literal.              |

**Predefined Macros:**

| Baa Predefined Macro        | C99 Equivalent | Description                                                                 |
| --------------------------- | -------------- | --------------------------------------------------------------------------- |
| `__الملف__`                 | `__FILE__`     | Expands to the current source file name as a string literal.                |
| `__السطر__`                 | `__LINE__`     | Expands to the current line number as an integer constant.                  |
| `__التاريخ__`               | `__DATE__`     | Expands to the compilation date as a string literal (e.g., "Nov 06 2025").  |
| `__الوقت__`                 | `__TIME__`     | Expands to the compilation time as a string literal (e.g., "08:32:00").   |
| `__الدالة__`                | `__func__`     | Expands to `L"__BAA_FUNCTION_PLACEHOLDER__"` in preprocessor. Final value by later stages. |
| `__إصدار_المعيار_باء__`       | `__STDC_VERSION__` (conceptually) | Expands to a long int (e.g., `10210L`) representing Baa language version. |

### 6. Numeric Literals (القيم العددية)

Baa supports Arabic in numeric literals extensively:

- **Arabic-Indic Digits (الأرقام الهندية):** Digits `٠` through `٩` (U+0660-U+0669) can be used wherever Western digits (`0`-`9`) are used, for both integers and floats. The lexer captures these raw digits as part of the token's lexeme.
  - Examples: `عدد_صحيح س = ١٢٣.` (s = 123), `عدد_حقيقي ص = ٣٫١٤.` (p = 3.14 using Arabic decimal separator)
- **Arabic Decimal Separator (الفاصلة العشرية العربية):** The character `٫` (U+066B) is recognized as a decimal separator in floating-point numbers, in addition to the period (`.`). The lexer captures this raw character.
  - Example: `عدد_حقيقي pi = ٣٫١٤١٥٩.`
- **Scientific Notation:** Uses `أ` (ALIF WITH HAMZA ABOVE, U+0623) as the exponent marker (e.g., `1.23أ4` for `1.23 * 10^4`). English `e`/`E` are not supported.
  - Examples: `1.23أ4`, `١٠أ-٢`, `٠٫٥أ+٣`
- **Underscores with Arabic Numerals (الشرطة السفلية مع الأرقام العربية):** Underscores can be used as separators for readability with Arabic-Indic digits as well.
  - Example: `عدد_صحيح كبير = ١_٠٠٠_٠٠٠.` (one million)
- **Literal Suffixes (لواحق القيم الحرفية):** Baa uses Arabic suffixes to specify literal types.
  - **Integer Suffixes:** `غ` (unsigned), `ط` (long), `طط` (long long), and combinations (e.g., `غط`, `ططغ`).
    - Examples: `123غ`, `٤٥٦ط`, `0x10ططغ`
  - **Float Suffix:** `ح` (float, e.g., `3.14ح`, `١٫٠أ٢ح`).

### 7. Character and String Literals (القيم الحرفية والنصية)

Character literals are enclosed in single quotes (`'ح'`). String literals are enclosed in double quotes (`"نص"`).
Baa uses the backslash (`\`) as the escape character, followed by a **Baa-specific Arabic escape character or sequence.**
*Old C-style single-letter escapes (like `\n`, `\t`) and `\uXXXX` / `\xHH` are **not** supported and will be treated as errors.*

**Baa Escape Sequences (Arabic Syntax Only):**

| Escape    | Character Represented      | Unicode (if applicable) | Notes                               |
| --------- | -------------------------- | ----------------------- | ----------------------------------- |
| `\س`      | Newline                    | `L'\n'` (U+000A)        | (SEEN)                              |
| `\م`      | Tab                        | `L'\t'` (U+0009)        | (MEEM)                              |
| `\ر`      | Carriage Return            | `L'\r'` (U+000D)        | (REH)                               |
| `\ص`      | Null Character             | `L'\0'` (U+0000)        | (SAD)                               |
| `\\`      | Backslash `\`              | `L'\\'` (U+005C)        | (Standard, kept for escaping itself)|
| `\'`      | Single Quote `'`           | `L'\''` (U+0027)        | (Standard, for char literals)       |
| `\"`      | Double Quote `"`           | `L'"'`  (U+0022)        | (Standard, for string literals)     |
| `\يXXXX`  | Unicode Character U+XXXX   |                         | (YEH) XXXX are 4 hex digits.      |
| `\هـHH`   | Byte value 0xHH            |                         | (HEH, Tatweel) HH are 2 hex digits. Value 0-255. |

```baa
حرف سطر_جديد = '\س'.
حرف علامة_جدولة = '\م'.
نص مثال = "هذا نص يتضمن \سسطر جديد و \متاب."
حرف همزة_علوية = '\ي0623'. // Represents 'أ'
حرف قيمة_بايت = '\هـ41'. // Represents ASCII 'A'
```

- **Multiline Strings:** Sequences of characters enclosed in triple double quotes (`"""`). Newlines within the string are preserved. Baa escape sequences are processed.

    ```baa
    حرف نص_متعدد = """سطر أول\سطر ثاني مع \م تاب.""".
    ```

- **Raw String Literals:** Prefixed with `خ` (Kha). No escape sequence processing.

    ```baa
    حرف مسار = خ"C:\مسارات\ملف.txt". // \م is literal here
    حرف خام_متعدد = خ"""هذا \س نص خام. الهروب \م لا يعمل.""".
    ```

### 8. Function Parameters (معاملات الدالة)

| Feature               | Description                          | Status      |
| --------------------- | ------------------------------------ | ----------- |
| Optional Parameters   | Parameters with default values       | Planned     |
| Rest Parameters       | Variable number of parameters        | Planned     |
| Named Arguments       | Arguments specified by parameter name| Planned     |

## Code Examples (أمثلة برمجية)

### 1. Hello World

```baa
// Baa uses C-style function declarations
// Main function entry point
عدد_صحيح رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع 0.
}
```

### 2. Function with Parameters

```baa
عدد_صحيح جمع_الأرقام(عدد_صحيح أ، عدد_صحيح ب) {
    إرجع أ + ب.
}
```

### 3. Conditional Statement

```baa
إذا (عمر >= 18) {
    اطبع("بالغ").
} وإلا {
    اطبع("قاصر").
}
```

### 4. Loop Example

```baa
عدد_صحيح مجموع = 0.
لكل (عدد_صحيح i = 1; i <= 10; i++) {
    مجموع += i.
}
```

## File Support (دعم الملفات)

### 1. File Extensions

- `.ب` - Primary Arabic source file extension
- `.baa` - Alternative source file extension

### 2. File Encoding

- Source files are expected to be UTF-8 or UTF-16LE (BOM recommended for UTF-16LE).
- The preprocessor auto-detects these and converts to an internal UTF-16LE stream for the lexer.
- Full support for Arabic characters in comments, strings, and identifiers.

## Error Messages (رسائل الخطأ)

The compiler aims to provide error messages in both Arabic and English.

```
خطأ: متغير غير معرف 'س'
Error: Undefined variable 's'

خطأ: نوع غير متوافق في التعيين
Error: Incompatible type in assignment
```

## Development Tools (أدوات التطوير)

### 1. Editor Support (Planned)

- RTL text rendering.
- Arabic syntax highlighting.
- Auto-completion for Arabic keywords.
- Error messages in Arabic.

### 2. Debugging (Planned)

- Arabic variable names in debugger.
- Arabic call stack.
- Arabic watch expressions.

## Best Practices (أفضل الممارسات)

### 1. Naming Conventions

- Use meaningful Arabic names.
- Follow Arabic grammar rules where applicable for clarity.
- Be consistent with naming style (e.g., `اسم_متغير_طويل` or `اسم متغير طويل` if supported).
- Use underscores for compound names if preferred: `الاسم_الاول`.

### 2. Code Style

- Maintain consistent text direction (especially in editors that support mixed RTL/LTR).
- Use clear Arabic comments.
- Follow standard indentation.
- Group related declarations.

## Future Enhancements (التحسينات المستقبلية)

1. Enhanced RTL support in editors and tool outputs.
2. More Arabic standard library functions.
3. Arabic documentation generator tools.
4. Improved and fully localized error messages.
5. Arabic code formatting tools.

## Version Support (دعم الإصدارات)

- **Version 0.1.15 (Current - November 2025):**
  - ✅ Priority 4 Complete: Core compiler with preprocessor, lexer, parser, and AST
  - ✅ Full Unicode support for Arabic identifiers and keywords
  - ✅ Complete preprocessor with Arabic directives
  - ✅ Variables, functions, control flow, expressions, basic types
  - 🚧 Semantic analysis in progress
  - 📋 Code generation planned

- **Future Versions:**
  - Advanced semantic analysis
  - LLVM code generation
  - Struct, union, enum, pointer support
  - Enhanced tooling and IDE integration
  - Comprehensive standard library
