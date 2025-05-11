# دعم اللغة العربية (Arabic Language Support)

## Overview

Baa (باء) provides comprehensive support for Arabic programming through:

- Full UTF-8 encoding support
- Right-to-left (RTL) text handling
- Arabic identifiers and keywords
- Bilingual error messages
- Arabic documentation

## Language Features (ميزات اللغة)

### 1. Keywords (الكلمات المفتاحية)

| Arabic | English | Description |
|--------|---------|-------------|
| إذا | if | Conditional statement |
| وإلا | else | Alternative branch |
| طالما | while | While loop |
| لكل | for | For loop (C-style: init; condition; increment) |
| افعل | do | Do-while loop |
| اختر | switch | Switch statement |
| حالة | case | Case label |
| توقف | break | Break statement |
| استمر | continue| Continue statement |
| إرجع | return | Return statement |
| بنية | struct | Structure definition |
| ثابت | const | Constant keyword |
| اتحاد | union | Union definition |
| مستقر | static | Static storage duration / internal linkage |
| خارجي | extern | External linkage |
| مضمن | inline | Inline function hint (C99) |
| مقيد | restrict | Pointer optimization hint (C99) |
| نوع_مستخدم | typedef | Type alias definition |
| حجم | sizeof | Size of type/object operator |
| متطاير | volatile | Volatile memory access qualifier (C99) |
| تعداد | enum | Enumeration type definition |

### 2. Types (الأنواع)

| Arabic      | English | Description           | Size    |
|-------------|---------|----------------------|---------|
| عدد_صحيح    | int     | Integer              | 32-bit  |
| عدد_حقيقي   | float   | Floating-point       | 32-bit  |
| عدد_صحيح_طويل_جدا | long long int | Signed Long Long Integer | 64-bit |
| حرف        | char    | Character            | 16-bit  | (Lexer uses wchar_t)
| فراغ        | void    | No type             | -       |
| منطقي       | bool    | Boolean              | 8-bit   |
| مصفوفة      | array    | Array type         | -       |
| مؤشر        | pointer  | Pointer type       | -       |
| بنية        | struct   | Structure type     | -       |
| اتحاد       | union    | Union type         | -       |
| تعداد       | enum     | Enumeration type     | -       |

### 3. Operators (العمليات)

| Category | Arabic | Symbol | Description |
|----------|---------|--------|-------------|
| Arithmetic | جمع | + | Addition |
| | طرح | - | Subtraction |
| | ضرب | * | Multiplication |
| | قسمة | / | Division |
| | باقي | % | Modulo |
| | زيادة | ++ | Increment |
| | إنقاص | -- | Decrement |
| Compound | جمع_وتعيين | += | Add and assign |
| | طرح_وتعيين | -= | Subtract and assign |
| | ضرب_وتعيين | *= | Multiply and assign |
| | قسمة_وتعيين | /= | Divide and assign |
| | باقي_وتعيين | %= | Modulo and assign |
| Comparison | يساوي | == | Equal to |
| | لا_يساوي | != | Not equal to |
| | أكبر_من | > | Greater than |
| | أصغر_من | < | Less than |
| | أكبر_أو_يساوي | >= | Greater than or equal |
| | أصغر_أو_يساوي | <= | Less than or equal |
| Logical | و | && | Logical AND |
| | أو | \|\| | Logical OR |
| | ليس | ! | Logical NOT |
| Member Access | وصول_مباشر | :: | Direct member access (for structs/unions) |
| | وصول_مؤشر | -> | Pointer member access (for structs/unions) |

### 4. Boolean Literals (القيم المنطقية)

| Arabic | English | Description |
|--------|---------|-------------|
| صحيح | true | Boolean true value |
| خطأ | false | Boolean false value |

### 5. Preprocessor Directives (توجيهات المعالج المسبق)

Baa uses Arabic keywords for its preprocessor directives, aligning with C99 functionality:

| Baa Directive             | C99 Equivalent     | Description                                                                 |
|---------------------------|--------------------|-----------------------------------------------------------------------------|
| `#تضمين`                  | `#include`         | Includes another source file.                                               |
| `#تعريف`                  | `#define`          | Defines a macro. Supports object-like and function-like macros.             |
| `وسائط_إضافية` (in def)   | `...` (in def)     | Indicates variadic arguments in a function-like macro definition. (Implemented) |
| `__وسائط_متغيرة__` (in body) | `__VA_ARGS__` (in body) | Accesses variadic arguments within a macro expansion. (Implemented)         |
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

| Baa Predefined Macro | C99 Equivalent | Description                                                                 |
|----------------------|----------------|-----------------------------------------------------------------------------|
| `__الملف__`          | `__FILE__`     | Expands to the current source file name as a string literal.                |
| `__السطر__`          | `__LINE__`     | Expands to the current line number as an integer constant.                  |
| `__التاريخ__`        | `__DATE__`     | Expands to the compilation date as a string literal (e.g., "May 10 2025").  |
| `__الوقت__`          | `__TIME__`     | Expands to the compilation time as a string literal (e.g., "02:00:00").   |
| `__الدالة__` (Planned)| `__func__`     | Expands to the current function name as a string literal.                   |
| `__إصدار_المعيار_باء__` (Planned) | `__STDC_VERSION__` (conceptually) | Expands to a long int representing Baa language version (e.g., `10010L`). |

