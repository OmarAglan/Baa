# لغة باء - مواصفات اللغة

# B (باء) Language Specification

## 1. Basic Syntax Examples

* (Note: See subsequent sections for detailed definitions)*

### 1.1 Preprocessor Directives

Baa supports a preprocessor step that handles directives starting with `#` before lexical analysis. These directives allow for file inclusion, macro definitions, and conditional compilation.

#### 1.1.1 File Inclusion

* **`#تضمين` (Include):** Used to include the content of other files. - *[Implemented]*
  * `#تضمين "مسار/ملف/نسبي.ب"` : Includes a file relative to the current file's path.
  * `#تضمين <مكتبة_قياسية>` : Includes a file found in standard library include paths.

    ```baa
    #تضمين "my_definitions.b" // Include a local file
    #تضمين <standard_io>      // Include a standard library
    ```

#### 1.1.2 Macro Definitions

* **`#تعريف` (Define):** Defines a preprocessor macro. - *[Implemented]*
  * **Object-like macros:** Replaces an identifier with a token sequence.

      ```baa
        #تعريف PI 3.14159
        #تعريف GREETING "مرحباً بالعالم"
        عدد_حقيقي س = PI.
        اطبع(GREETING).
      ```

  * **Function-like macros:** Defines macros that take arguments.

      ```baa
        #تعريف MAX(a, b) ((a) > (b) ? (a) : (b))
        عدد_صحيح الأكبر = MAX(10, 20). // Expands to ((10) > (20) ? (10) : (20))

        #تعريف ADD(x, y) (x + y)
        عدد_صحيح المجموع = ADD(5, 3). // Expands to (5 + 3)
      ```

  * **Stringification Operator (`#`):** Converts a macro parameter into a string literal. - *[Implemented]*

      ```baa
        #تعريف STRINGIFY(val) #val
        اطبع(STRINGIFY(مرحبا)). // Expands to اطبع("مرحبا").
        اطبع(STRINGIFY(123)).   // Expands to اطبع("123").
      ```

  * **Token Pasting Operator (`##`):** Concatenates two tokens. - *[Implemented]*

      ```baa
        #تعريف CONCAT(a, b) a##b
        عدد_صحيح CONCAT(var, Name) = 10. // Declares عدد_صحيح varName = 10.
      ```

  * **Variadic Macros (C99):** Defines macros that can accept a variable number of arguments. For Baa, this is achieved using `وسائط_إضافية` in the parameter list and `__وسائط_متغيرة__` in the macro body. - *[Implemented]*
    * `وسائط_إضافية` (wasā'iṭ iḍāfiyyah - additional arguments): Used in the parameter list (must be the last parameter) to indicate variable arguments.
    * `__وسائط_متغيرة__` (al-wasā'iṭ al-mutaghayyirah - The Variable Arguments): The special identifier used in the macro body to refer to the arguments matched by `وسائط_إضافية`. If no arguments are passed to `وسائط_إضافية`, `__وسائط_متغيرة__` expands to an empty sequence of tokens.

        ```baa
        #تعريف DEBUG_PRINT(تنسيق, وسائط_إضافية) hypothetical_print_func(تنسيق, __وسائط_متغيرة__)
        // Example usage:
        // DEBUG_PRINT("رسالة تصحيح %d", 10) expands to hypothetical_print_func("رسالة تصحيح %d", 10)
        // DEBUG_PRINT("رسالة بسيطة") expands to hypothetical_print_func("رسالة بسيطة", )
        ```

* **`#الغاء_تعريف` (Undefine):** Removes a previously defined macro. - *[Implemented]*

    ```baa
    #تعريف TEMP_MACRO 100
    // TEMP_MACRO is 100
    #الغاء_تعريف TEMP_MACRO
    // TEMP_MACRO is no longer defined
    ```

#### 1.1.3 Conditional Compilation

Directives for compiling parts of the code based on conditions. Expressions in these directives can use arithmetic, bitwise, and logical operators, as well as the `معرف()` operator. - *[Implemented]*

* **`#إذا` (If):** Compiles the subsequent code if the expression evaluates to true (non-zero). - *[Implemented]*

    ```baa
    #تعريف DEBUG_MODE 1
    #إذا DEBUG_MODE
        اطبع("وضع التصحيح مفعل.").
    #نهاية_إذا

    #تعريف VALUE 10
    #إذا VALUE > 5 && معرف(DEBUG_MODE)
        اطبع("القيمة أكبر من 5 ووضع التصحيح مفعل.").
    #نهاية_إذا
    ```

* **`#إذا_عرف` (If defined):** Compiles the subsequent code if the macro is defined. Equivalent to `#إذا معرف(MACRO_NAME)`. - *[Implemented]*

    ```baa
    #تعريف MY_FEATURE
    #إذا_عرف MY_FEATURE
        اطبع("ميزة MY_FEATURE مفعلة.").
    #نهاية_إذا
    ```

* **`#إذا_لم_يعرف` (If not defined):** Compiles the subsequent code if the macro is not defined. Equivalent to `#إذا !معرف(MACRO_NAME)`. - *[Implemented]*

    ```baa
    #إذا_لم_يعرف PRODUCTION_BUILD
        اطبع("هذا ليس بناء إنتاجي.").
    #نهاية_إذا
    ```

* **`#وإلا_إذا` (Else if):** Compiles the subsequent code if the preceding `#إذا` or `#وإلا_إذا` condition was false, and its own expression evaluates to true. - *[Implemented]*

    ```baa
    #تعريف LEVEL 2
    #إذا LEVEL == 1
        اطبع("المستوى 1.").
    #وإلا_إذا LEVEL == 2
        اطبع("المستوى 2.").
    #إلا
        اطبع("مستوى آخر.").
    #نهاية_إذا
    ```

* **`#إلا` (Else):** Compiles the subsequent code if the preceding `#إذا` or `#وإلا_إذا` condition was false. - *[Implemented]*
* **`#نهاية_إذا` (End if):** Marks the end of a conditional compilation block. - *[Implemented]*
* **`معرف()` Operator:** Used within conditional expressions to check if a macro is defined. Returns `1` if defined, `0` otherwise. Can be used as `معرف(اسم_الماكرو)` or `معرف اسم_الماكرو`. - *[Implemented]*

    ```baa
    #إذا معرف(VERBOSE) || معرف EXTRA_DEBUG
        // Code for verbose or extra debug output
    #نهاية_إذا
    ```

#### 1.1.4 Other Standard Directives (Planned)

Baa plans to support other standard C preprocessor directives with Arabic keywords:

* **`#خطأ "رسالة"` (`#error "message"`):** Instructs the preprocessor to report a fatal error. The compilation process stops. - *[Implemented]*

    ```baa
    #إذا_لم_يعرف REQUIRED_FEATURE
        #خطأ "الميزة المطلوبة REQUIRED_FEATURE غير معرفة."
    #نهاية_إذا
    ```

* **`#تحذير "رسالة"` (`#warning "message"`):** Instructs the preprocessor to issue a warning message. Compilation typically continues. - *[Implemented]*

    ```baa
    #تحذير "هذه الميزة مهملة وسيتم إزالتها في الإصدارات القادمة."
    ```

* **`#سطر رقم "اسم_الملف"` (`#line number "filename"`):** Changes the preprocessor's internally stored line number and filename. This affects the output of `__السطر__` and `__الملف__`. - *[Planned]*

    ```baa
    #سطر ١٠٠ "ملف_مصدر_آخر.ب"
    // الآن __السطر__ سيكون ١٠٠ و __الملف__ سيكون "ملف_مصدر_آخر.ب"
    ```

* **`#براغما توجيه_خاص` (`#pragma directive`):** Used for implementation-defined directives. The specific `توجيه_خاص` (special directive) and its behavior depend on the Baa compiler. - *[Planned]*
  * Example: `#براغما مرة_واحدة` (could be Baa's equivalent of `#pragma once`).
* **`أمر_براغما("توجيه_نصي")` (`_Pragma("string_directive")`):** An operator (not a directive starting with `#`) that allows a macro to generate a `#براغما` directive. It takes a string literal which is then treated as the content of a `#براغما` directive. - *[Planned]*

    ```baa
    #تعريف DO_PRAGMA(x) أمر_براغما(#x)
    // DO_PRAGMA(توجيه_خاص للتحسين)
    // expands to: أمر_براغما("توجيه_خاص للتحسين")
    // which is then processed as if #براغما توجيه_خاص للتحسين was written.
    ```

#### 1.1.5 Predefined Macros

Baa provides several predefined macros that offer information about the compilation process. - *[Implemented]*

* `__الملف__` : Expands to a string literal representing the name of the current source file.
* `__السطر__` : Expands to an integer constant representing the current line number in the source file.
* `__التاريخ__` : Expands to a string literal representing the compilation date (e.g., "May 09 2025").
* `__الوقت__` : Expands to a string literal representing the compilation time (e.g., "07:40:00").
* `__الدالة__` : Expands to a string literal placeholder `L"__BAA_FUNCTION_PLACEHOLDER__"`. Actual function name substitution is intended for later compiler stages. - *[Implemented by Preprocessor as placeholder]*
* `__إصدار_المعيار_باء__` : Expands to a long integer constant `10150L` (representing Baa language version 0.1.15, value updated). - *[Implemented by Preprocessor]*

    ```baa
    اطبع("تم التجميع من الملف: " + __الملف__).
    اطبع("في السطر رقم: " + __السطر__).
    // اطبع("في الدالة: " + __الدالة__). // Example if implemented
    ```

### 1.2 Statement Termination

Statements are terminated with a dot (`.`) instead of a semicolon.

```baa
اطبع("مرحبا").    // Correct
اطبع("مرحبا");    // Incorrect
```

### 1.3 Function Declaration Example

* (See Section 4.2 for full details)*

```baa
// [Implementation Pending] Main function (entry point) - Assuming void return if omitted
فراغ رئيسية() {
    // Function body
}

// [Implementation Pending] Function with integer parameter and return value
عدد_صحيح مربع(عدد_صحيح س) {
    إرجع س * س.
}
```

### 1.4 Variable Declaration Example

*(See Section 4.1 for full details)*
Variables are declared using the C-style syntax.

```baa
// [Implemented] Declare an integer named 'counter'
عدد_صحيح counter.

// [Implemented] Declare and initialize a float named 'price'
عدد_حقيقي price = 10.5.

// [Partial] Declare a constant boolean
ثابت منطقي FLAG = صحيح.
```

### 1.5 Control Flow Example

*(See Section 6 for full details)*
Control structures use Arabic keywords and dot termination.

```baa
// If statement
إذا (س > 0) {
    اطبع("موجب").
} وإلا {
    اطبع("سالب").
}

// While loop
طالما (س < 10) {
    اطبع(س).
    س = س + 1.
}
```

### 1.6 Operator Examples

* (See Section 5 for full details)*

```baa
عدد_صحيح a = 10.
عدد_صحيح b = 5.

// Increment and decrement
a++.      // Postfix increment (a becomes 11)
--b.      // Prefix decrement (b becomes 4)

// Compound assignment
a += b.   // Add and assign (a becomes 11 + 4 = 15)
```

## 2. Lexical Structure

This section describes the low-level building blocks of Baa programs **after** preprocessing. The Baa preprocessor accepts source files in UTF-8 (default if no BOM is present) or UTF-16LE. The processed output from the preprocessor, which is then fed to the lexer, is encoded in **UTF-16LE**.

### 2.1 Comments

Baa supports standard C/C++ style comments, as well as documentation comments:

* **Single-line:** Begins with `//` and continues to the end of the line. The lexer produces a `BAA_TOKEN_SINGLE_LINE_COMMENT` whose lexeme is the content after `//`. The token's location points to the start of `//`. - *[Implemented]*
* **Multi-line:** Begins with `/*` and ends with `*/`. These comments can span multiple lines. The lexer produces a `BAA_TOKEN_MULTI_LINE_COMMENT` whose lexeme is the content between `/*` and `*/`. The token's location points to the start of `/*`. - *[Implemented]*
* **Documentation Comments:** Begin with `/**` and end with `*/`. by the lexer as `BAA_TOKEN_DOC_COMMENT`. The lexeme is the content between `/**` and `*/`, and the token's location points to the start of `/**`. - *[Implemented by Lexer]*
* **Preprocessor Directives:** Lines starting with `#` (e.g., `#تضمين`, `#تعريف`) are handled entirely by the preprocessor before lexical analysis. They are not treated as comments by the lexer. - *[Implemented for various directives]*
* **Whitespace and Newlines:** Sequences of spaces/tabs are tokenized as `BAA_TOKEN_WHITESPACE`.
* Newline characters (`\n`, `\r`, `\r\n`) are tokenized as `BAA_TOKEN_NEWLINE`. These are passed to the parser. - *[Implemented]*

```baa
// هذا تعليق سطر واحد.
/*
  هذا تعليق
  متعدد الأسطر.
*/
/**
 * هذا تعليق توثيقي.
 * يمكن أن يمتد عبر عدة أسطر.
 */
```

### 2.2 Identifiers

Identifiers are names used for variables, functions, types, etc.

* **Allowed Characters:** Arabic letters (Unicode ranges 0x0600-0x06FF, FB50-FDFF, FE70-FEFF), English letters (`a-z`, `A-Z`), Arabic-Indic digits (`٠-٩`), ASCII digits (`0-9`), and the underscore (`_`).
* **Starting Character:** Must begin with a letter (Arabic or English) or an underscore.
* **Case Sensitivity:** Identifiers are case-sensitive.
* **Reserved Words:** Keywords (see Section 2.3) cannot be used as identifiers.

```baa
// أمثلة صحيحة (Valid Examples)
حرف الإسم_الأول.         // Example of a string type identifier
عدد_صحيح _temporaryValue. // Example of an integer type identifier starting with underscore
عدد_صحيح قيمة_رقمية1.    // Example with Arabic and Western digits
حرف القيمة٢.             // Example with Arabic-Indic digits

// أمثلة خاطئة (Invalid Examples)
// عدد_صحيح 1stValue. // Cannot start with a digit
// حرف قيمة-خاصة.     // Hyphen not allowed
// عدد_صحيح إذا.      // Keyword cannot be an identifier
```

### 2.3 Keywords

Keywords are reserved words with special meaning in the Baa language and cannot be used as identifiers.

*(Based on `lexer.h` and `language.md`)*

* **Declarations:** `ثابت` (`const`), `مستقر` (`static`), `خارجي` (`extern`), `مضمن` (`inline`), `مقيد` (`restrict`), `نوع_مستخدم` (`typedef`), `حجم` (`sizeof`), `تعداد` (`enum`) - *[Lexer Implemented for `ثابت`, `مضمن`, `مقيد`; Partial/Planned for others]*
* **Control Flow:** `إذا`, `وإلا`, `طالما`, `إرجع`, `توقف` (`break`), `استمر` (`continue`) - *[Implemented]*
  * *Partial/Planned:* `لكل` (`for`), `افعل` (`do`), `اختر` (`switch`), `حالة` (`case`)
* **Types:** `عدد_صحيح`, `عدد_حقيقي`, `حرف`, `منطقي`, `فراغ` - *[Implemented]*
* **Boolean Literals:** `صحيح`, `خطأ` - *[Implemented]*

*(Note: Standard library function names like `اطبع` are technically identifiers, not keywords)*

### 2.4 Literals

Literals represent fixed values in the source code. The lexer captures the raw characters from the source for numeric literals; `number_parser.c` later converts these to values.

* **Integer Literals (`عدد_صحيح`):** Represent whole numbers. The lexer tokenizes these as `BAA_TOKEN_INT_LIT`.
  * **Decimal:** Sequences of Western digits (`0`-`9`) and/or Arabic-Indic digits (`٠`-`٩` / U+0660-U+0669).
    * Examples: `123`, `٠`, `٤٢`, `1٠2` - *[Implemented]*
  * **Hexadecimal:** Must start with `0x` or `0X`, followed by hexadecimal digits (`0`-`9`, `a`-`f`, `A`-`F`).
    * Examples: `0xFF`, `0x1a`, `0XDEADBEEF` - *[Implemented]*
  * **Binary:** Must start with `0b` or `0B`, followed by binary digits (`0` or `1`). - *[Implemented]*
    * Examples: `0b1010`, `0B11001` - *[Implemented]*
  * **Underscores for Readability:** Single underscores (`_`) can be used as separators within the digits of any integer literal type.
    * Examples: `1_000_000`, `٠_١٢٣_٤٥٦`, `0xAB_CD_EF`, `0b10_10_10` - *[Implemented]*
  * **Arabic Suffixes for Type:** Integer literals can have suffixes `غ` (unsigned), `ط` (long), `طط` (long long), and combinations.
    * Examples: `123غ`, `0xFFط`, `0b101ططغ` - *[Lexer Implemented]*
* **Floating-Point Literals (`عدد_حقيقي`):** The lexer tokenizes these as `BAA_TOKEN_FLOAT_LIT`.
  * **Decimal Representation:** Digits (Western or Arabic-Indic) with a decimal point (`.` or `٫` U+066B).
    * Examples: `3.14`, `٠٫١٢٣` - *[Implemented]*
  * **Scientific Notation:** Introduced by `أ` (ALIF WITH HAMZA ABOVE, U+0623), followed by an optional sign (`+`/`-`) and decimal/Arabic-Indic digits.
    * Examples: `1.23أ4`, `10أ-2`, `٠٫٥أ+٣`, `1أ+٠٥` - *[Conceptual: `أ` implemented in lexer, `e`/`E` removed]*
  * **Underscores for Readability:** Can be used in integer, fractional, and exponent parts.
    * Examples: `1_234.567_890`, `3.141_592أ+1_0` - *[Implemented]*
  * **Hexadecimal Floating-Point Constants (C99):** Baa may support a C99-like hex float format (e.g., `0x1.فأ+2` using `ف` for fraction part and `أ` for binary power exponent). - *[Planned]*
* **Boolean Literals (`منطقي`):**
  * `صحيح` (true) - *[Implemented]*
  * `خطأ` (false) - *[Implemented]*
* **Arabic Suffix for Type:** `ح` (Hah) for float (e.g., `3.14ح`). - *[Conceptual: Implemented in Lexer]*
* **Character Literals (`حرف`):** Represent single characters enclosed in single quotes (`'`). Baa uses `\` as the escape character followed by a specific Baa escape character.
  * `'a'`, `'أ'`, `'#'`, `'١'` - *[Implemented]*
  * **Baa Escape Sequences (Arabic Syntax Only):**
    * `\س` (newline), `\م` (tab), `\\` (backslash), `\'` (single quote)
    * `\"` (double quote)
    * `\ر` (carriage return), `\ص` (null char)
    * `\يXXXX` (Unicode escape, where XXXX are four hex digits, e.g., `\ي0623` for 'أ')
    * `\هـHH` (Hex byte escape, where `ـ` is Tatweel and HH are two hex digits, e.g., `\هـ41` for 'A')
  * *Old C-style escapes like `\n`, `\t`, `\uXXXX` are **not** supported and will result in errors.* - *[Conceptual: Implemented in Lexer]*
* **String Literals:** Represent sequences of characters enclosed in double quotes (`"`). Uses UTF-16LE encoding internally after preprocessing. Baa uses `\` as the escape character followed by a specific Baa escape character.
  * `"مرحباً"` - *[Implemented]*
  * `"Hello, World!"` - *[Implemented]*
  * **Baa Escape Sequences (Arabic Syntax Only):** Same as for character literals (`\س`, `\م`, `\"`, `\\`, `\ر`, `\ص`, `\يXXXX`, `\هـHH`).
  * *Old C-style escapes like `\n`, `\t`, `\uXXXX` are **not** supported and will result in errors.* - *[Conceptual: Implemented in Lexer]*
  * **Multiline Strings:** Sequences of characters enclosed in triple double quotes (`"""`). Newlines within the string are preserved. Baa escape sequences (using `\`) are processed as in regular strings. - *[Implemented, Baa Arabic Escapes conceptually implemented in Lexer]*
    * Example: `حرف نص_متعدد = """سطر أول\سطر ثاني مع \م تاب.""".`
  * **Raw String Literals:** Prefixed with `خ` (Kha), these strings do not process escape sequences. All characters between the delimiters are taken literally, including `\`.
    * Single-line raw strings: `خ"..."`
    * Multiline raw strings: `خ"""..."""` (newlines are preserved)
    * Examples:
      * `حرف مسار = خ"C:\Users\MyFolder\file.txt".` (Backslashes are literal)
      * `حرف تعبير_نمطي = خ"\\d{3}-\\d{2}-\\d{4}".` (`\d` is literal here, not a Baa escape)
      * `حرف خام_متعدد = خ"""هذا \س نص خام.
                الهروب \م لا يعمل هنا.""".` (`\س` and `\م` are literal here)
    * *[Implemented]*
* **Compound Literals (C99):** Allow the creation of unnamed objects of a given type using an initializer list. The syntax is `(type_name){initializer_list}`. - *[Planned]*
  * Example: `دالة_تأخذ_مصفوفة((عدد_صحيح[]){1, 2, 3}).`
  * Example: `مؤشر_للبنية = &(بنية_مثال){ ::عضو1 = 10, ::عضو2 = "نص" }.` (Assuming `::` for designated initializers as well)

### 2.5 Operators

Operators perform operations on values. See Section 5 for a detailed list and precedence.

* **Member Access:**
  * Direct member access: `::` (e.g., `متغير_بنية::عضو`) - *[Planned]*
  * Indirect (pointer) member access: `->` (e.g., `مؤشر_بنية->عضو`) - *[Planned]*

### 2.6 Separators / Delimiters

Symbols used to structure code: `( ) { } [ ] , . :`

* **`.` (Dot):** Terminates statements. - *[Implemented]*

## 3. Types

Baa has a static type system based on C, with Arabic names for built-in types.

### 3.1 Basic Types

| Arabic Name          | English Equiv.   | Description            | Size   | Status      |
| -------------------- | ---------------- | ---------------------- | ------ | ----------- |
| `عدد_صحيح`           | `int`            | Signed Integer         | 32-bit | Implemented |
| `عدد_صحيح_طويل_جدا`  | `long long int`  | Signed Long Long Integer | 64-bit | Planned     |
| `عدد_حقيقي`          | `float`          | Floating-point (suffix `ح` for literals) | 32-bit | Implemented |
| `حرف`                | `char`           | Character / String     | 16-bit | Implemented |
| `منطقي`              | `bool`           | Boolean (`صحيح`/`خطأ`) | 8-bit? | Implemented |
| `فراغ`               | `void`           | Represents no value    | N/A    | Implemented |

*(Internal representation sizes based on `types.c`)*

### 3.2 Derived Types

| Arabic Name | English Equiv. | Description                    | Status  | Notes                                     |
| ----------- | -------------- | ------------------------------ | ------- | ----------------------------------------- |
| `مصفوفة`    | array          | Ordered collection of elements | Partial | Basic AST/Type support exists.            |
| `مؤشر`      | pointer        | Address of a variable          | Planned | Requires memory model & operator support. |
| `بنية`      | struct         | Collection of named members. Supports C99 flexible array members (e.g., `type last_member[];`) as the last member. Direct member access via `::`, pointer access via `->`. | Planned |                                           |
| `اتحاد`     | union          | Shared memory for members. Direct member access via `::`, pointer access via `->`.      | Planned |                                           |
| `تعداد`     | enum           | Enumeration type. Allows defining a set of named integer constants. | Planned |                                           |

### 3.3 Type Compatibility & Conversion

*(Based on current `types.c` implementation)*

* **General Principle:** Operations (like assignment, arithmetic) require operands of compatible types.
* **Implicit Conversions Allowed:**
  * `عدد_صحيح` -> `عدد_حقيقي` (Integer to Float)
  * `حرف` -> `عدد_صحيح` (Character to Integer)
* **Boolean Conversion:** In conditional contexts (like `if`, `while`), integer or float values might be implicitly converted (0 is false, non-zero is true). `منطقي` type is preferred. *(Needs verification)*.
* **Operators:** Require compatible operands (see Section 5). Results have defined types (e.g., comparison yields `منطقي`).
* **Explicit Casts:** Syntax and implementation for explicit type casting are *[Planned]*.

*[Status: Type definitions exist. Enforcement via semantic analysis is currently limited.]*

## 4. Declarations

Declarations introduce new names (identifiers) into a scope.

### 4.1 Variable Declarations

Variables store values that can potentially change.

* **Syntax:** `type identifier ('=' initializer_expression)? '.'` - *[Implemented]*
* **Initialization:** Optional. If omitted, default value depends on scope (e.g., zero/null for globals/static, potentially uninitialized for locals - *needs clarification*).
  * The `initializer_expression` can use C99-style designated initializers for arrays (e.g., `[index] = value`) and structs/unions (e.g., `::member = value`). - *[Planned]*
* **Constants:** Use the `ثابت` keyword before the type to declare a constant whose value cannot be changed after initialization. Constants *must* be initialized. `ثابت type identifier = initializer_expression '.'` - *[Partial - Keyword parsed, semantic enforcement needed]*.

```baa
عدد_صحيح counter.             // Variable, likely default initialized
عدد_حقيقي price = 10.5.        // Variable, initialized
ثابت منطقي FLAG = صحيح.       // Constant, must be initialized
// price = 11.0.               // OK
// FLAG = خطأ.                // Error: Cannot assign to constant
```

* **Removed:** The `متغير` keyword is no longer planned for variable declarations.

### 4.2 Function Declarations

Functions define reusable blocks of code.

* **Syntax:** `return_type? identifier '(' parameter_list? ')' '{' statement* '}'` (C-style declaration) - *[Implementation Pending]*
* **Return Type:** Specified *before* the function identifier. If omitted, defaults to `فراغ` (void).
* **Parameters (`parameter_list`):** Comma-separated list of `type identifier`. `( )` for no parameters.
* **Entry Point:** The program must contain a function named `رئيسية` with no parameters and typically returning `عدد_صحيح`. `عدد_صحيح رئيسية() { ... إرجع 0. }`

```baa
// Function with no parameters, implicit void return
فراغ print_hello() {
    اطبع("مرحبا!").
}

// Function with parameters and explicit return type
عدد_صحيح أضف(عدد_صحيح a, عدد_صحيح b) {
    إرجع a + b. // Keyword `مستقر` for static functions is planned.
}
```

* **Planned/Partial:** Optional parameters, rest parameters, named arguments (AST support exists, parsing/analysis status unclear).
* **Inline Functions (C99):** Functions can be prefixed with the `مضمن` (`inline`) keyword. This serves as a hint to the compiler to attempt to reduce function call overhead, typically by integrating the function's code directly at the call site. The exact behavior follows C99 semantics for `inline`. - *[Lexer Implemented, Semantic enforcement needed]*

### 4.3 Other Declarations (Planned)

Support for `typedef` (`نوع_مستخدم`), `struct` (`بنية`), `union` (`اتحاد`), pointers (`مؤشر`), and `enum` is planned for closer C compatibility.

### 4.4 Type Qualifiers (C99)

Type qualifiers modify the properties of types. Baa plans to support C99 qualifiers.

* **`مستقر` (`static`):** For static storage duration / internal linkage. - *[Planned for Lexer/Parser]*
* **`ثابت` (`const`):** Indicates that the object's value cannot be changed after initialization. Constants *must* be initialized.
    `ثابت type identifier = initializer_expression '.'` - *[Lexer Implemented, Semantic enforcement needed]*
* **`مقيد` (`restrict`):** Can only be applied to pointers to an object type. It indicates that for the lifetime of the pointer, only that pointer itself or values derived directly from it (such as `pointer + 1`) will be used to access the object it points to. This is a hint for compiler optimizations and does not change the program's semantics if correctly used. - *[Lexer Implemented, Semantic enforcement needed]*
* **`متطاير` (`volatile`):** Indicates that an object may be modified by means not under the control of the compiler (e.g., by hardware or another thread). Accesses to volatile objects should not be optimized away. - *[Planned]*

## 5. Operator Precedence and Associativity

The following table summarizes operator precedence (highest first) and associativity. This determines the order of evaluation in complex expressions (e.g., `a + b * c` is evaluated as `a + (b * c)` because `*` has higher precedence than `+`).

| Precedence Level  | Operators          | Associativity | Notes                    | Status                  |
| ----------------- | ------------------ | ------------- | ------------------------ | ----------------------- |
| Highest (Primary) | `()` `[]` `::` `->` | Left-to-right | Grouping, Index, Member Access | Implemented/Partial(::, -> Planned)  |
| (Postfix)         | `++` `--`          | Left-to-right | Postfix Inc/Dec          | Implemented             |
| (Unary)           | `++` `--`          | Right-to-left | Prefix Inc/Dec           | Implemented             |
|                   | `+` `-` (unary)    | Right-to-left | Unary Plus/Minus         | Implemented             |
|                   | `!` `~`            | Right-to-left | Logical NOT, Bitwise NOT | Implemented/Partial (~) |
| (Factor)          | `*` `/` `%`        | Left-to-right | Multiplicative           | Implemented             |
| (Term)            | `+` `-` (binary)   | Left-to-right | Additive                 | Implemented             |
| (Shift)           | `<<` `>>`          | Left-to-right | Bitwise Shift            | Partial                 |
| (Comparison)      | `<` `>` `<=` `>=`  | Left-to-right | Relational               | Implemented             |
| (Equality)        | `==` `!=`          | Left-to-right | Equality                 | Implemented             |
| (Bitwise AND)     | `&`                | Left-to-right | Bitwise AND              | Partial                 |
| (Bitwise XOR)     | `^`                | Left-to-right | Bitwise XOR              | Partial                 |
| (Bitwise OR)      | `|`                | Left-to-right | Bitwise OR               | Partial                 |
| (Logical AND)     | `&&`               | Left-to-right | Logical AND              | Implemented             |
| (Logical OR)      | `||`               | Left-to-right | Logical OR               | Implemented             |
| Lowest(Assign)    | `=` `+=` `-=` etc. | Right-to-left | Assignment               | Implemented             |

*(Status indicates parsing support based on `expression_parser.c` and `operators.h`. Semantic validation status for operators varies - see Section 3.3 and `c_comparison.md`)*.

*(Note: Pointer dereference `*`/address-of `&` depend on planned pointer features).*

## 6. Statements

Statements are the units of execution. They are terminated by a dot (`.`).

### 6.1 Expression Statements

An expression followed by a dot is a statement. The expression is evaluated for its side effects.

```baa
counter++.             // Increment counter
add(5, 3).             // Call function (result discarded)
x = y + 1.             // Assignment
```

### 6.2 Block Statements (Compound Statements)

A sequence of zero or more statements enclosed in curly braces `{}`. Defines a block scope.
In line with C99, declarations can be mixed with statements within a block, but an identifier must be declared before its first use in that scope. - *[Planned]*

```baa
{
    عدد_صحيح temp = x.
    x = y.
    y = temp.
}
```

### 6.3 Conditional Statements (`if`/`else`)

Executes statements based on a condition.

* **Syntax:** `إذا '(' expression ')' statement_or_block ( وإلا statement_or_block )?` - *[Implemented]*
* **Condition:** The `expression` is evaluated. It should result in a `منطقي` value (or be convertible to one).

```baa
إذا (x > 0) {
    اطبع("Positive").
}

إذا (y == 0)
    اطبع("Zero").
وإلا {
    اطبع("Non-zero").
}
```

### 6.4 Iteration Statements (Loops)

#### 6.4.1 `while` Loop

Repeats a statement/block as long as a condition is true.

* **Syntax:** `طالما '(' expression ')' statement_or_block` - *[Implemented]*
* **Condition:** Evaluated *before* each iteration.

```baa
عدد_صحيح i = 0.
طالما (i < 5) {
    اطبع(i).
    i++.
}
```

#### 6.4.2 `for` Loop

Provides initialization, condition, and post-iteration expressions for controlled looping.

* **Syntax:** `لكل '(' init_expr? ';' condition_expr? ';' incr_expr? ')' statement_or_block` - *[Implemented - Note: Uses semicolons ';', not dots '.' as separators inside parentheses]*.
* **Components:**
  * `init_expr`: Evaluated once before the loop. This can be an expression or a C99-style declaration (e.g., `عدد_صحيح i = 0`). If it's a declaration, the scope of the declared variable(s) is limited to the loop. - *[Planned for declaration support]*
  * `condition_expr`: Evaluated before each iteration. Loop continues if true.
  * `incr_expr`: Evaluated after each iteration.

```baa
// Example - Note the use of semicolons inside the parentheses
لكل (عدد_صحيح i = 0; i < 10; i++) {
    اطبع(i).
}
```

#### 6.4.3 `do-while` Loop (Planned)

Repeats a statement/block, evaluating the condition *after* the first iteration.

* **Syntax:** `افعل statement_or_block طالما '(' expression ')' '.'` - *[Syntax Defined, Implementation Pending]*.

### 6.5 Jump Statements

Transfer control flow unconditionally.

#### 6.5.1 `return` Statement

Exits the current function, optionally returning a value.

* **Syntax:** `إرجع (expression)? '.'` - *[Implemented]*
* **Value:** If the function returns non-void, the `expression` type must be compatible with the declared return type.

#### 6.5.2 `break` Statement

Exits the innermost enclosing loop (`while`, `for`, `do`) or `switch` statement.

* **Syntax:** `توقف '.'` - *[Implemented]*

#### 6.5.3 `continue` Statement

Skips the remainder of the current iteration of the innermost enclosing loop (`while`, `for`, `do`) and proceeds to the next iteration (evaluating the condition/increment).

* **Syntax:** `استمر '.'` - *[Implemented]*

#### 6.5.4 `goto` Statement (Planned)

Unconditional jump to a labeled statement within the same function.

* **Syntax:** `اذهب identifier '.'` and `identifier ':' statement` - *[Planned]*.

### 6.6 `switch` Statement

Selects a block of code to execute based on the value of an expression.

* **Syntax:** `اختر '(' expression ')' '{' case_group* '}'` - *[Implemented]*.
* **`case_group`:** Consists of one or more `حالة constant_expression ':'` labels or a `افتراضي ':'` (default) label, followed by statements. Execution falls through cases unless `توقف` is used.

```baa
// Example - Syntax/Semantics need verification
اختر (day) {
    حالة 1:
        اطبع("Monday").
        توقف.
    حالة 2:
        اطبع("Tuesday").
        توقف.
    افتراضي:
        اطبع("Other day").
}

```

## 7. Scope Rules

Baa uses lexical scoping, similar to C. The scope determines the visibility of identifiers (variables, functions, etc.).

* **Global Scope:** Declarations made outside any function are generally visible from their point of declaration to the end of the source file. *[Note: Visibility across multiple files via `خارجي` is planned but likely requires linker/codegen support].*
* **Function Scope:** Parameters and variables declared directly inside a function body (outside any nested blocks) are visible from their point of declaration to the end of the function.
* **Block Scope:** Variables declared inside a block (code enclosed in `{...}`, e.g., within an `if`, `while`, or compound statement) are visible only from their point of declaration to the end of that block (including nested blocks).
* **Shadowing:** A declaration in an inner scope can *shadow* (hide) a declaration with the same name from an outer scope. The inner declaration takes precedence within its scope.

```baa
عدد_صحيح global_var = 10. // Global scope

test(عدد_صحيح param) { // Function scope starts
    عدد_صحيح func_var = param + global_var. // Can access global and param

    إذا (func_var > 20) { // Block scope starts
        عدد_صحيح block_var = 5.
        عدد_صحيح param = 0. // Shadows the function parameter 'param'
        اطبع(block_var + param). // Accesses block_var and the *shadowing* param (0)
    } // Block scope ends

    // اطبع(block_var). // Error: block_var not visible here

    اطبع(param). // Accesses the *original* function parameter 'param'
} // Function scope ends

```

*[Status: Basic block scoping is likely handled by the parser structure. Full enforcement and symbol table management via `scope.h` happens during semantic analysis, which is currently underdeveloped.]*

## 8. Program Structure

### 8.1 Basic Program Structure

Every executable program must have a main function (`رئيسية`) which serves as the entry point.

```baa
// مثال برنامج بسيط بلغة باء
عدد_صحيح رئيسية() {
    اطبع("مرحباً بالعالم!").
    إرجع 0. // Conventionally return 0 on success
}
```

### 8.2 File Extension

Baa source code files typically use the `.ب` extension.

```Baa
  برنامج.ب
  my_module.ب
```

## 9. Naming Conventions

(These are conventions, not strictly enforced by the compiler beyond identifier rules)

* Use clear and descriptive names in Arabic or English.
* Follow consistent casing (e.g., `snake_case` or `camelCase`).
* Avoid overly short or ambiguous names.
