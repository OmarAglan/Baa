# Baa Language Specification

> **Version:** 0.3.2.6.4 | [← User Guide](USER_GUIDE.md) | [Compiler Internals →](INTERNALS.md)

Baa (باء) is a compiled systems programming language using Arabic syntax. It compiles directly to native machine code via Assembly/GCC on Windows.

---

## Table of Contents

- [Program Structure](#1-program-structure)
- [Preprocessor](#2-preprocessor)
- [Variables & Types](#3-variables--types)
- [Constants](#4-constants)
- [Functions](#5-functions)
- [Input / Output](#6-input--output)
- [Control Flow](#7-control-flow)
- [Operators](#8-operators)
- [Complete Example](#9-complete-example)

---

## 1. Program Structure

A Baa program is a collection of **Global Variables** and **Functions**.

| Aspect | Description |
|--------|-------------|
| **File Format** | UTF-8 encoded, `.baa` extension |
| **Entry Point** | `الرئيسية` (Main) function |
| **Statements** | End with period (`.`) |
| **Comments** | Single-line with `//` |

### Minimal Program

```baa
صحيح الرئيسية() {
    إرجع ٠.
}
```

---

## 2. Preprocessor (المعالج القبلي)

The preprocessor handles directives before the code is compiled. All directives start with `#`.

### 2.1. Include Directive (`#تضمين`)

Include other files (headers) into the current file. This works like C's `#include`.

**Syntax:** `#تضمين "file.baahd"`

**Example:**

```baa
#تضمين "math.baahd"
// This includes the header file "math.baahd"
```

### 2.2. Definitions (`#تعريف`)

Define compile-time constants (macros). The compiler replaces the identifier with the specified value wherever it appears in the code.

**Syntax:** `#تعريف <name> <value>`

**Example:**

```baa
#تعريف حد_أقصى ١٠٠
#تعريف رسالة "مرحباً"

صحيح الرئيسية() {
    // سيتم استبدال 'حد_أقصى' بـ ١٠٠
    صحيح س = حد_أقصى.
    اطبع رسالة.
    إرجع ٠.
}
```

### 2.3. Conditional Compilation (`#إذا_عرف`)

Include or exclude blocks of code based on whether a symbol is defined.

**Syntax:**

```baa
#إذا_عرف <name>
    // Compiled if <name> is defined
#وإلا
    // Compiled if <name> is NOT defined
#نهاية
```

**Example:**

```baa
#تعريف تصحيح 1

#إذا_عرف تصحيح
    اطبع "Debug mode enabled".
#وإلا
    اطبع "Release mode".
#نهاية
```

### 2.4. Undefine Directive (`#الغاء_تعريف`)

Remove a previously defined macro.

**Syntax:** `#الغاء_تعريف <name>`

**Example:**

```baa
// Use تصحيح macro...
#تعريف تصحيح ١
#الغاء_تعريف تصحيح
// Now 'تصحيح' is undefined
```

---

## 3. Variables & Types

Baa is statically typed. All variables must be declared with their type.

### 3.1. Supported Types

| Baa Type | C Equivalent | Description | Example |
|----------|--------------|-------------|---------|
| `صحيح` | `int64_t` (stored) | Integer value (stored as 8 bytes) | `صحيح س = ٥.` |
| `نص` | `char*` | String pointer (Reference) | `نص اسم = "باء".` |
| `منطقي` | `bool` (stored as int) | Boolean value (`صواب`/`خطأ`, stored as 1/0) | `منطقي ب = صواب.` |

### 3.2. Integer Sizes (أحجام الأعداد الصحيحة) [Scheduled v0.3.5.5]

Baa supports explicit integer sizes for systems programming:

| Type | Description | Range (Approx) |
|------|-------------|----------------|
| `ص٨` | Signed 8-bit | -128 to 127 |
| `ص١٦` | Signed 16-bit | -32,768 to 32,767 |
| `ص٣٢` | Signed 32-bit | -2 Billion to +2 Billion |
| `ص٦٤` | Signed 64-bit | Large (Default `صحيح`) |
| `ط٨` | Unsigned 8-bit | 0 to 255 |
| `ط١٦` | Unsigned 16-bit | 0 to 65,535 |
| `ط٣٢` | Unsigned 32-bit | 0 to 4 Billion |
| `ط٦٤` | Unsigned 64-bit | Huge (Default `طبيعي`) |

### 3.3. Type Aliases (أسماء الأنواع البديلة) [Scheduled v0.3.6.5]

Create custom names for existing types using `نوع`.

**Syntax:** `نوع <new_name> = <existing_type>.`

```baa
نوع معرف = ط٦٤.
نوع كود = ص٣٢.

معرف ر = ١٠٠.
كود خ = -١.
```

### 3.4. Union Types (الاتحادات) [Scheduled v0.3.4.5]

Unions allow different data types to share the same memory location.

**Syntax:**
```baa
اتحاد بيانات {
    صحيح رقم.
    نص نص_قيمة.
}

اتحاد بيانات د.
د:رقم = ١٠.
```

### 3.5. Static Local Variables (متغيرات ساكنة) [Scheduled v0.3.7.5]

Variables inside a function that persist across calls.

**Syntax:** `ساكن <type> <name> = <value>.`

```baa
صحيح عداد() {
    ساكن صحيح ع = ٠.
    ع = ع + ١.
    إرجع ع.
}
```

### 3.6. Type Casting (تحويل الأنواع) [Scheduled v0.3.10.5]

Explicitly convert between types using `كـ` (As).

**Syntax:** `كـ<type>(expression)`

```baa
صحيح س = ٦٥.
حرف ح = كـ<حرف>(س).
```

**Character literals:** Baa supports character literals like `'أ'`, but there is currently no dedicated `حرف` type keyword; character literals behave like integers in expressions and code generation.

### 3.2. Scalar Variables

**Syntax:** `<type> <identifier> = <expression>.`

```baa
// Integer
صحيح س = ٥٠.
س = ١٠٠.

// String (Pointer to text)
نص رسالة = "مرحباً".
رسالة = "وداعاً".
```

### 3.3. Arrays

Fixed-size arrays allocated on the stack.

**Syntax:** `صحيح <identifier>[<size>].`

```baa
// تعريف مصفوفة من ٥ عناصر
صحيح قائمة[٥].

// تعيين قيمة (الفهرس يبدأ من ٠)
قائمة[٠] = ١٠.
قائمة[١] = ٢٠.

// قراءة قيمة
صحيح أول = قائمة[٠].

// استخدام متغير كفهرس
صحيح س = ٢.
قائمة[س] = ٣٠.
```

---

## 4. Constants (الثوابت)

Constants are immutable variables that cannot be reassigned after initialization.

### 4.1. Constant Declaration

Use the `ثابت` keyword before the type to declare a constant.

**Syntax:** `ثابت <type> <identifier> = <expression>.`

```baa
// Global constant
ثابت صحيح الحد_الأقصى = ١٠٠.

صحيح الرئيسية() {
    // Local constant
    ثابت صحيح المعامل = ٥.
    
    اطبع الحد_الأقصى.  // ✓ OK: Reading constant
    اطبع المعامل.      // ✓ OK: Reading constant
    
    // الحد_الأقصى = ٢٠٠.  // ✗ Error: Cannot reassign constant
    
    إرجع ٠.
}
```

### 4.2. Constant Arrays

Arrays can also be declared as constants to prevent modification of their elements.

**Syntax:** `ثابت صحيح <identifier>[<size>].`

```baa
صحيح الرئيسية() {
    ثابت صحيح أرقام[٣].
    
    // أرقام[٠] = ١٠.  // ✗ Error: Cannot modify constant array
    
    إرجع ٠.
}
```

### 4.3. Rules for Constants

| Rule | Description |
|------|-------------|
| **Must be initialized** | Constants require an initial value at declaration |
| **Cannot be reassigned** | Attempting to reassign produces a semantic error |
| **Array elements immutable** | Elements of constant arrays cannot be modified |
| **Functions cannot be const** | The `ثابت` keyword applies only to variables |

---

## 5. Functions

Functions enable code reuse and modularity.

### 5.1. Definition

**Syntax:** `<type> <name>(<parameters>) { <body> }`

```baa
// Function that adds two integers
صحيح جمع(صحيح أ, صحيح ب) {
    إرجع أ + ب.
}

// Function that squares an integer
صحيح مربع(صحيح س) {
    إرجع س * س.
}
```

### 5.2. Function Prototypes (النماذج الأولية)

To use a function defined in another file (or later in the same file), you can declare its prototype without a body.

**Syntax:** `<type> <name>(<parameters>).` ← Note the dot at the end!

```baa
// Prototype declaration (notice the dot at the end)
صحيح جمع(صحيح أ, صحيح ب).

صحيح الرئيسية() {
    // Calls 'جمع' defined in another file
    اطبع جمع(١٠, ٢٠).
    إرجع ٠.
}
```

### 5.3. Calling

**Syntax:** `<name>(<arguments>)`

```baa
صحيح الناتج = جمع(١٠, ٢٠).   // الناتج = ٣٠
صحيح م = مربع(٥).            // م = ٢٥
```

### 5.4. Entry Point (`الرئيسية`)

Every program **must** have a main function:

```baa
صحيح الرئيسية() {
    // Program code here
    إرجع ٠.  // 0 means success
}
```

**Important:** The entry point **must** be named `الرئيسية` (ar-ra'īsīyah). It is exported as `main` in the generated assembly.

**Command Line Arguments [Scheduled v0.3.12.5]:**
The main function can optionally accept arguments:

```baa
صحيح الرئيسية(صحيح عدد، نص[] معاملات) {
    إذا (عدد > ١) {
        اطبع معاملات[١].
    }
    إرجع ٠.
}
```

### 5.6. Function Pointers (مؤشرات الدوال) [Scheduled v0.3.10.6]

Pass functions as values.

**Syntax:** `دالة(types) -> return_type`

```baa
نوع عملية = دالة(صحيح، صحيح) -> صحيح.

صحيح جمع(صحيح أ، صحيح ب) { إرجع أ + ب. }

صحيح الرئيسية() {
    عملية ع = جمع.
    اطبع ع(١، ٢).
    إرجع ٠.
}
```

### 5.7. Variadic Functions (دوال متغيرة المعاملات) [Scheduled v0.4.0.5]

Functions that accept any number of arguments using `...`.

```baa
عدم سجل(نص رسالة، ...) {
    // Implementation
}
```

### 5.5. Recursion (التكرار)

Functions can call themselves (recursion), provided there is a base case to terminate the loop.

```baa
// حساب متتالية فيبوناتشي
صحيح فيبوناتشي(صحيح ن) {
    إذا (ن <= ١) {
        إرجع ن.
    }
    إرجع فيبوناتشي(ن - ١) + فيبوناتشي(ن - ٢).
}
```

---

## 6. Input / Output

### 6.1. Print Statement (`اطبع`)

**Syntax:** `اطبع <expression>.`

Prints a value followed by a newline.

**Current implementation notes (v0.2.9):**

- `صحيح` and `منطقي` values are printed using C `printf("%d\n", ...)` (so output is effectively 32-bit).
- `نص` values are printed using `printf("%s\n", ...)`.
- Character literals like `'أ'` are treated as integer values during code generation and print as their numeric code.

```baa
اطبع "مرحباً بالعالم".    // طباعة نص
اطبع ١٠٠.                 // طباعة رقم
اطبع 'أ'.                 // طباعة حرف

// طباعة متغيرات
نص اسم = "علي".
اطبع اسم.
```

### 6.2. Input Statement (`اقرأ`)

**Syntax:** `اقرأ <variable>.`

Reads an integer from standard input and stores it in the specified variable.

**Current implementation notes (v0.2.9):**

- Input uses C `scanf("%d", ...)` and is intended for `صحيح` variables.

```baa
صحيح العمر = ٠.
اطبع "كم عمرك؟ ".
اقرأ العمر.

اطبع "عمرك هو: ".
اطبع العمر.
```

---

## 7. Control Flow

### 7.1. Conditional (`إذا` / `وإلا`)

Executes a block based on conditions.

**Syntax:**

```baa
إذا (<condition>) {
    // ...
} وإلا إذا (<condition>) {
    // ...
} وإلا {
    // ...
}
```

**Example:**

```baa
صحيح س = ١٥.

إذا (س > ٢٠) {
    اطبع "كبير جداً".
} وإلا إذا (س > ١٠) {
    اطبع "متوسط".
} وإلا {
    اطبع "صغير".
}
```

### 7.2. While Loop (`طالما`)

Repeats a block while the condition is true.

**Syntax:** `طالما (<condition>) { <body> }`

```baa
صحيح س = ٥.

طالما (س > ٠) {
    اطبع س.
    س = س - ١.
}
// يطبع: ٥ ٤ ٣ ٢ ١
```

### 7.3. For Loop (`لكل`)

C-style loop using **Arabic semicolon `؛`** as separator (NOT regular semicolon).

**Syntax:** `لكل (<init>؛ <condition>؛ <increment>) { <body> }`

```baa
// طباعة الأرقام من ٠ إلى ٩
لكل (صحيح س = ٠؛ س < ١٠؛ س++) {
    اطبع س.
}
```

### 7.4. Loop Control (`توقف` & `استمر`)

- **`توقف` (Break)**: Exits the loop immediately.
- **`استمر` (Continue)**: Skips the rest of the current iteration and proceeds to the next one.

**Example:**

```baa
صحيح الرئيسية() {
    لكل (صحيح س = ٠؛ س < ١٠؛ س++) {
        // تخطي الرقم ٥
        إذا (س == ٥) {
            استمر.
        }
        
        // الخروج عند الوصول للرقم ٨
        إذا (س == ٨) {
            توقف.
        }
        
        اطبع س.
    }
    // الناتج: ٠ ١ ٢ ٣ ٤ ٦ ٧
    إرجع ٠.
}
```

### 7.5. Switch Statement (`اختر`)

Multi-way branching based on integer or character values.

- **`اختر` (Switch)**: Starts the statement.
- **`حالة` (Case)**: Defines a value to match.
- **`افتراضي` (Default)**: Defines the code to run if no case matches.
- **`:` (Colon)**: Separator after the case value.

**Syntax:**

```baa
اختر (<expression>) {
    حالة <value>:
        // code...
        توقف.
    حالة <value>:
        // code...
        توقف.
    افتراضي:
        // code...
        توقف.
}
```

**Note:** Just like in C, execution "falls through" to the next case unless you explicitly use `توقف` (break).

**Example:**

```baa
صحيح س = ٢.

اختر (س) {
    حالة ١:
        اطبع "واحد".
        توقف.
    حالة ٢:
        اطبع "اثنان".
        توقف.
    افتراضي:
        اطبع "رقم آخر".
        توقف.
}
```

---

## 8. Operators

### 8.1. Arithmetic

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `٥ + ٣` → `٨` |
| `-` | Subtraction | `٥ - ٣` → `٢` |
| `*` | Multiplication | `٥ * ٣` → `١٥` |
| `/` | Division | `١٠ / ٢` → `٥` |
| `%` | Modulo | `١٠ % ٣` → `١` |
| `++` | Increment (postfix) | `س++` |
| `--` | Decrement (postfix) | `س--` |
| `-` | Negative (unary) | `-٥` |

### 8.2. Comparison

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `س == ٥` |
| `!=` | Not equal | `س != ٥` |
| `<` | Less than | `س < ١٠` |
| `>` | Greater than | `س > ١٠` |
| `<=` | Less or equal | `س <= ١٠` |
| `>=` | Greater or equal | `س >= ١٠` |

### 8.3. Logical

| Operator | Description | Behavior |
|----------|-------------|----------|
| `&&` | AND | Short-circuit: stops if left is false |
| `\|\|` | OR | Short-circuit: stops if left is true |
| `!` | NOT | Inverts truth value |

**Short-circuit Evaluation:** `&&` stops if left is false; `||` stops if left is true.

```baa
// Short-circuit example
إذا (س > ٠ && س < ١٠) {
    اطبع "س بين ١ و ٩".
}

إذا (!خطأ) {
    اطبع "لا يوجد خطأ".
}
```

### 8.4. Operator Precedence

From highest to lowest:

1. `()` — Parentheses
2. `!`, `-` (unary), `++`, `--` — Unary operators
3. `*`, `/`, `%` — Multiplication, Division, Modulo
4. `+`, `-` — Addition, Subtraction
5. `<`, `>`, `<=`, `>=` — Relational
6. `==`, `!=` — Equality
7. `&&` — Logical AND
8. `||` — Logical OR

**Note:** Use parentheses `()` to override precedence when needed.

---

## 10. Inline Assembly (المجمع المدمج) [Scheduled v0.4.0.6]

Embed raw assembly code directly.

**Syntax:** `مجمع { strings }`

```baa
مجمع {
    "nop"
}
```

---

## 9. Complete Example

```baa
// استخدام الثوابت والماكرو
#تعريف الحد_الأقصى ١٠

// ثابت عام
ثابت صحيح المعامل = ٢.

// Main function
صحيح الرئيسية() {
    // ثابت محلي
    ثابت صحيح البداية = ١.
    
    // طباعة الأرقام المضاعفة
    لكل (صحيح س = البداية؛ س <= الحد_الأقصى؛ س++) {
        اطبع س * المعامل.
    }
    إرجع ٠.
}
```

---

*[← User Guide](USER_GUIDE.md) | [Compiler Internals →](INTERNALS.md)*