### 6. Numeric Literals (القيم العددية)

Baa supports Arabic in numeric literals extensively:
- **Arabic-Indic Digits (الأرقام الهندية):** Digits `٠` through `٩` (U+0660-U+0669) can be used wherever Western digits (`0`-`9`) are used, for both integers and floats.
  - Examples: `عدد_صحيح س = ١٢٣.` (s = 123), `عدد_حقيقي ص = ٣٫١٤.` (p = 3.14 using Arabic decimal separator)
- **Arabic Decimal Separator (الفاصلة العشرية العربية):** The character `٫` (U+066B) is recognized as a decimal separator in floating-point numbers, in addition to the period (`.`).
  - Example: `عدد_حقيقي pi = ٣٫١٤١٥٩.`
  - **Scientific Notation:** Uses `أ` (alif) as the exponent marker (e.g., `1.23أ4` for `1.23e4`).
- **Underscores with Arabic Numerals (الشرطة السفلية مع الأرقام العربية):** Underscores can be used as separators for readability with Arabic-Indic digits as well.
  - Example: `عدد_صحيح كبير = ١_٠٠٠_٠٠٠.` (one million)
- **Literal Suffixes (لواحق القيم الحرفية):** Baa uses Arabic suffixes to specify integer and floating-point literal types. - *[Planned]*
  * `غ` (ghayn): Unsigned (e.g., `123غ` for `123U`).
  * `ط` (ṭāʾ): Long (e.g., `456ط` for `456L`).
  * `طط` (ṭāʾ-ṭāʾ): Long Long (e.g., `789طط` for `789LL`).
  * `ح` (ḥāʾ): Float (e.g., `3.14ح` for `3.14F`).
  * Combinations are also possible, with a preferred order (e.g., unsignedness then length):
    * `غط` for Unsigned Long (e.g., `100غط`).
    * `غطط` for Unsigned Long Long (e.g., `200غطط`).

### 7. Character and String Literals (القيم الحرفية والنصية)

Character literals are enclosed in single quotes (`'ح'`). String literals are enclosed in double quotes (`"نص"`).
Baa uses the backslash (`\`) as the escape character, followed by an Arabic letter or specific sequences for special characters.

**Planned Arabic Escape Sequences:**
*   Newline: `\س`
*   Tab: `\م`
*   Backslash: `\\`
*   Single Quote: `\'`
*   Double Quote: `\"`
*   Carriage Return: `\ر`
*   Null Character: `\ص`
*   Unicode (4 hex digits): `\يXXXX` (e.g., `\ي0623` for 'أ')
*   Hex Byte (2 hex digits): `\هـHH` (e.g., `\هـ41` for 'A')

```baa
حرف سطر_جديد = '\س'.
حرف علامة_جدولة = '\م'.
نص مثال = "هذا نص يتضمن \سسطر جديد و \متاب."
```
*(Note: Standard C escapes like `\n`, `\t` are currently implemented; transition to full Arabic escapes is planned).*

### 8. Function Parameters (معاملات الدالة)

| Feature | Description |
|---------|-------------|
| Optional Parameters | Parameters with default values |
| Rest Parameters | Variable number of parameters (...args) |
| Named Arguments | Arguments specified by parameter name |
| Method/Function Distinction | Functions that belong to an object/class |

## Code Examples (أمثلة برمجية)

### 1. Hello World

```baa
// Baa uses C-style function declarations, e.g.:
// عدد_صحيح رئيسية() { ... }
// فراغ رئيسية() { ... }
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

- UTF-16LE encoding for source files (implemented)
- Support for Arabic comments and strings (implemented)
- Enhanced RTL text support (planned)

## Error Messages (رسائل الخطأ)

The compiler will provide error messages in both Arabic and English (planned feature):

```
خطأ: متغير غير معرف 'س'
Error: Undefined variable 's'

خطأ: نوع غير متوافق في التعيين
Error: Incompatible type in assignment
```

## Development Tools (أدوات التطوير)

### 1. Editor Support

- RTL text rendering
- Arabic syntax highlighting
- Auto-completion for Arabic keywords
- Error messages in Arabic

### 2. Debugging

- Arabic variable names in debugger
- Arabic call stack
- Arabic watch expressions

## Best Practices (أفضل الممارسات)

### 1. Naming Conventions

- Use meaningful Arabic names
- Follow Arabic grammar rules
- Be consistent with naming style
- Use underscores for compound names

### 2. Code Style

- Maintain consistent text direction
- Use clear Arabic comments
- Follow standard indentation
- Group related declarations

## Future Enhancements (التحسينات المستقبلية)

1. Enhanced RTL support in editors
2. More Arabic standard library functions
3. Arabic documentation generator
4. Improved error messages in Arabic
5. Arabic code formatting tools

## Version Support (دعم الإصدارات)

- Version 0.1.7: Basic Arabic support
- Version 0.1.8: Enhanced Arabic features
  - Improved error messages
  - Better RTL handling
  - More Arabic keywords
  - Extended documentation
